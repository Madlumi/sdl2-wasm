#include "mutil.h"
#include "tick.h"
#include <SDL.h>

new_pool(tickF, tickFunc);

void tick(){
   static Uint32 last = 0;
   Uint32 now = SDL_GetTicks();
   D dt = (now - last) / 1000.0;
   last = now;
   FOR(tickF_num,{ tickF_pool[i](dt); });
}

