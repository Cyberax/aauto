//
// Created by Besogonov, Aleksei on 1/24/16.
//
#include <assert.h>
#include <iostream>
#include "utils.h"
#include <sys/select.h>

debug_level debug_stream_t::the_debug_level_ = DEBUG_OUTPUT;
std::mutex debug_stream_t::out_mutex_;

void notifier_t::sleep(uint32_t millis) const
{
    if (is_terminating())
        return;

    assert(millis >= 0);
    timeval tm={(int)millis/1000, (int)(millis%1000) * 10000};
    select(0, NULL, NULL, NULL, &tm);
}

debug_stream_t::~debug_stream_t() {
    if (discard_)
        return;
    std::lock_guard<std::mutex> l(out_mutex_);

    std::string deb(str());
    if (deb.at(deb.size()-1) != '\n')
        std::cout << deb << std::endl;
    else
        std::cout << deb;
}

debug_stream_t::debug_stream_t(debug_level lvl) {
    discard_ = lvl < the_debug_level_;
}
