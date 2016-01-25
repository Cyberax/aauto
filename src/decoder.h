//
// Created by Besogonov, Aleksei on 1/26/16.
//

#ifndef AAUTO_DECODER_H
#define AAUTO_DECODER_H

#include "utils.h"
#include <queue>
#include <thread>
#include "aa_helpers.h"

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

struct frame_t
{
    size_t width_, height_, stride_;
    std::vector<u_char> data_;
};
typedef std::shared_ptr<frame_t> frame_ptr_t;


class decoder_t {
    AVCodec* codec_;
    std::shared_ptr<AVPacket> av_packet_;
    std::shared_ptr<AVFrame> av_picture_;
    std::shared_ptr<AVCodecContext> codec_context_;
    volatile bool terminating_;

    std::function<void()> new_frame_callback_;

    std::mutex queue_lock_;
    std::condition_variable have_something_;
    std::queue<packet_ptr_t> packets_;
    std::shared_ptr<AVFrame> last_frame_;

    std::thread decoder_thread_;
    std::string error_;

    std::mutex scaler_mutex_;
    SwsContext *scaler_context_;
public:
    decoder_t(std::function<void()> new_frame_callback);
    virtual ~decoder_t();

    std::pair<size_t, size_t> get_dimensions();
    buf_t get_frame(int tgt_width, int tgt_height);
    void submit_packet(packet_ptr_t packet);
    void check_for_errors();

    static void init_codecs();
private:
    void run_loop();
    void decode_frame(packet_ptr_t packet);
};


#endif //AAUTO_DECODER_H
