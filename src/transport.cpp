//
// Created by Besogonov, Aleksei on 1/22/16.
//

#include "transport.h"
#include <libusb.h>
#include <assert.h>
#include "aa_helpers.h"

static const int GOOGLE_VENDOR_ID = 0x18d1;
static const int GOOGLE_ACCESSORY_PID = 0x2d00;
static const int DEFAULT_TIMEOUT_MS = 1000;

// OAP Control requests
enum oap_control_req {
    ACC_REQ_GET_PROTOCOL = 51,
    ACC_REQ_SEND_STRING = 52,
    ACC_REQ_START = 53
};

enum acc_control_indexes {
    ACC_IDX_MAN = 0,
    ACC_IDX_MOD = 1
};

enum aa_packet_flags {
    AA_ENCRYPTED = 0b1000,
    AA_LAST_FRAG = 0b0010,
    AA_FIRST_FRAG = 0b0001,
    AA_CONTROL_FLAG = 0b0100,
};

static const char *ACC_MANUFACTURER = "Android";
static const char *ACC_MODEL = "Android Auto";


usb_context_ptr_t get_usb_lib()
{
    std::shared_ptr<libusb_context> usb_ctx;
    libusb_context *ctx;
    libusb_init(&ctx); //initialize a library session
    if (debug_stream_t::get_debug_level() == TRACE_OUTPUT)
        libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_DEBUG);
    usb_ctx.reset(ctx, [](libusb_context* p){libusb_exit(p);});
    return usb_ctx;
}

device_ptr_t enumerate_devices(const usb_context_ptr_t &ctx)
{
    libusb_device **devices = nullptr;
    ssize_t cnt = libusb_get_device_list(ctx.get(), &devices);
    ON_BLOCK_EXIT([=]{libusb_free_device_list(devices, 1);});

    if(cnt < 0)
        throw std::runtime_error("Failed to enumerate USB devices");

    for(int f=0;f<cnt; ++f)
    {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devices[f], &desc) < 0)
            continue;

        libusb_device_handle *hndl;
        if (libusb_open(devices[f], &hndl) < 0) {
            TA_TRACE() << "Failed to open vid=" << desc.idVendor
                        << ", pid=" << desc.idProduct;
            continue;
        }

        device_ptr_t dev(hndl, [](libusb_device_handle *d) {libusb_close(d);});
        // Found our accessory device!
        if (desc.idVendor == GOOGLE_VENDOR_ID && desc.idProduct == GOOGLE_ACCESSORY_PID)
            return dev;

        // Try to switch device into the accessory mode
        u_char buf[512];
        int res = libusb_control_transfer(dev.get(), LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_VENDOR,
            ACC_REQ_GET_PROTOCOL, 0, 0, buf, 512, DEFAULT_TIMEOUT_MS);
        if (res != 2) {
            TA_TRACE() << "Failed to send control ouput to vid=" << desc.idVendor
                    << ", pid=" << desc.idProduct;
            continue;
        }

        TA_DEBUG() << "Found device vid=" << desc.idVendor
                      << ", pid=" << desc.idProduct << " supporting ACC version "
                      << (int)buf[0] << "." << (int)buf[1];

        res = libusb_control_transfer(dev.get(), LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_VENDOR,
                                      ACC_REQ_SEND_STRING, 0, ACC_IDX_MAN,
                                      (u_char *)ACC_MANUFACTURER,
                                      (uint16_t) strlen(ACC_MANUFACTURER)+1, DEFAULT_TIMEOUT_MS);
        if (res != strlen(ACC_MANUFACTURER)+1)
        {
            TA_DEBUG() << "Device vid=" << desc.idVendor
                << ", pid=" << desc.idProduct << " couldn't handle ACC request";
            continue;
        }

        res = libusb_control_transfer(dev.get(), LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_VENDOR,
                                      ACC_REQ_SEND_STRING, 0, ACC_IDX_MOD,
                                      (u_char *)ACC_MODEL,
                                      (uint16_t) strlen(ACC_MODEL)+1, DEFAULT_TIMEOUT_MS);
        if (res != strlen(ACC_MODEL)+1)
        {
            TA_DEBUG() << "Device vid=" << desc.idVendor
                << ", pid=" << desc.idProduct << " couldn't handle ACC request";
            continue;
        }

        res = libusb_control_transfer(dev.get(), LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_VENDOR,
                                      ACC_REQ_START, 0, 0,
                                      NULL, 0, DEFAULT_TIMEOUT_MS);
        if (res != 0)
        {
            TA_DEBUG() << "Device vid=" << desc.idVendor
                << ", pid=" << desc.idProduct << " failed to start ACC";
        }

        //Don't do anything, device will re-enumerate itself and appear later
    }

    return device_ptr_t();
}

