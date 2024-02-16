#include "mutilSDL.h"
#include "SDL.h"
#include "renderer.h"
#include "tick.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
typedef struct _Elem{ RECT area; void (*onPress)(struct _Elem *e); void (*onUpdate)(struct _Elem *e); void (*onRender)(struct _Elem *e, Uint32 *pixels); } Elem;
typedef struct { RECT area; void (*onPress)(); Elem *elems; int numElems; } UiHandler;

UiHandler *ui=NULL;
I uiN = 0;

V uiTick() {
   if (ui == NULL) {return;}
   FORY(uiN, { FORX(ui[y].numElems , {
      if (ui[y].elems == NULL || ui[y].elems[x].onUpdate == NULL) {continue;}
      ui[y].elems[x].onUpdate(&ui[y].elems[x]); 
   }); });
}
V uiRender(Uint32 *p) {
   if (ui == NULL) {return;}
   FORY(uiN, { FORX(ui[y].numElems , {
      if (ui[y].elems == NULL || ui[y].elems[x].onRender== NULL) {continue;}
      ui[y].elems[x].onRender(&ui[y].elems[x],p); 
   }); });
}

V uiDestroy(){ /*loop and free all elems and handlers*/ }
V uiDestroyhandler(int index){ /*destroy a specific handler*/ }

I initUiHandler(RECT r){
    if (ui == NULL) {
        addTickFunction(uiTick);
        addRenderFunction(uiRender); 
        ui = (UiHandler *)malloc(sizeof(UiHandler)); // Allocate memory for ui
    }
    uiN++;
    ui = (UiHandler *)realloc(ui, uiN * sizeof(UiHandler));
    ui[uiN-1].area = r; ui[uiN-1].elems = NULL; ui[uiN-1].numElems = 0; 
    return uiN-1;
}

I addElem(UiHandler *uh, Elem *b) {
   uh->elems = (Elem *)realloc(uh->elems, (uh->numElems + 1) * sizeof(Elem));
   if (uh->elems != NULL) {
       uh->elems[uh->numElems] = *b;
       uh->numElems++;
       return uh->numElems;
   } else { return -1; }
}

Elem *newElem(RECT r, void (*onPress)(), void (*onUpdate)(), void (*onRender)()) {
    Elem *b = (Elem *)malloc(sizeof(Elem));
    b->area = r; b->onPress = onPress; b->onUpdate = onUpdate; b->onRender = onRender;
    return b;
}

//test functions
V onPressFunction(Elem *e) { printf("Elem pressed!\n"); }
V onUpdateFunction(Elem *e) { printf("Elem updated!\n"); }
V onRenderFunction(Elem *e, U32 *pixels) { 
   printf("%d\n",e->area.w);
   FORYX(e->area.h, e->area.w, { pixels[(x+e->area.x)+(y+ e->area.y)*w]=0xFF00FF00; })
}



