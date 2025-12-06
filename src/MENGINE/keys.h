#ifndef MKEYHANDLER
#define MKEYHANDLER
#include "mutilSDL.h"
#include <SDL_scancode.h>
#include <stddef.h>

E POINT mpos;
enum KEYMAP{
    INP_W,
    INP_S,
    INP_D,
    INP_A,
    INP_ENTER,
    INP_EXIT,
    INP_CLICK,
    INP_LCLICK,
    INP_RCLICK,
    INP_TAB,
    INP_TOTS
};



I Pressed(enum KEYMAP k);
I Held(enum KEYMAP k);
I PressedScancode(SDL_Scancode sc);
I HeldScancode(SDL_Scancode sc);
I pollTextInput(char *buf, size_t bufSize);

E I QUIT;
E I mouseWheelMoved;
V events(D dt);
V keysInit();
#endif
