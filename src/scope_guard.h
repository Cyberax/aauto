//
// Created by Besogonov, Aleksei on 1/24/16.
//

#ifndef AAUTO_SCOPE_GUARD_H
#define AAUTO_SCOPE_GUARD_H

#include <functional>

class scope_guard_t
{
public:
    template<class callable_t> scope_guard_t(
            callable_t && undo_func) : f(std::forward<callable_t>(undo_func)) {}

    scope_guard_t(scope_guard_t && other) : f(std::move(other.f))
    {
        other.f = nullptr;
    }

    ~scope_guard_t()
    {
        if(f) f();
    }

    void dismiss() throw()
    {
        f = nullptr;
    }

    scope_guard_t(const scope_guard_t &) = delete;
    void operator = (const scope_guard_t &) = delete;

private:
    std::function<void()> f;
};


#define CONCATENATE_DIRECT(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_DIRECT(s1, s2)
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __LINE__)

#define ON_BLOCK_EXIT auto ANONYMOUS_VARIABLE(scopeGuard) = scope_guard_t

#endif //AAUTO_SCOPE_GUARD_H
