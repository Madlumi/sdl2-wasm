#ifndef __M_RENDERER___
#define __M_RENDERER___
#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "mutil.h"
E I w; 
E I h;
E SDL_Window *window; E SDL_Renderer *renderer; E SDL_Surface *surface;
typedef void (*RenderFunction)(Uint32*);

V addRenderFunction(RenderFunction func);
V renderInit(CON I W, CON I H);

V renderFree();
V render();
#endif

