#include <iostream>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include "transport.h"

int main(int argc, char **argv) {
    usb_context_ptr_t ctx=get_usb_lib();
    notifier_t stop_notifier;

    const std::shared_ptr<transport_t> &trans = find_usb_transport(ctx, stop_notifier);

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
