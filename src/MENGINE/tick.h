#ifndef TICK
#define TICK
#include "mutil.h"
typedef void (*tickFunc)(void);
new_pool_h(tickF, tickFunc);
void tick();
#endif
