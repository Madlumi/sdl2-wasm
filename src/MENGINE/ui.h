#ifndef __M_UI__
#define __M_UI__
#include "mutilSDL.h"
#include "renderer.h"
#include <SDL.h>

typedef struct _Elem{
   RECT area; void (*onPress)(struct _Elem *e);
   void (*onUpdate)(struct _Elem *e);
   void (*onRender)(struct _Elem *e, SDL_Renderer *r);
} Elem;
typedef struct { RECT area; void (*onPress)(); Elem **elems; int numElems; } UiHandler;

E UiHandler *ui;
V uiDestroy();
V uiDestroyhandler(int index);

I initUiHandler(RECT r);
I addElem(UiHandler *uh, Elem *b);
Elem *newElem(RECT r, void (*onPress)(), void (*onUpdate)(), void (*onRender)());

V onPressFunction(Elem *e);
V onUpdateFunction(Elem *e);
V onRenderFunction(Elem *e, SDL_Renderer *r);
V drawUiRect(RECT area, SDL_Color color);
V drawUiTexture(const C* name, RECT area, ANCHOR anchor);
V drawUiText(const C* fontName, RECT area, ANCHOR anchor, SDL_Color color, const C* fmt, ...);

Elem *createButton(RECT area, const C* label, void (*onPress)(Elem*));
Elem *createSlider(RECT area, const C* label, F *value, F min, F max);
Elem *createTextbox(RECT area, const C* text);
#endif
