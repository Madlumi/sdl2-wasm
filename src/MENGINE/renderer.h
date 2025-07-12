#ifndef __M_RENDERER___
#define __M_RENDERER___
#include <SDL.h>
#include "mutil.h"
E I w;
E I h;
E SDL_Window *window;
E SDL_Renderer *renderer;
E I camX;
E I camY;
typedef void (*RenderFunction)(SDL_Renderer*);
new_pool_h(renderF, RenderFunction);


V renderInit(CON I W, CON I H);
V renderSetCamera(I x, I y);
V renderFree();
V render();
V drawTexture(const char *name, SDL_Rect *src, SDL_Rect *dst, B staticPos);
V drawTexturePal(const char *name, SDL_Rect *src, SDL_Rect *dst, B staticPos, const char *palName);
V drawText(const char *fontName, I x, I y, SDL_Color c, const char *fmt, ...);
#endif

