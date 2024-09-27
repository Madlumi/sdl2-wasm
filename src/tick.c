#include "mutil.h"
#include "tick.h"

new_pool(tickF, tickFunc);

void tick(){
   FOR( tickF_num,{ tickF_pool[i](); });
}

