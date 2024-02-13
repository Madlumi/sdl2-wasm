#ifndef RENDERER
#define RENDERER 
#include "SDL_rect.h"
#include "keys.c"
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <sys/ucontext.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include "mutil.h"
#include<unistd.h>
int w=512; int h=125;
SDL_Window *window; SDL_Renderer *renderer; SDL_Surface *surface;
#define MAX_RENDER_FUNCTIONS 10
typedef void (*RenderFunction)(Uint32*);
RenderFunction renderFunctions[MAX_RENDER_FUNCTIONS];

void addRenderFunction(RenderFunction func) {
  FOR(MAX_RENDER_FUNCTIONS,{ if (!renderFunctions[i]) { renderFunctions[i] = func; break; } }); }

void renderInit(CON I W, CON I H){
   w=W;h=H;
   SDL_Init(SDL_INIT_VIDEO);
   SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer);
   surface = SDL_CreateRGBSurface(0, w , h, 32, 0, 0, 0, 0);
}
V renderFree(){ }
void render(){
   if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
   Uint32 * pixels = (Uint32 *)surface->pixels;
   FOR(MAX_RENDER_FUNCTIONS,{ if (renderFunctions[i]) { renderFunctions[i](pixels); } });

   if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
   SDL_Texture *screenTexture = SDL_CreateTextureFromSurface(renderer, surface);

   SDL_RenderClear(renderer);
   SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
   SDL_RenderPresent(renderer);
   SDL_DestroyTexture(screenTexture);
}

#endif

