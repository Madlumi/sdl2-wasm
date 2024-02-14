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
RenderFunction* renderFunctions = NULL;
size_t maxRenderFunctions = 0;
size_t numRenderFunctions = 0;

void addRenderFunction(RenderFunction func) {
    if (numRenderFunctions >= maxRenderFunctions) {
        size_t newSize = maxRenderFunctions == 0 ? 1 : maxRenderFunctions * 2;
        RenderFunction* newRenderFunctions = realloc(renderFunctions, newSize * sizeof(RenderFunction));
        if (newRenderFunctions == NULL) { fprintf(stderr, "Memory allocation failed.\n"); return; }
        renderFunctions = newRenderFunctions;
        maxRenderFunctions = newSize;
    }
    renderFunctions[numRenderFunctions++] = func;
}
void renderInit(CON I W, CON I H){
   w=W;h=H;
   SDL_Init(SDL_INIT_VIDEO);
   SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer);
   surface = SDL_CreateRGBSurface(0, w , h, 32, 0, 0, 0, 0);
}

V renderFree(){
    free(renderFunctions);
    renderFunctions = NULL;
    maxRenderFunctions = 0;
    numRenderFunctions = 0;
}

void render(){
   if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
   Uint32 * pixels = (Uint32 *)surface->pixels;
   FOR( numRenderFunctions,{ renderFunctions[i](pixels); });
   if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
   SDL_Texture *screenTexture = SDL_CreateTextureFromSurface(renderer, surface);

   SDL_RenderClear(renderer);
   SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
   SDL_RenderPresent(renderer);
   SDL_DestroyTexture(screenTexture);
}

#endif

