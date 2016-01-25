//
// Created by Besogonov, Aleksei on 1/22/16.
//

#ifndef AAUTO_TRANSPORT_H
#define AAUTO_TRANSPORT_H

#include "utils.h"

struct libusb_context;
struct libusb_device_handle;

typedef std::shared_ptr<libusb_context> usb_context_ptr_t;
typedef std::shared_ptr<libusb_device_handle> device_ptr_t;

class transport_t {
    usb_context_ptr_t ctx_;
    device_ptr_t dev_;
public:
    transport_t(const usb_context_ptr_t &ctx_, const device_ptr_t &dev_) :
            ctx_(ctx_), dev_(dev_) { }

    std::ostream& operator<<(std::ostream&);

};

usb_context_ptr_t get_usb_lib();
std::shared_ptr<transport_t> find_usb_transport(const usb_context_ptr_t &ctx,
                                                const notifier_t &notifier);

#endif //AAUTO_TRANSPORT_H
