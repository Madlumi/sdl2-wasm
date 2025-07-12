#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "keys.h"
#include "tick.h"
#include "renderer.h"
#include "ui.h"
int running=1;

void quit(){ renderFree(); SDL_Quit(); running=0; }

RECT MAINUI={50,50,50,50};
void init(){ keysInit(); renderInit(512,512); initUiHandler(MAINUI);
    addElem(&ui[0], newElem(MAINUI, *onPressFunction, *onUpdateFunction, *onRenderFunction)) ;
    addElem(&ui[0], newElem((RECT){150,50,50,50}, *onPressFunction, *onUpdateFunction, *onRenderFunction)) ;
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



void test(){
   
}
void testRender(){

}
void testTick(D dt){

}
