#include "mutil.h"
#include <stdio.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "keys.c"
#include "tick.c"
#include "renderer.h"
#include "ui.c"
int running=1;

void quit(){ SDL_Quit(); running=0; }

RECT MAINUI={0,0,512,512};
void init(){ keysInit(); renderInit(512,512); initUiHandler(MAINUI);
    addElem(&ui[0], newElem(MAINUI, *onPressFunction, *onUpdateFunction, *onRenderFunction)) ;
}

void mainLoop(){ events(); tick(); render(); }

int main(int argc, char* argv[]) {
   init();
   #ifdef __EMSCRIPTEN__
   emscripten_set_main_loop(mainLoop, 0, 1);
   #else
   while(running) { mainLoop(); SDL_Delay(16); if(QUIT){quit();}}
   #endif 
}
