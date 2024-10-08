#include <SDL.h>
#include <SDL_image.h>
#include "mutil.h"
#include "renderer.h"
int w=512; int h=125;
SDL_Window *window; SDL_Renderer *renderer; SDL_Surface *surface;
new_pool(renderF, RenderFunction);
void renderInit(CON I W, CON I H){
   w=W;h=H;
   SDL_Init(SDL_INIT_VIDEO);
   SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer);
   surface = SDL_CreateRGBSurface(0, w , h, 32, 0, 0, 0, 0);
}
void render(){
   if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
   Uint32 * pixels = (Uint32 *)surface->pixels;
   FOR( renderF_num,{ renderF_pool[i](pixels); });
   if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
   SDL_Texture *screenTexture = SDL_CreateTextureFromSurface(renderer, surface);

   SDL_RenderClear(renderer);
   SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
   SDL_RenderPresent(renderer);
   SDL_DestroyTexture(screenTexture);
}