transport_ptr_t find_usb_transport(const usb_context_ptr_t &ctx, const notifier_t *notifier)
{
    device_ptr_t dev;
    TA_INFO() << "Enumerating USB devices";
    while (!dev)
    {
        dev = enumerate_devices(ctx);
        if (!dev || libusb_reset_device(dev.get()) != LIBUSB_SUCCESS)
            notifier->sleep(1000);
        TA_TRACE() << "ACC device not found, retrying";
    }
    TA_INFO() << "Found an ACC device.";
    // Always detach the kernel first
    libusb_set_auto_detach_kernel_driver(dev.get(), 1);

    return transport_ptr_t(new transport_t(ctx, dev, notifier));
}

std::ostream &transport_t::operator<<(std::ostream &ost) {
    return ost;
}

transport_t::transport_t(const usb_context_ptr_t &ctx_, const device_ptr_t &dev,
                         const notifier_t *terminator) :
        ctx_(ctx_), dev_(dev), terminator_(terminator), claimed_interface_(false),
        writer_termination_requested_(false), cur_consumed_(), read_window_size_()
{
    stream_buffer_.resize(stream_buffer_size_);

    std::shared_ptr<libusb_device> dev_info(libusb_get_device(dev_.get()),
        [](libusb_device* d){libusb_unref_device(d);});
    assert(dev_info);

    TA_TRACE() << "Retrieving USB configuration";
    libusb_config_descriptor *config = 0;
    //We're always using the first configuration
    if (libusb_get_config_descriptor(dev_info.get(), 0, &config) != 0)
        throw std::runtime_error("Failed to get USB configuration");
    ON_BLOCK_EXIT([=]{libusb_free_config_descriptor(config);});

    uint8_t interface_id=0, endpoint_in=0, endpoint_out=0;
    bool endpoint_in_found = false, endpoint_out_found = false;
    int alt_setting_id=-1;

    for(interface_id=0; interface_id<config->bNumInterfaces; ++interface_id) {
        const libusb_interface *inter = &config->interface[interface_id];
        for(int n=0; n<inter->num_altsetting; ++n) {
            endpoint_in_found = false;
            endpoint_out_found = false;

            const libusb_interface_descriptor *interdesc = &inter->altsetting[n];
            alt_setting_id = interdesc->bAlternateSetting;
            for(int k=0; k<(int)interdesc->bNumEndpoints; k++) {
                const libusb_endpoint_descriptor *epdesc = &interdesc->endpoint[k];
                if ((epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK)
                    continue;

                int dir = epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK;
                if (dir == LIBUSB_ENDPOINT_IN) {
                    endpoint_in = epdesc->bEndpointAddress;
                    endpoint_in_found = true;
                    //The read buffer size must be a multiply of wMaxPacketSize
                    //to avoid buffer overruns.
                    read_window_size_ = (stream_buffer_size_ - AA_PACKET_HEADER_SIZE);
                    read_window_size_ -= (read_window_size_ % epdesc->wMaxPacketSize);
                } else {
                    endpoint_out = epdesc->bEndpointAddress;
                    endpoint_out_found = true;
                }
            }

            if (endpoint_in_found && endpoint_out_found)
                break;
        }

        if (endpoint_in_found && endpoint_out_found)
            break;
    }

    if (!endpoint_out || !endpoint_in)
        throw std::runtime_error("Failed to find bulk in/out endpoints on the ACC device");

    TA_DEBUG() << "Found bulk endpoints: "<< (int)endpoint_in << " and " << (int)endpoint_out
        << ", config " << alt_setting_id << ", interface " << (int)interface_id;

    int ret = -1;
    for(int n = 0; n<10; ++n) {
        ret = libusb_claim_interface(dev.get(), interface_id);
        if (ret == 0)
            break;
        TA_DEBUG() << "Failed to claim USB interface, try " << n;
        terminator_->sleep(200);
    }
    if (ret != 0)
        throw std::runtime_error(
                str_out_t() << "Failed to claim USB interface: " << libusb_error_name(ret));

    TA_DEBUG() << "Claimed USB interface";

    ret = libusb_set_interface_alt_setting(dev.get(), interface_id, alt_setting_id);
    if (ret != 0)
        throw std::runtime_error(str_out_t() << "Failed to select configuration " << alt_setting_id);

    TA_INFO() << "USB interface ready";

    assert(read_window_size_);

    this->claimed_interface_ = true;
    this->endpoint_in_ = endpoint_in;
    this->endpoint_out_ = endpoint_out;
    this->interface_id_ = interface_id;

    this->writer_thread_ = std::thread([](transport_t *t){t->writer_loop();}, this);
}

transport_t::~transport_t() {
    //Terminate the writer thread
    {
        std::unique_lock<std::mutex> l(this->queue_mutex_);
        this->writer_termination_requested_ = true;
        this->have_pending_.notify_all();
    }
    if (this->writer_thread_.joinable())
        this->writer_thread_.join();

    if (claimed_interface_)
        libusb_release_interface(this->dev_.get(), interface_id_); //No error checking
}

bool transport_t::read_more(unsigned int timeout_millis)
{
    {
        std::unique_lock<std::mutex> l(this->queue_mutex_);
        if (!stored_exception_.empty())
            throw std::runtime_error(stored_exception_);
    }

    int read = 0;
    int remaining_len = safe_cast<int>(std::min(read_window_size_,
                                                stream_buffer_.size() - cur_consumed_));
    int res = libusb_bulk_transfer(this->dev_.get(), endpoint_in_,
                                   &stream_buffer_.at(cur_consumed_),
                                   remaining_len,
                                   &read, timeout_millis);
    if (res == LIBUSB_ERROR_TIMEOUT) {
        this->terminator_->check_termination();
        return false;
    }
    if (res < 0)
        throw std::runtime_error(libusb_error_name(res));

    cur_consumed_ += read;
    return true;
}

packet_ptr_t transport_t::handle_events(unsigned int timeout_millis) {

    //Check if we have a header
    if (cur_consumed_ < AA_PACKET_HEADER_SIZE) {
        read_more(timeout_millis);
        if (cur_consumed_ < AA_PACKET_HEADER_SIZE)
            return packet_ptr_t();
    }

    // Read the header
    u_char chan = stream_buffer_.at(0);
    u_char flags = stream_buffer_.at(1);
    uint16_t packet_size = (uint16_t) (stream_buffer_.at(2) * 256 + stream_buffer_.at(3));
    //Check if we need more data for this packet
    if (cur_consumed_ < packet_size+4) {
        read_more(timeout_millis);
        return packet_ptr_t();
    }

    packet_ptr_t cur_packet;
    size_t header_size = 4;

    // This is the first packet of multi-packet series
    if ((flags & AA_LAST_FRAG) == 0 && (flags & AA_FIRST_FRAG))
    {
        if (this->mutli_packet_)
            throw std::runtime_error("Interleaved multi-packet: first fragment");

        packet_ptr_t multi_first(new packet_t());
        multi_first->chan_ = chan;
        multi_first->encrypted_ = flags & AA_ENCRYPTED;
        multi_first->control_ = flags & AA_CONTROL_FLAG;

        uint32_t full_size = safe_cast<uint32_t>(stream_buffer_.at(4)*(256*256*256) +
                stream_buffer_.at(5)*(256*256) + stream_buffer_.at(6)*256 + stream_buffer_.at(7));

        //Insert the initial content
        multi_first->content_.reserve(full_size);
        this->mutli_packet_ = multi_first;
        cur_packet = multi_first;
        header_size = 8;
    } else if (this->mutli_packet_)
    {
        cur_packet = this->mutli_packet_;
        if (chan != cur_packet->chan_)
            throw std::runtime_error("Interleaved packet stream: middle fragment");
    } else {
        cur_packet = packet_ptr_t(new packet_t());
        cur_packet->chan_ = chan;
        cur_packet->encrypted_ = flags & AA_ENCRYPTED;
        cur_packet->control_ = flags & AA_CONTROL_FLAG;
        cur_packet->content_.reserve(packet_size);
    }

    cur_packet->content_.insert(cur_packet->content_.end(),
                                stream_buffer_.begin()+header_size,
                                stream_buffer_.begin()+header_size+packet_size);
    std::copy(stream_buffer_.begin()+packet_size+header_size, stream_buffer_.end(),
              stream_buffer_.begin());
    cur_consumed_ -= packet_size+header_size;

    if (flags & AA_LAST_FRAG) {
        this->mutli_packet_ = packet_ptr_t();
        return cur_packet;
    }

    return packet_ptr_t();
}

void transport_t::write_packet(packet_ptr_t packet) {
    if (packet->content_.size()-4 > 65536)
        throw std::out_of_range("USB packet out of range");
    std::unique_lock<std::mutex> l(this->queue_mutex_);
    this->write_queue_.push(packet);
    this->have_pending_.notify_all();
}

void transport_t::writer_loop() {
    try {
        TA_DEBUG() << "USB writer thread starts";
        while(true)
        {
            packet_ptr_t cur_packet;
            {
                std::unique_lock<std::mutex> l(this->queue_mutex_);
                if (writer_termination_requested_)
                    break;

                if (!this->write_queue_.empty())
                {
                    cur_packet = this->write_queue_.front();
                    this->write_queue_.pop();
                } else
                    this->have_pending_.wait(l);
            }
            if (!cur_packet)
                continue;

            // Start the write request
            std::vector<u_char> out_buf_;
            out_buf_.reserve(cur_packet->content_.size()+4);
            out_buf_.push_back(cur_packet->chan_);

            u_char flags = AA_LAST_FRAG | AA_FIRST_FRAG;
            if (cur_packet->encrypted_)
                flags |= AA_ENCRYPTED;
            if (cur_packet->control_)
                flags |= AA_CONTROL_FLAG;
            out_buf_.push_back(flags);

            // Write the content length (big endian)
            out_buf_.push_back(safe_cast<u_char>(cur_packet->content_.size() / 256));
            out_buf_.push_back(safe_cast<u_char>(cur_packet->content_.size() % 256));

            // Write the content data
            out_buf_.insert(out_buf_.end(),
                            cur_packet->content_.begin(), cur_packet->content_.end());

            int actual = 0;
            TA_TRACE() << "Writing packet: " << desc(cur_packet);
            libusb_bulk_transfer(this->dev_.get(), this->endpoint_out_, &out_buf_[0],
                                 safe_cast<int>(out_buf_.size()), &actual, poll_timeout_millis_);
            TA_TRACE() << "Written: " << actual;

            if (actual != out_buf_.size())
                throw std::runtime_error("Failed to write a buffer");
        }
    } catch(const std::exception &ex)
    {
        std::unique_lock<std::mutex> l(this->queue_mutex_);
        stored_exception_ = ex.what();
    } catch(...)
    {
        std::unique_lock<std::mutex> l(this->queue_mutex_);
        stored_exception_ = "Unknown error in writer thread";
    }

    TA_DEBUG() << "USB writer thread ends";
}
