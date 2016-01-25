//
// Created by Besogonov, Aleksei on 1/26/16.
//

#include "decoder.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
};

void decoder_t::init_codecs() {
    avcodec_register_all();
}

decoder_t::decoder_t(std::function<void()> new_frame_callback) :
        terminating_(false), new_frame_callback_(new_frame_callback), scaler_context_(0) {
    codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    codec_context_ = std::shared_ptr<AVCodecContext>(avcodec_alloc_context3(codec_),
                                                     [](AVCodecContext *c)
                                                     {
                                                         avcodec_close(c);
                                                         av_free(c);
                                                     });

    if (!codec_context_ || avcodec_open2(codec_context_.get(), codec_, NULL) < 0)
        throw std::runtime_error("Failed to open the codec");

    av_packet_ = std::shared_ptr<AVPacket>(new AVPacket{0});
    av_init_packet(av_packet_.get());
    av_picture_ = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *f){av_frame_free(&f);});

    decoder_thread_ = std::thread([](decoder_t *that){that->run_loop();}, this);
}

decoder_t::~decoder_t() {
    sws_freeContext(scaler_context_);
    this->terminating_ = true;
    {
        std::unique_lock<std::mutex> l(queue_lock_);
        have_something_.notify_all();
    }
    decoder_thread_.join();
}

void decoder_t::check_for_errors() {
    std::unique_lock<std::mutex> l(queue_lock_);
    if (!error_.empty())
        throw std::runtime_error(error_);
}

void decoder_t::run_loop() {
    TA_INFO() << "Decoder thread starting";
    try {
        while(!terminating_)
        {
            packet_ptr_t cur_packet;
            {
                std::unique_lock<std::mutex> l(queue_lock_);
                if (!this->packets_.empty())
                {
                    cur_packet = this->packets_.front();
                    this->packets_.pop();
                } else
                    have_something_.wait(l);
            }
            if (!cur_packet)
                continue;

            decode_frame(cur_packet);
        }
    } catch(const std::exception &ex)
    {
        error_ = ex.what();
    } catch(...)
    {
        error_ = "Неведомая х..ня";
    }
    this->new_frame_callback_();
    TA_INFO() << "Decoder thread ends";
}

void decoder_t::decode_frame(packet_ptr_t packet) {
    std::unique_lock<std::mutex> l(queue_lock_);

    std::vector<u_char> padded;
    padded.resize(packet->content_.size() - 2 + FF_INPUT_BUFFER_PADDING_SIZE);
    std::copy(packet->content_.begin()+2, packet->content_.end(), padded.begin());

    int got_picture;
    av_packet_->data = &padded.at(0);
    av_packet_->size = (int) (packet->content_.size() - 2);

    int res = avcodec_decode_video2(codec_context_.get(), av_picture_.get(), &got_picture,
                              av_packet_.get());
    if (res < 0)
        TA_DEBUG() << "BAD frame " << std::hex << (uint32_t)res;
        //throw std::runtime_error("Failed to decode H264 data");
    // Sometimes we can't decode a picture right away
    if (!got_picture)
       return;

//    {
//        std::unique_lock<std::mutex> l(queue_lock_);
//        this->last_frame_ = av_picture_;
//        av_picture_.reset();
//    }
    this->last_frame_ = av_picture_;
    this->new_frame_callback_();
}

void decoder_t::submit_packet(packet_ptr_t packet) {
    std::unique_lock<std::mutex> l(queue_lock_);
    this->packets_.push(packet);
    this->have_something_.notify_all();
}

buf_t decoder_t::get_frame(int tgt_width, int tgt_height) {
    std::unique_lock<std::mutex> l(queue_lock_);
    //std::unique_lock<std::mutex> sl(scaler_mutex_);
    std::shared_ptr<AVFrame> frame = this->last_frame_;
    if (!frame)
        return buf_t();

//    std::shared_ptr<AVFrame> frame;
//    {
//        frame = this->last_frame_;
//        this->last_frame_.reset();
//
//        if (!frame)
//            return buf_t();
//    }
//
    scaler_context_ = sws_getContext(
            frame->width, frame->height,
            (AVPixelFormat) frame->format,
            tgt_width, tgt_height,
            AV_PIX_FMT_RGBA, SWS_BILINEAR,
            NULL, NULL, NULL);

    AVFrame *rgb=av_frame_alloc();
    ON_BLOCK_EXIT([&]{av_frame_free(&rgb);});

    // Determine required buffer size and allocate buffer
    int bytes=av_image_get_buffer_size(AV_PIX_FMT_RGBA, tgt_width, tgt_height, 1);
    buf_t res;
    res.resize(safe_cast<size_t>(bytes));

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    av_image_fill_arrays(rgb->data, rgb->linesize, &res[0], AV_PIX_FMT_RGBA, tgt_width, tgt_height, 1);
    //avpicture_fill((AVPicture *)rgb, &res[0], AV_PIX_FMT_RGB24, tgt_width, tgt_height);

    sws_scale(scaler_context_, (uint8_t const * const *)frame->data,
              frame->linesize, 0, frame->height,
              rgb->data, rgb->linesize);

    return res;
}

std::pair<size_t, size_t> decoder_t::get_dimensions()
{
    std::unique_lock<std::mutex> l(queue_lock_);
    if (!last_frame_)
        return std::make_pair(0,0);
    return std::make_pair(last_frame_->width, last_frame_->height);
}
