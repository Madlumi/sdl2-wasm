#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#define THROW(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

#endif
