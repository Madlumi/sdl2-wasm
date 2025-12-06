#include "SDL_rect.h"
#include "SDL_scancode.h"
#include <SDL.h>
#include <stdio.h>
#include <sys/ucontext.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "tick.h"
#define IN(x,l,h) ((l)<=(x)&&(x)<=(h))
#include "mutil.h"
#include "keys.h"

#define PRESS_DELAY 10
#define mkeyn 24
#define keyn 512
#define keyt keyn+mkeyn

I KEYS[keyt];
SDL_Point mpos = {0,0};
I QUIT=0;
I mouseWheelMoved=0;
static char textEvents[32][SDL_TEXTINPUTEVENT_TEXT_SIZE];
static int textEventHead = 0;
static int textEventTail = 0;

#define  MAX_KEYBINDS 2
I keyBinds[INP_TOTS][MAX_KEYBINDS] = {
   [INP_W]     = { SDL_SCANCODE_W, SDL_SCANCODE_UP },
   [INP_S]     = { SDL_SCANCODE_S, SDL_SCANCODE_DOWN },
   [INP_D]     = { SDL_SCANCODE_D, SDL_SCANCODE_RIGHT },
   [INP_A]     = { SDL_SCANCODE_A, SDL_SCANCODE_LEFT },
   [INP_ENTER] = { SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER },
   [INP_EXIT]  = { SDL_SCANCODE_ESCAPE, -1 },
   [INP_CLICK] = { SDL_BUTTON_LEFT+keyn, SDL_BUTTON_RIGHT+keyn },
   [INP_LCLICK] = { SDL_BUTTON_LEFT+keyn, SDL_SCANCODE_SPACE },
   [INP_RCLICK] = { SDL_BUTTON_RIGHT+keyn, -1 },
   [INP_TAB]   = { SDL_SCANCODE_TAB, -1 }
};

I Pressed(enum KEYMAP k) {
   FOR(MAX_KEYBINDS,{ if (keyBinds[k][i] != -1 && KEYS[keyBinds[k][i]]==PRESS_DELAY-1) return 1; });
   return 0;
}

I Held(enum KEYMAP k) {
   FOR(MAX_KEYBINDS,{ if (keyBinds[k][i] != -1 && KEYS[keyBinds[k][i]]) return 1; });
   return 0;
}

I PressedScancode(SDL_Scancode sc) {
   if (!IN(sc, 0, keyn - 1)) return 0;
   return KEYS[sc] == PRESS_DELAY - 1;
}

I HeldScancode(SDL_Scancode sc) {
   if (!IN(sc, 0, keyn - 1)) return 0;
   return KEYS[sc] > 0;
}

I pollTextInput(char *buf, size_t bufSize) {
   if (!buf || bufSize == 0) return 0;
   if (textEventHead == textEventTail) return 0;

   strncpy(buf, textEvents[textEventHead], bufSize - 1);
   buf[bufSize - 1] = '\0';
   textEventHead = (textEventHead + 1) % 32;
   return 1;
}

static void pushTextEvent(const char *text) {
   if (!text) return;
   int nextTail = (textEventTail + 1) % 32;
   if (nextTail == textEventHead) {
      textEventHead = (textEventHead + 1) % 32;
   }
   strncpy(textEvents[textEventTail], text, SDL_TEXTINPUTEVENT_TEXT_SIZE - 1);
   textEvents[textEventTail][SDL_TEXTINPUTEVENT_TEXT_SIZE - 1] = '\0';
   textEventTail = nextTail;
}

void RemapKey(enum KEYMAP k, int newKey, int bindIndex) {
   if (bindIndex < MAX_KEYBINDS) {
      keyBinds[k][bindIndex] = newKey;
   }
}

V events(D dt){
   (void)dt;
   #define sc e.key.keysym.scancode
   #define bc e.button.button
   SDL_Event e;
   SDL_GetMouseState(&mpos.x, &mpos.y);
   for(int i = 0; i < keyt; i++){if(KEYS[i]>1){KEYS[i]--;}}
   while(SDL_PollEvent(&e)) {
      if      (e.type == SDL_KEYDOWN){          if(!IN(sc,0,keyn-1 )){LOG("key: %d", sc );R;}KEYS[sc]=(KEYS[sc]>0) ?  2 : PRESS_DELAY;}
      else if (e.type == SDL_KEYUP){            if(!IN(sc,0,keyn-1 )){LOG("key: %d", sc );R;}KEYS[sc] = 0;}
      else if (e.type == SDL_MOUSEBUTTONDOWN){  if(!IN(bc,0,mkeyn-1)){LOG("key: %d", bc );R;}KEYS[bc+keyn]=(KEYS[bc+keyn]>0) ? 2 : PRESS_DELAY;}
      else if (e.type == SDL_MOUSEBUTTONUP){    if(!IN(bc,0,mkeyn-1)){LOG("key: %d", bc );R;}KEYS[bc+keyn]=0;}
      else if (e.type == SDL_MOUSEWHEEL) {      mouseWheelMoved+=e.wheel.y ; }
      else if (e.type == SDL_TEXTINPUT) {       pushTextEvent(e.text.text); }


      else if (e.type == SDL_QUIT){ QUIT=1; }
   }
#ifndef __EMSCRIPTEN__
   if(KEYS[SDL_SCANCODE_Q]) { QUIT=1; }
#endif
   #undef sc
   #undef bc
}

V keysInit(){
   for(I i = 0; i < keyt; i++) {
      KEYS[i] = 0;
   }
   SDL_StartTextInput();
}
