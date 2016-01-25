//
// Created by Besogonov, Aleksei on 1/24/16.
//
#include <assert.h>
#include <iostream>
#include "utils.h"

debug_level debug_stream_t::the_debug_level_ = DEBUG_OUTPUT;

void notifier_t::sleep(uint32_t millis) const
{
    assert(millis >= 0);
    timeval tm={(int)millis/1000, (int)(millis%1000) * 10000};
    select(0, NULL, NULL, NULL, &tm);
}

debug_stream_t::~debug_stream_t() {
    std::string deb(str(), (size_t)pcount());
    if (deb.at(deb.size()-1) != '\n')
        std::cout << deb << std::endl;
    else
        std::cout << deb;
}

debug_stream_t::debug_stream_t(debug_level lvl) {
    discard_ = lvl >= the_debug_level_;
}
