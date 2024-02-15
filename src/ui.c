#include "mutil.h"
#include "SDL.h"
#include "renderer.h"
#include "tick.c"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

typedef struct { RECT area; void (*onPress)(); void (*onUpdate)(); void (*onRender)(Uint32 *pixels); } Button;
typedef struct { RECT area; void (*onPress)(); Button *buttons; int numButtons; } UiHandler;
UiHandler *ui=NULL;
int uiN = 0;
void uiTick() {
   if (ui == NULL) {return;}
   FORY(uiN, { FORX(ui[y].numButtons , {
      if (ui[y].buttons == NULL || ui[y].buttons[x].onUpdate == NULL) {continue;}
      ui[y].buttons[x].onUpdate(); 
   }); });
}

void uiRender(Uint32 *p) {
   if (ui == NULL) {return;}
   FORY(uiN, { FORX(ui[y].numButtons , {
      if (ui[y].buttons == NULL || ui[y].buttons[x].onRender== NULL) {continue;}
      ui[y].buttons[x].onRender(p); 
   }); });
}

V uiDestroy(){
   //loop and free all buttons and handlers
}
V uiDestroyhandler(int index){
   //destroy a specific handler
}
I initUiHandler(RECT r){
    if (ui == NULL) {
        addTickFunction(uiTick);
        addRenderFunction(uiRender); 
        ui = (UiHandler *)malloc(sizeof(UiHandler)); // Allocate memory for ui
    }
    uiN++;
    ui = (UiHandler *)realloc(ui, uiN * sizeof(UiHandler));
    ui[uiN-1].area = r; 
    ui[uiN-1].buttons = NULL; 
    ui[uiN-1].numButtons = 0; 
    return uiN-1;
}

I addButton(UiHandler *uh, Button *b) {
   uh->buttons = (Button *)realloc(uh->buttons, (uh->numButtons + 1) * sizeof(Button));
   if (uh->buttons != NULL) {
       uh->buttons[uh->numButtons] = *b;
       uh->numButtons++;
       return uh->numButtons;
   } else { return -1; }
}

Button *newButton(RECT r, void (*onPress)(), void (*onUpdate)(), void (*onRender)()) {
    Button *b = (Button *)malloc(sizeof(Button));
    b->area = r; b->onPress = onPress; b->onUpdate = onUpdate; b->onRender = onRender;
    return b;
}

V onPressFunction() { printf("Button pressed!\n"); }
V onUpdateFunction() { printf("Button updated!\n"); }
V onRenderFunction(Uint32 *pixels) { printf("Button rendered!\n"); }



