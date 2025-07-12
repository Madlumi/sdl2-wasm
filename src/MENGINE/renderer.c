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

void drawTexturePal(const char *name, SDL_Rect *src, SDL_Rect *dst, B staticPos, const char *palName){
    SDL_Surface *surf = resGetSurface(name);
    PalRes *pal = resGetPalette(palName);
    if(!surf || !pal){ drawTexture(name, src, dst, staticPos); return; }

    SDL_Surface *tmp = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    if(!tmp){ drawTexture(name, src, dst, staticPos); return; }
    SDL_LockSurface(tmp);
    Uint32 *pix = (Uint32 *)tmp->pixels;
    int count = (tmp->w * tmp->h);
    for(int i=0;i<count;i++){
        Uint8 r,g,b,a;
        SDL_GetRGBA(pix[i], tmp->format, &r,&g,&b,&a);
        int idx = (r + g + b) * pal->count / (3 * 256);
        if(idx >= pal->count) idx = pal->count-1;
        Uint32 col = pal->cols[idx];
        Uint8 nr = (col>>24)&0xFF;
        Uint8 ng = (col>>16)&0xFF;
        Uint8 nb = (col>>8)&0xFF;
        Uint8 na = col & 0xFF;
        pix[i] = SDL_MapRGBA(tmp->format, nr, ng, nb, (Uint8)(a*(na/255.0f)));
    }
    SDL_UnlockSurface(tmp);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, tmp);
    SDL_FreeSurface(tmp);
    if(!tex){ drawTexture(name, src, dst, staticPos); return; }
    SDL_Rect d = *dst;
    if(!staticPos){ d.x -= camX; d.y -= camY; }
    SDL_RenderCopy(renderer, tex, src, &d);
    SDL_DestroyTexture(tex);
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
