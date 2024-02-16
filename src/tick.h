
#ifndef TICK
#define TICK
#include "mutil.h"
typedef void (*tickFunc)(void);
void addTickFunction(tickFunc func);
void tick();
#endif
