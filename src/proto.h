//
// Created by Besogonov, Aleksei on 1/26/16.
//

#ifndef AAUTO_PROTO_H
#define AAUTO_PROTO_H

#include "transport.h"
#include "crypto.h"

class decoder_t;

class proto_t {
    transport_ptr_t trans_;
    std::shared_ptr<crypto_context_t> crypto_;
    std::shared_ptr<decoder_t> decoder_;
    notifier_t *terminator_;

    enum proto_phase_t {
        INIT, VERSION_NEGO, SSL_HANDSHAKE, READY,
    };
    proto_phase_t phase_;
    time_t phase_start_;
    static const int VERSION_NEGO_TIMEOUT_SEC = 2;
public:
    proto_t(const transport_ptr_t &trans_,
            const std::shared_ptr<crypto_context_t> &crypto_,
            notifier_t *terminator,
            std::shared_ptr<decoder_t> decoder) :
            trans_(trans_), crypto_(crypto_), phase_(INIT),
            terminator_(terminator), decoder_(decoder) {}

    void run_loop();
    void notify_mouse(int x, int y, bool mouse_down);
private:
    void transit_to(proto_phase_t p)
    {
        phase_ = p;
        phase_start_ = time(NULL);
    }

    void dispatch_in_established(packet_ptr_t pack);
    void encrypt_and_send(packet_ptr_t pack);
};

#endif //AAUTO_PROTO_H
