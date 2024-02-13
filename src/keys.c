#ifndef MKEYHANDLER
#define MKEYHANDLER

#include "SDL_rect.h"
#include <SDL.h>
#include <stdio.h>
#include <sys/ucontext.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include "tick.c"
#define IN(x,l,h) ((l)<=(x)&&(x)<=(h))
#include<unistd.h>

#define keyn 512
#define mkeyn 24
int KEYS[keyn];
SDL_Point mpos = {0,0};
int MKEYS[mkeyn];
int QUIT=0;
int mouseWheelMoved=0;
void events(){
   SDL_Event e;
   SDL_GetMouseState(&mpos.x, &mpos.y);
   for(int i = 0; i < mkeyn; i++){if(MKEYS[i]>1){MKEYS[i]--;}}
   for(int i = 0; i < keyn; i++){if(KEYS[i]>1){KEYS[i]--;}}
   while(SDL_PollEvent(&e)) {
      //if mousewheelevent: add to mouseWheelMoved 
      if (e.type==SDL_KEYDOWN){if(!IN(e.key.keysym.sym,0,keyn-1)){printf("key: %d\n",e.key.keysym.sym);return;}KEYS[e.key.keysym.sym] = 2;}
      else if (e.type == SDL_KEYUP){if(!IN(e.key.keysym.sym,0,keyn-1)){return;}KEYS[e.key.keysym.sym] = 0;}
      else if (e.type == SDL_MOUSEBUTTONDOWN){if(!IN(e.button.button,0,mkeyn-1)){printf("key: %d\n",e.button.button);return;}MKEYS[e.button.button]=2;}
      else if (e.type == SDL_MOUSEBUTTONUP){if(!IN(e.button.button,0,mkeyn-1)){printf("key: %d\n",e.button.button);return;}MKEYS[e.button.button]=0;}
      else if (e.type == SDL_MOUSEWHEEL) { mouseWheelMoved+=e.wheel.y ; }


      else if (e.type == SDL_QUIT){ QUIT=1; }
   }
#ifndef __EMSCRIPTEN__
   if(KEYS[SDLK_q]) { QUIT=1; }
#endif
}

void keysInit(){
   addTickFunction(events);
   for(int i = 0; i < 322; i++) { // init them all to false
      KEYS[i] = false;
   }
}
#endif
