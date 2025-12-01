#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "keys.h"
#include "tick.h"
#include "renderer.h"
#include "EGAME/game.h"
int running=1;

void quit(){ renderFree(); running=0; }

void init(){
   keysInit();
   renderInitGL(1024,768, "Wolf3D GL");
   gameInit();
}



void mainLoop(){ events(0); tick(); render(); }

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
