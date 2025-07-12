#include <SDL.h>
#include <SDL_image.h>
#include "mutil.h"
#include "renderer.h"
#include "res.h"
int w=512; int h=125;
SDL_Window *window;
SDL_Renderer *renderer;
new_pool(renderF, RenderFunction);
void renderInit(CON I W, CON I H){
   w=W;h=H;
   SDL_Init(SDL_INIT_VIDEO);
   SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer);
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   resLoadAll(renderer);
}
void render(){
   SDL_RenderClear(renderer);
   FOR( renderF_num,{ renderF_pool[i](renderer); });
   SDL_RenderPresent(renderer);
}

void drawTexture(const char *name, SDL_Rect *src, SDL_Rect *dst){
   SDL_Texture *tex = resGetTexture(name);
   if(tex){
       SDL_RenderCopy(renderer, tex, src, dst);
   }
}

void renderFree(){
   resFreeAll();
   if(renderer){
       SDL_DestroyRenderer(renderer);
       renderer = NULL;
   }
   if(window){
       SDL_DestroyWindow(window);
       window = NULL;
   }
}
