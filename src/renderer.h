#ifndef __M_RENDERER___
#define __M_RENDERER___
#include <SDL.h>
#include "mutil.h"
E I w; 
E I h;
E SDL_Window *window; E SDL_Renderer *renderer; E SDL_Surface *surface;
typedef void (*RenderFunction)(Uint32*);
new_pool_h(renderF, RenderFunction);


V renderInit(CON I W, CON I H);

V renderFree();
V render();
#endif

