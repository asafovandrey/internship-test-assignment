#pragma once

#include <iostream>

inline int g_failures = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            std::cerr << "ПРОВАЛ " << __FILE__ << ':' << __LINE__    \
                      << ": " #cond "\n";                            \
            ++g_failures;                                            \
        }                                                            \
    } while (0)
