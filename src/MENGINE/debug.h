#ifndef DEBUG_H
#define DEBUG_H
#include <stdio.h>

// Minimal debugging helpers used across the engine.
#define THROW(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

#endif
