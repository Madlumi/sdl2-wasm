#include <SDL.h>
#include <SDL_image.h>
#include "mutil.h"
#include "renderer.h"
#include "res.h"
#include <SDL_ttf.h>
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

void drawText(const char *fontName, const char *text, int x, int y, SDL_Color c){
   TTF_Font *font = resGetFont(fontName);
   if(!font){ return; }
   SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, c);
   if(!s){ return; }
   SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
   if(!t){ SDL_FreeSurface(s); return; }
   SDL_Rect d = {x, y, s->w, s->h};
   SDL_RenderCopy(renderer, t, NULL, &d);
   SDL_DestroyTexture(t);
   SDL_FreeSurface(s);
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
