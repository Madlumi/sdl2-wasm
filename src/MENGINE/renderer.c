#include <SDL.h>
#include <SDL_image.h>
#include "mutil.h"
#include "renderer.h"
#include "res.h"
#include <SDL_ttf.h>
#include <stdarg.h>
int w=512; int h=125;
SDL_Window *window;
SDL_Renderer *renderer;
int camX=0; int camY=0;
new_pool(renderF, RenderFunction);
void renderInit(CON I W, CON I H){
   w=W;h=H;
   SDL_Init(SDL_INIT_VIDEO);
   SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer);
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   resLoadAll(renderer);
}

void renderSetCamera(I x, I y){
   camX = x;
   camY = y;
}
void render(){
   SDL_RenderClear(renderer);
   FOR( renderF_num,{ renderF_pool[i](renderer); });
   SDL_RenderPresent(renderer);
}

void drawTexture(const char *name, SDL_Rect *src, SDL_Rect *dst, B staticPos){
   SDL_Texture *tex = resGetTexture(name);
   if(tex){
       SDL_Rect d = *dst;
       if(!staticPos){ d.x -= camX; d.y -= camY; }
       SDL_RenderCopy(renderer, tex, src, &d);
   }
}

void drawText(const char *fontName, I x, I y, SDL_Color c, const char *fmt, ...){
   TTF_Font *font = resGetFont(fontName);
   if(!font){ return; }
   char buf[256];
   va_list va;
   va_start(va, fmt);
   vsnprintf(buf, sizeof(buf), fmt, va);
   va_end(va);
   SDL_Surface *s = TTF_RenderUTF8_Blended(font, buf, c);
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
