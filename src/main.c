#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "keys.h"
#include "tick.h"
#include "renderer.h"
#include "res.h"
#include "terminal.h"
#include "config.h"
int running=1;

void quit(){ renderFree(); SDL_Quit(); running=0; }

void init(){
   keysInit();
   configInit();
   renderInit(512,512, "stick-terminal");
   terminalInit();
}



void mainLoop(){ events(0); tick(); render(); if(QUIT){quit();} }

int main(int argc, char* argv[]) {
   init();
   #ifdef __EMSCRIPTEN__
   emscripten_set_main_loop(mainLoop, 0, 1);
   #else
   while(running) { mainLoop(); SDL_Delay(16); }
   #endif
}



void test(){

}
void testRender(){

}
void testTick(D dt){

}
