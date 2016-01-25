//
// Created by Besogonov, Aleksei on 1/25/16.
//
#include "crypto.h"
#include <openssl/ssl.h>
#include <openssl/rand.h>


void init_crypto() {
    int ret = SSL_library_init();
    if (ret != 1)
        throw std::runtime_error("Failed to initialize SSL");
//    SSL_load_error_strings();
//    ERR_load_BIO_strings();
//    ERR_load_CRYPTO_strings();
//    ERR_load_SSL_strings();
//
//    OPENSSL_add_all_algorithms_conf();

    ret = RAND_status ();
    if (ret != 1)
        throw std::runtime_error("OpenSSL doesn't have an entropy source");
}

crypto_context_t::crypto_context_t(const std::string &cert, const std::string &pk) {
    // Convert certificate to BIO
    std::shared_ptr<BIO> cert_bio(BIO_new_mem_buf((void*)&cert.at(0), safe_cast<int>(cert.size())),
                                  [](BIO* bio){BIO_free(bio);});
    std::shared_ptr<X509> x509_cert(PEM_read_bio_X509_AUX (cert_bio.get(), 0, 0, 0),
                                    [](X509* xx){X509_free(xx);});
    if (!x509_cert)
        throw std::runtime_error("Failed to parse certificate");

    // Convert PK to BIO
    std::shared_ptr<BIO> pkey_bio(BIO_new_mem_buf((void*)&pk.at(0), safe_cast<int>(pk.size())),
                                  [](BIO* bio){BIO_free(bio);});
    std::shared_ptr<EVP_PKEY> priv_key(PEM_read_bio_PrivateKey(pkey_bio.get(), 0, 0, 0),
                                       [](EVP_PKEY* epk){EVP_PKEY_free(epk);});
    if (!priv_key)
        throw std::runtime_error("Failed to parse the private key");

    SSL_METHOD * method = (SSL_METHOD *) TLSv1_2_method();
    if (!method)
        throw std::runtime_error("Can't initialize SSL method");

    std::shared_ptr<SSL_CTX> ssl_ctx(SSL_CTX_new(method),
                                     [priv_key,x509_cert](SSL_CTX *c) {SSL_CTX_free(c);});
    if (!ssl_ctx)
        throw std::runtime_error("Can't initialize SSL context");

    if (SSL_CTX_use_certificate(ssl_ctx.get(), x509_cert.get()) != 1)
        throw std::runtime_error("Can't use certificate for SSL");

    if (SSL_CTX_use_PrivateKey (ssl_ctx.get(), priv_key.get()) != 1)
        throw std::runtime_error("Can't use private key for SSL");

    BIO* read_bio = BIO_new(BIO_s_mem());
    scope_guard_t read_bio_guard([=](){BIO_free(read_bio);});
    BIO* write_bio = BIO_new(BIO_s_mem());
    scope_guard_t write_bio_guard([=](){BIO_free(write_bio);});
    if (!read_bio || !write_bio)
        throw std::runtime_error("Can't create SSL BIOs");

    std::shared_ptr<SSL> ssl_conn(SSL_new(ssl_ctx.get()), [ssl_ctx](SSL* ssl){SSL_free(ssl);});
    if (!ssl_conn)
        throw std::runtime_error("Can't create SSL connection");
    if (SSL_check_private_key(ssl_conn.get()) != 1)
        throw std::runtime_error("SSL connection failed PK check");

    SSL_set_bio(ssl_conn.get(), read_bio, write_bio);
    // From this point on, BIOs are managed by SSL
    read_bio_guard.dismiss();
    write_bio_guard.dismiss();

    //TODO: verify Google's cert?
    SSL_set_verify(ssl_conn.get(), SSL_VERIFY_NONE, NULL);
    SSL_set_connect_state(ssl_conn.get());

    this->read_bio_ = read_bio;
    this->write_bio_ = write_bio;
    this->ssl_ = ssl_conn;
    this->ssl_ctx_ = ssl_ctx;
}

