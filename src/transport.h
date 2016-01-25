//
// Created by Besogonov, Aleksei on 1/22/16.
//

#ifndef AAUTO_TRANSPORT_H
#define AAUTO_TRANSPORT_H

#include "utils.h"
#include <thread>
#include <queue>

struct libusb_context;
struct libusb_device_handle;

typedef std::shared_ptr<libusb_context> usb_context_ptr_t;
typedef std::shared_ptr<libusb_device_handle> device_ptr_t;

struct packet_t;
typedef std::shared_ptr<packet_t> packet_ptr_t;

class transport_t {
    usb_context_ptr_t ctx_;
    device_ptr_t dev_;
    const notifier_t *terminator_;

    uint8_t endpoint_in_, endpoint_out_, interface_id_;
    bool claimed_interface_;

    // Writer subinterface
    std::thread writer_thread_;
    std::queue<packet_ptr_t> write_queue_;
    std::mutex queue_mutex_;
    std::condition_variable have_pending_;
    bool writer_termination_requested_;
    std::string stored_exception_;

    //Reader packet
    buf_t stream_buffer_;
    size_t read_window_size_, cur_consumed_;
    packet_ptr_t mutli_packet_;
    enum state_e {
        READING_HEADER, READING_BODY
    };

    static const int poll_timeout_millis_ = 1000;
    static const size_t stream_buffer_size_ = 128000;
public:
    transport_t(const usb_context_ptr_t &ctx_, const device_ptr_t &dev_,
                const notifier_t *terminator);
    virtual ~transport_t();

    packet_ptr_t handle_events(unsigned int timeout_millis=poll_timeout_millis_);
    void write_packet(packet_ptr_t packet);

    std::ostream& operator<<(std::ostream&);

private:
    void writer_loop();
    bool read_more(unsigned int timeout_millis);
};

typedef std::shared_ptr<transport_t> transport_ptr_t;

usb_context_ptr_t get_usb_lib();
transport_ptr_t find_usb_transport(const usb_context_ptr_t &ctx, const notifier_t *notifier);

#endif //AAUTO_TRANSPORT_H
