#ifndef __M_UI__
#define __M_UI__
#include "mutilSDL.h"

typedef struct _Elem{ RECT area; void (*onPress)(struct _Elem *e); void (*onUpdate)(struct _Elem *e); void (*onRender)(struct _Elem *e, Uint32 *pixels); } Elem;
typedef struct { RECT area; void (*onPress)(); Elem *elems; int numElems; } UiHandler;

E UiHandler *ui;
V uiDestroy();
V uiDestroyhandler(int index);

I initUiHandler(RECT r);
I addElem(UiHandler *uh, Elem *b);
Elem *newElem(RECT r, void (*onPress)(), void (*onUpdate)(), void (*onRender)());

V onPressFunction(Elem *e);
V onUpdateFunction(Elem *e);
V onRenderFunction(Elem *e, U32 *pixels);
#endif
