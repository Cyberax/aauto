//
// Created by Besogonov, Aleksei on 1/24/16.
//

#ifndef AAUTO_UTILS_H
#define AAUTO_UTILS_H

#include <memory>
#include <string>
#include <memory>
#include <sstream>
#include <stdexcept>
#include "scope_guard.h"
#include <vector>

typedef std::vector<u_char> buf_t;

class terminated_exception : public std::logic_error
{
public:
    terminated_exception(const std::string &s) : logic_error(s) {}
    terminated_exception(const std::logic_error &o) : logic_error(o) {}
};

class notifier_t
{
    bool is_terminating_;
    int pipe_r_, pipe_w_;
public:
    notifier_t() : is_terminating_(false)
    {
        int desc[2] = {0};
        if (pipe(desc) != 0)
            throw std::runtime_error("Can't create a pipe");
        pipe_r_ = desc[0];
        pipe_w_ = desc[1];
    }

    int get_pipe_fd() const
    {
        return pipe_r_;
    }

    bool is_terminating() const
    {
        return is_terminating_;
    }

    bool check_termination() const
    {
        if (is_terminating())
            throw terminated_exception("Thread termination");
        return true;
    }

    void set_termination()
    {
        if (is_terminating_)
            return;
        is_terminating_ = true;
        write(pipe_w_, "A", 1);
    }

    void sleep(uint32_t millis) const;
};

enum debug_level {
    TRACE_OUTPUT,
    DEBUG_OUTPUT,
    INFO_OUTPUT,
    ERR_OUTPUT
};


struct debug_stream_t : public std::stringstream
{
    bool discard_;
    static debug_level the_debug_level_;
    static std::mutex out_mutex_;
public:
    debug_stream_t(debug_level lvl);
    virtual ~debug_stream_t() override;

    static void set_debug_level(debug_level lvl) { the_debug_level_ = lvl; }
    static debug_level get_debug_level() { return the_debug_level_;}
};

#define TA_TRACE() debug_stream_t(TRACE_OUTPUT)
#define TA_DEBUG() debug_stream_t(DEBUG_OUTPUT)
#define TA_INFO() debug_stream_t(INFO_OUTPUT)


struct str_out_t : public std::stringstream
{
public:
    operator std::string() const
    {
        return str();
    }
};


template<class to, class from> inline to safe_cast(from f)
{
    to t = static_cast<to>(f);
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
    if (t != f)
        throw std::out_of_range("Safe cast");
#pragma clang diagnostic pop
    return t;
}

#endif //AAUTO_UTILS_H
