#ifndef TICK
#define TICK
#include "mutil.h"
typedef void (*tickFunc)(D dt);
new_pool_h(tickF, tickFunc);
void tick();
#endif
