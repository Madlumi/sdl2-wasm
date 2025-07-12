#pragma once
#include "SDL_rect.h"
#include "mutil.h"
#include "SDL_stdinc.h"
#define INSQ(p, a) ( IN(p.x, a.x, a.x+a.w) && IN(p.y, a.y, a.y+a.h))

typedef Uint32 U32;
typedef SDL_Rect RECT;
typedef SDL_Point V2;
typedef SDL_Point POINT;
