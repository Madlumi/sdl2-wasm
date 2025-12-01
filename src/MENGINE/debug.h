#pragma once
#include <stdio.h>

// Lightweight logging helpers used throughout the engine
#define THROW(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
