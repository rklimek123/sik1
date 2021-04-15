#ifndef STRING_SET_H
#define STRING_SET_H

#ifdef __cplusplus
#include <exception>
#include <set>
#include <string>
#endif
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
#endif
    void set_reset();

// The return bool is task completion status.
// true means success. false means some undisclosed error on the way.
#ifdef __cplusplus
extern "C"
#endif
    bool set_add(const char*);

// The return bool is the answer to the question
// of that string's existence in the set.
#ifdef __cplusplus
extern "C"
#endif
    bool set_exists(const char*);

#endif /* STRING_SET_H */