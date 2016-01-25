#include <iostream>
#include "transport.h"
#include <fstream>
#include "crypto.h"
#include "proto.h"
#include "decoder.h"

#include <SDL2/SDL.h>

class AppWindow {
    SDL_Window *window_;
    usb_context_ptr_t usb_ctx_;
    Uint32 new_frame_event_, proto_state_event_;

    notifier_t terminator_;
    std::string cert_, pk_;

    std::thread proto_thread_;
    std::mutex proto_mutex_;
    std::shared_ptr<decoder_t> decoder_;
    std::shared_ptr<proto_t> proto_;
public:

    AppWindow(const std::string &cert, const std::string &pk) :
        cert_(cert), pk_(pk), window_(0)
    {
        new_frame_event_ = SDL_RegisterEvents(1);
        proto_state_event_ = SDL_RegisterEvents(1);

        // Create an application window with the following settings:
        window_ = SDL_CreateWindow(
                "Android Auto",
                SDL_WINDOWPOS_UNDEFINED,           // initial x position
                SDL_WINDOWPOS_UNDEFINED,           // initial y position
                800,                               // width, in pixels
                480,                               // height, in pixels
                SDL_WINDOW_RESIZABLE
        );

        // Check that the window was successfully created
        if (!window_)
            throw std::runtime_error("Failed to create an SDL window");

        usb_ctx_ = get_usb_lib();
    }

    ~AppWindow()
    {
        proto_thread_.join();
        
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    void init_decoder()
    {
        auto new_frame_callback = [this]{
            SDL_Event fe={0};
            fe.user.type = new_frame_event_;
            SDL_PushEvent(&fe);
        };
        std::unique_lock<std::mutex> l(proto_mutex_);
        decoder_ = std::shared_ptr<decoder_t>(new decoder_t(new_frame_callback));

        std::shared_ptr<crypto_context_t> crypto(new crypto_context_t(cert_, pk_));
        auto trans = find_usb_transport(usb_ctx_, &terminator_);

        proto_ = std::shared_ptr<proto_t>(new proto_t(trans, crypto, &terminator_, decoder_));
    }

    void run_event_loop()
    {
        proto_thread_ = std::thread([](AppWindow *a) { a->run_proto_loop();}, this);
        SDL_ShowWindow(window_);

        SDL_Event event;
        while(true) {
            try {
                SDL_WaitEvent(&event);
                if (event.type == SDL_QUIT) {
                    terminator_.set_termination();
                    break;
                }

                if (event.type == new_frame_event_) {
                    render_frame();
                }

                if (event.type == SDL_MOUSEBUTTONUP ||
                        event.type == SDL_MOUSEBUTTONDOWN)
                    notify_mouse(event);
            } catch(const std::exception &ex)
            {
                std::cerr << "Unexpected error: " << ex.what();
                terminator_.set_termination();
                SDL_Quit();
            }
        }

        SDL_HideWindow(window_);
    }

    void notify_mouse(SDL_Event ev)
    {
        std::unique_lock<std::mutex> l(proto_mutex_);
        proto_->notify_mouse(ev.button.x, ev.button.y, ev.button.state == SDL_PRESSED);
    }

    void run_proto_loop()
    {
        while(!terminator_.is_terminating()) {
            try {
                init_decoder();
                proto_->run_loop();
            } catch(const std::exception &ex)
            {
                std::cerr << "Exception: " << ex.what() << std::endl;
            }

            terminator_.sleep(10000);
        }
    }
    
    void render_frame()
    {
        int req_width, req_height, cur_width, cur_height;
        SDL_GetWindowSize(window_, &cur_width, &cur_height);

        buf_t frame_buf;
        {
            std::unique_lock<std::mutex> l(proto_mutex_);
            decoder_->check_for_errors();
            std::tie(req_width, req_height) = decoder_->get_dimensions();
            frame_buf = decoder_->get_frame(cur_width, cur_height);
        }
        if (frame_buf.empty())
            return;

        SDL_Surface *frame=SDL_CreateRGBSurfaceFrom((void*)&frame_buf.at(0),
                                              cur_width,
                                              cur_height,
                                              32,
                                              4*cur_width,
                                              0x000000FF, 0x0000FF00, 0x00FF0000,
                                              0x00000000);
        ON_BLOCK_EXIT([=]{SDL_FreeSurface(frame);});

        SDL_BlitSurface(frame, NULL, SDL_GetWindowSurface(window_), NULL);

        SDL_UpdateWindowSurface(window_);
    }
};

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    init_crypto();
    decoder_t::init_codecs();
    //debug_stream_t::set_debug_level(TRACE_OUTPUT);

    std::ifstream cert_file("certificate.crt");
    std::ifstream pk_file("private_key.key");
    std::string cert((std::istreambuf_iterator<char>(cert_file)),
                     (std::istreambuf_iterator<char>()));
    std::string pk((std::istreambuf_iterator<char>(pk_file)),
                   (std::istreambuf_iterator<char>()));
    if (cert.empty() || pk.empty()) {
        std::cerr << "Can't read PK/CERT pair" << std::endl;
        exit(1);
    }

    try {
        AppWindow window(cert, pk);
        window.run_event_loop();
    } catch(const std::exception &ex)
    {
        std::cerr << "Unexpected error: " << ex.what();
        exit(3);
    }

    return 0;
//    Fl_Window *window = new Fl_Window(800, 480);
//    window->begin();
//    Fl_Widget *box = new Fl_Box(20, 40, 260, 100, "Connecting");
//    box->box(FL_UP_BOX);
//    box->labelfont(FL_HELVETICA_BOLD_ITALIC);
//    box->labelsize(36);
//    box->labeltype(FL_SHADOW_LABEL);
//    window->end();
//    window->show(argc, argv);
//    return Fl::run();
}
