#include "mutil.h"
#define MAX_TICK_FUNC 10
typedef void (*tickFunc)(void);
tickFunc tickFuncs[MAX_TICK_FUNC];
void addTickFunction(tickFunc func) {
   FOR(MAX_TICK_FUNC,{ if (!tickFuncs[i]) { tickFuncs[i] = func; break; } });
}
void tick(){
   FOR(MAX_TICK_FUNC,{ if (tickFuncs[i]) { tickFuncs[i]();}});
}

