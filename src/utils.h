//
// Created by Besogonov, Aleksei on 1/24/16.
//

#ifndef AAUTO_UTILS_H
#define AAUTO_UTILS_H

#include <memory>
#include <string>
#include <memory>
#include <strstream>
#include "scope_guard.h"

class notifier_t
{
    bool is_terminating_;
public:
    notifier_t() : is_terminating_(false) { }

    bool is_terminating() const
    {
        return is_terminating_;
    }

    bool check_termination() const
    {
        if (is_terminating())
            throw std::runtime_error("Thread termination");
        return true;
    }

    void set_termination()
    {
        is_terminating_ = true;
    }

    void sleep(uint32_t millis) const;
};

enum debug_level {
    TRACE_OUTPUT,
    DEBUG_OUTPUT,
    INFO_OUTPUT,
    ERR_OUTPUT
};

struct debug_stream_t : public std::strstream
{
    bool discard_;
    static debug_level the_debug_level_;
public:
    debug_stream_t(debug_level lvl);
    virtual ~debug_stream_t() override;

    static void set_debug_level(debug_level lvl) { the_debug_level_ = lvl; }
};

#define TA_TRACE() debug_stream_t(TRACE_OUTPUT)
#define TA_DEBUG() debug_stream_t(DEBUG_OUTPUT)
#define TA_INFO() debug_stream_t(INFO_OUTPUT)

#endif //AAUTO_UTILS_H