buf_t crypto_context_t::do_handshake(const buf_t &input, size_t pos) {
    std::lock_guard<std::mutex> l(this->mutex_);
    ensure_handshake_state(false);

    buf_t res_buf;
    size_t in_pos = pos;

    while(true)
    {
        int ret = SSL_do_handshake(this->ssl_.get());
        if (ret == 0 && in_pos != input.size())
            throw std::runtime_error("Handshake completed with pending input");

        int err = SSL_get_error(this->ssl_.get(), ret);
        if (err && err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
            throw std::runtime_error(str_out_t() << "Unexpected error from SSL: " << err);

        int pending = BIO_pending(this->write_bio_);
        if (pending != 0) {
            res_buf.resize(res_buf.size() + pending);
            int res = BIO_read(this->write_bio_, &res_buf.at(res_buf.size()-pending), pending);
            if (res != pending)
                throw std::runtime_error(str_out_t() << "Pending bytes: "<<pending
                                         <<" differ from read: " << res);
        } else if (in_pos < input.size()) {
            int res=BIO_write(this->read_bio_,
                              &input.at(in_pos), safe_cast<int>(input.size() - in_pos));
            if (res <= 0)
                throw std::runtime_error(str_out_t() << "SSL library refused read of "
                                         <<(input.size() - in_pos)<<" bytes");
            in_pos += res;
        } else {
            break; //Nothing to do
        }
    }

    return std::move(res_buf);
}

bool crypto_context_t::is_handshake_finished() const {
    std::lock_guard<std::mutex> l(this->mutex_);
    return SSL_is_init_finished(this->ssl_.get());
}

buf_t crypto_context_t::encrypt(const buf_t &input, size_t pos) {
    std::lock_guard<std::mutex> l(this->mutex_);
    ensure_handshake_state(true);

    buf_t res_buf;

    //We are using memory BIOs which can expand indefinitely, so SSL_write
    //must always succeed.
    int ret = SSL_write(this->ssl_.get(), &input.at(pos), safe_cast<int>(input.size()-pos));
    if (ret != input.size())
        throw std::runtime_error(str_out_t() << "Failed to encrypt " << input.size() << " bytes");

    int pending = BIO_pending(this->write_bio_);
    if (pending != 0) {
        res_buf.resize(res_buf.size() + pending);
        int res = BIO_read(this->write_bio_, &res_buf.at(res_buf.size()-pending), pending);
        if (res != pending)
            throw std::runtime_error(str_out_t() << "Pending bytes: "<<pending
                                     <<" differ from read: " << res);
    }

    return std::move(res_buf);
}

buf_t crypto_context_t::decrypt(const buf_t &input, size_t pos) {
    std::lock_guard<std::mutex> l(this->mutex_);
    ensure_handshake_state(true);

    //BIO write is guaranteed to succeed, since we're using memory-based
    //BIOs that can expand to any size
    int res = BIO_write(this->read_bio_, &input.at(pos), safe_cast<int>(input.size()-pos));
    if (res != input.size())
        throw std::runtime_error(str_out_t() << "Input bytes: "<< input.size()
                                 <<" differ from written: " << res);

    int size_step = safe_cast<int>(input.size());
    buf_t res_buf;
    while(true)
    {
        size_t pos = res_buf.size();
        //Reserve some space (might be excessive)
        res_buf.resize(res_buf.size() + size_step);
        int ret = SSL_read(this->ssl_.get(), &res_buf.at(pos), size_step);
        if (SSL_get_error(this->ssl_.get(), ret) == SSL_ERROR_WANT_READ)
            break;
        if (ret <= 0)
            throw std::runtime_error(str_out_t() << "SSL read failed, error="<<ret);

        //Snap our buffer back to real size
        res_buf.resize(pos+ret);
        //Get the size of the next step (the pending bytes in the buffer)
        //size_step = SSL_pending(this->ssl_.get());
        //if (size_step==0)
        //    break;
    }

    return std::move(res_buf);
}

void crypto_context_t::ensure_handshake_state(bool expect_finished) {
    bool is_finished = SSL_is_init_finished(this->ssl_.get());
    if (is_finished != expect_finished)
        throw std::runtime_error("Unexpected handshake state");
}
