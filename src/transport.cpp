//
// Created by Besogonov, Aleksei on 1/22/16.
//

#include "transport.h"
#include <libusb.h>

static const int GOOGLE_VENDOR_ID = 0x18d1;
static const int GOOGLE_ACCESSORY_PID = 0x2d00;
static const int DEFAULT_TIMEOUT_MS = 1000;

// OAP Control requests
enum oap_control_req {
    ACC_REQ_GET_PROTOCOL = 51,
    ACC_REQ_SEND_STRING = 52,
    ACC_REQ_START = 53,
    ACC_REQ_REGISTER_HID = 54,
    ACC_REQ_UNREGISTER_HID = 55,
    ACC_REQ_SET_HID_REPORT_DESC = 56,
    ACC_REQ_SEND_HID_EVENT = 57,
    ACC_REQ_AUDIO = 58,
};

enum acc_control_indexes {
    ACC_IDX_MAN = 0,
    ACC_IDX_MOD = 1,
};

static const char *ACC_MANUFACTURER = "Android";
static const char *ACC_MODEL = "Android Auto";


std::ostream &transport_t::operator<<(std::ostream &ostream) {
    return ostream;
}

usb_context_ptr_t get_usb_lib()
{
    std::shared_ptr<libusb_context> usb_ctx;
    libusb_context *ctx;
    libusb_init(&ctx); //initialize a library session
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
                      << buf[0] << buf[1];

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

std::shared_ptr<transport_t> find_usb_transport(const usb_context_ptr_t &ctx,
                                                const notifier_t &notifier)
{
    device_ptr_t dev;
    while (!dev)
    {
        TA_TRACE() << "Enumerating USB devices";
        dev = enumerate_devices(ctx);
        if (!dev || libusb_reset_device(dev.get()) != LIBUSB_SUCCESS)
            notifier.sleep(1000);
    }
    TA_INFO() << "Found an ACC device.";
    

    return std::shared_ptr<transport_t>();
}
