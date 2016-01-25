//
// Created by Besogonov, Aleksei on 1/25/16.
//

#ifndef AAUTO_CRYPTO_H
#define AAUTO_CRYPTO_H

#include "utils.h"

typedef struct ssl_st SSL;
typedef struct bio_st BIO;
typedef struct ssl_ctx_st SSL_CTX;

class crypto_context_t {
    std::shared_ptr<SSL> ssl_;
    std::shared_ptr<SSL_CTX> ssl_ctx_;
    //Raw pointer, as BIOs are managed by SSL
    BIO *read_bio_, *write_bio_;
    mutable std::mutex mutex_;

public:
    crypto_context_t(const std::string &cert, const std::string &pk);

    bool is_handshake_finished() const;
    buf_t do_handshake(const buf_t &input, size_t pos);

    buf_t encrypt(const buf_t &input, size_t pos);
    buf_t decrypt(const buf_t &input, size_t pos);

private:
    void ensure_handshake_state(bool expect_finished);
};

void init_crypto();

#endif //AAUTO_CRYPTO_H
