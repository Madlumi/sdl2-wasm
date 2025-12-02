#ifndef MKEYHANDLER
#define MKEYHANDLER
#include "mutilSDL.h"

E POINT mpos;
enum KEYMAP{
    INP_W,
    INP_SPACE,
    INP_S,
    INP_D,
    INP_A,
    INP_ENTER,
    INP_EXIT,
    INP_CLICK,
    INP_LCLICK,
    INP_RCLICK,
    INP_TOTS
};



I Pressed(enum KEYMAP k);
I Held(enum KEYMAP k);

E I QUIT;
E I mouseWheelMoved;
V events(D dt);
V keysInit();
#endif
