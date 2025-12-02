
#include "mutilSDL.h"
#include "mutil.h"
#include "renderer.h"
#include "tick.h"
#include "keys.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <SDL.h>
#include <string.h>
#include <stdarg.h>

UiHandler *ui = NULL;
I uiN = 0;

typedef struct {
    Elem base;
    const C* label;
} ButtonElem;

typedef struct {
    Elem base;
    const C* label;
    F *value;
    F min;
    F max;
    B dragging;
} SliderElem;

typedef struct {
    Elem base;
    const C* text;
} TextboxElem;

static F clampf(F v, F min, F max) {
    if (v < min) { return min; }
    if (v > max) { return max; }
    return v;
}


V uiTick(D dt) {
   (void)dt;
   if (ui == NULL) { return; }
   
   // Clamp mouse coordinates to prevent segfault
   I clampedX = mpos.x;
   I clampedY = mpos.y;
   if (clampedX < 0) clampedX = 0;
   if (clampedY < 0) clampedY = 0;
   if (clampedX >= WINW) clampedX = WINW - 1;
   if (clampedY >= WINH) clampedY = WINH - 1;
   
   POINT clampedPos = {clampedX, clampedY};
   
   FORY(uiN, { 
      FORX(ui[y].numElems, {
         if (ui[y].elems == NULL || ui[y].elems[x] == NULL) { continue; }
         if (ui[y].elems[x]->onUpdate == NULL) { continue; }
         ui[y].elems[x]->onUpdate(ui[y].elems[x]);

         if (INSQ(clampedPos, ui[y].elems[x]->area) && Held(INP_ENTER)) { 
            ui[y].elems[x]->onPress(ui[y].elems[x]); 
         }
         if (INSQ(clampedPos, ui[y].elems[x]->area) && Pressed(INP_CLICK)) { 
            ui[y].elems[x]->onPress(ui[y].elems[x]); 
         }
      });
   });
}
V uiRender(SDL_Renderer *r) {
   if (ui == NULL) { return; }
   FORY(uiN, { 
      FORX(ui[y].numElems, {
         if (ui[y].elems == NULL || ui[y].elems[x] == NULL) { continue; }
         if (ui[y].elems[x]->onRender == NULL) { continue; }
         ui[y].elems[x]->onRender(ui[y].elems[x], r);
      });
   });
}

V uiDestroy() { 
   if (ui == NULL) { return; }
   
   FORY(uiN, {
      FORX(ui[y].numElems, {
         if (ui[y].elems[x] != NULL) {
            free(ui[y].elems[x]);
            ui[y].elems[x] = NULL;
         }
      });
      if (ui[y].elems != NULL) {
         free(ui[y].elems);
         ui[y].elems = NULL;
      }
      ui[y].numElems = 0;
   });
   
   free(ui);
   ui = NULL;
   uiN = 0;
}

V uiDestroyHandler(int index) {
   if (ui == NULL || index < 0 || index >= uiN) { return; }
   
   FORX(ui[index].numElems, {
      if (ui[index].elems[x] != NULL) {
         free(ui[index].elems[x]);
         ui[index].elems[x] = NULL;
      }
   });
   
   if (ui[index].elems != NULL) {
      free(ui[index].elems);
      ui[index].elems = NULL;
   }
   ui[index].numElems = 0;
   
   for (int i = index; i < uiN - 1; i++) {
      ui[i] = ui[i + 1];
   }
   
   uiN--;
   ui = (UiHandler *)realloc(ui, uiN * sizeof(UiHandler));
}

I initUiHandler(RECT r) {
    if (ui == NULL) {
        tickF_add(uiTick);
        renderF_add(uiRender);
        ui = (UiHandler *)malloc(sizeof(UiHandler));
        uiN = 1;
        ui[0].area = r;
        ui[0].elems = NULL;
        ui[0].numElems = 0;
        return 0;
    }
    
    uiN++;
    ui = (UiHandler *)realloc(ui, uiN * sizeof(UiHandler));
    ui[uiN-1].area = r;
    ui[uiN-1].elems = NULL;
    ui[uiN-1].numElems = 0;
    return uiN-1;
}

I addElem(UiHandler *uh, Elem *b) {
   if (uh == NULL) { return -1; }
   
   uh->elems = (Elem **)realloc(uh->elems, (uh->numElems + 1) * sizeof(Elem *));
   if (uh->elems != NULL) {
       uh->elems[uh->numElems] = b;
       uh->numElems++;
       return uh->numElems - 1;
   } else {
       return -1;
   }
}

Elem *newElem(RECT r, void (*onPress)(Elem*), void (*onUpdate)(Elem*), void (*onRender)(Elem*, SDL_Renderer*)) {
    Elem *b = (Elem *)malloc(sizeof(Elem));
    if (b != NULL) {
        b->area = r;
        b->onPress = onPress;
        b->onUpdate = onUpdate;
        b->onRender = onRender;
    }
    return b;
}

// Helper function to convert RECT to SDL_Rect
static SDL_Rect rectToSdlRect(RECT r) {
    SDL_Rect sdlRect = {r.x, r.y, r.w, r.h};
    return sdlRect;
}

// Updated test functions for new renderer
V onPressFunction(Elem *e) {
    printf("Elem pressed! Position: (%d, %d)\n", e->area.x, e->area.y);
}

V onUpdateFunction(Elem *e) {
    // Simple hover effect - can add logic here
}

V onRenderFunction(Elem *e, SDL_Renderer *r) {
    (void)r; // We're using our renderer functions
    
    // Draw a button with different states
    SDL_Color bgColor = {100, 100, 100, 255}; // Default gray
    SDL_Color borderColor = {200, 200, 200, 255};
    
    // Change color if hovered
    if (INSQ(mpos, e->area)) {
        bgColor.r = 150;
        bgColor.g = 150;
        bgColor.b = 150;
        
        // Change color if pressed
        if (Held(INP_CLICK) && INSQ(mpos, e->area)) {
            bgColor.r = 80;
            bgColor.g = 80;
            bgColor.b = 80;
        }
    }
    
    // Draw button background
    drawRect(e->area.x, e->area.y, e->area.w, e->area.h, ANCHOR_TOP_L, bgColor);
    
    // Draw button border
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_Rect borderRect = rectToSdlRect(e->area);
    SDL_RenderDrawRect(renderer, &borderRect);
    
    // Draw texture
    drawTexture("noise_a", e->area.x + 10, e->area.y + 10, ANCHOR_TOP_L, NULL);
    
    // Draw text label
    SDL_Color textColor = {255, 255, 255, 255};
    drawText("default_font", e->area.x + e->area.w/2, e->area.y + e->area.h/2, 
             ANCHOR_CENTER, textColor, "Button");
}

// Additional UI utility functions
V drawUiRect(RECT area, SDL_Color color) {
    drawRect(area.x, area.y, area.w, area.h, ANCHOR_TOP_L, color);
}

V drawUiTexture(const C* name, RECT area, ANCHOR anchor) {
    drawTexture(name, area.x, area.y, anchor, NULL);
}

V drawUiText(const C* fontName, RECT area, ANCHOR anchor, SDL_Color color, const C* fmt, ...) {
    char buffer[MAX_DRAW_TEXT_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    drawText(fontName, area.x, area.y, anchor, color, buffer);
}

// ------------------------------------------------------------
// Button
// ------------------------------------------------------------
static V buttonRender(Elem *e, SDL_Renderer *r) {
    (void)r;
    ButtonElem *btn = (ButtonElem*)e;

    SDL_Color base = {90, 120, 200, 255};
    SDL_Color hover = {120, 150, 230, 255};
    SDL_Color pressed = {50, 80, 150, 255};
    SDL_Color border = {30, 40, 70, 255};

    B isHover = INSQ(mpos, e->area);
    B isPressed = isHover && Held(INP_CLICK);

    SDL_Color fill = base;
    if (isHover) { fill = hover; }
    if (isPressed) { fill = pressed; }

    drawRect(e->area.x, e->area.y, e->area.w, e->area.h, ANCHOR_TOP_L, fill);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_Rect borderRect = rectToSdlRect(e->area);
    SDL_RenderDrawRect(renderer, &borderRect);

    SDL_Color textColor = {255, 255, 255, 255};
    drawText("default_font", e->area.x + e->area.w / 2, e->area.y + e->area.h / 2,
             ANCHOR_CENTER, textColor, "%s", btn->label ? btn->label : "Button");
}

Elem *createButton(RECT area, const C* label, void (*onPress)(Elem*)) {
    ButtonElem *button = (ButtonElem *)malloc(sizeof(ButtonElem));
    if (button == NULL) { return NULL; }
    button->base.area = area;
    button->base.onPress = onPress;
    button->base.onUpdate = NULL;
    button->base.onRender = buttonRender;
    button->label = label;
    return (Elem*)button;
}

// ------------------------------------------------------------
// Slider
// ------------------------------------------------------------
static V sliderPress(Elem *e) {
    SliderElem *slider = (SliderElem*)e;
    slider->dragging = true;
}

static V sliderUpdate(Elem *e) {
    SliderElem *slider = (SliderElem*)e;
    if (slider->value == NULL) { return; }
    F range = slider->max - slider->min;
    if (range <= 0.0f) { range = 1.0f; }

    B inside = INSQ(mpos, e->area);
    if (Pressed(INP_CLICK) && inside) {
        slider->dragging = true;
    }
    if (!Held(INP_CLICK)) {
        slider->dragging = false;
    }

    if (slider->dragging) {
        F t = (mpos.x - e->area.x) / (F)e->area.w;
        t = clampf(t, 0.0f, 1.0f);
        *slider->value = slider->min + t * range;
    }
}

static V sliderRender(Elem *e, SDL_Renderer *r) {
    (void)r;
    SliderElem *slider = (SliderElem*)e;
    F range = slider->max - slider->min;
    if (range <= 0.0f) { range = 1.0f; }
    F current = slider->value ? *slider->value : slider->min;
    F t = clampf((current - slider->min) / range, 0.0f, 1.0f);

    SDL_Color track = {70, 70, 80, 255};
    SDL_Color trackHighlight = {90, 90, 110, 255};
    SDL_Color knob = {220, 220, 230, 255};
    SDL_Color knobPressed = {180, 180, 190, 255};
    SDL_Color border = {35, 35, 45, 255};

    B inside = INSQ(mpos, e->area);
    B pressed = inside && Held(INP_CLICK);

    SDL_Color trackColor = inside ? trackHighlight : track;
    drawRect(e->area.x, e->area.y, e->area.w, e->area.h, ANCHOR_TOP_L, trackColor);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_Rect borderRect = rectToSdlRect(e->area);
    SDL_RenderDrawRect(renderer, &borderRect);

    I knobW = 14;
    I knobH = e->area.h + 4;
    I knobX = e->area.x + (I)(t * e->area.w) - knobW / 2;
    I knobY = e->area.y - 2;
    SDL_Color knobColor = pressed ? knobPressed : knob;
    drawRect(knobX, knobY, knobW, knobH, ANCHOR_TOP_L, knobColor);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_Rect knobRect = { knobX, knobY, knobW, knobH };
    SDL_RenderDrawRect(renderer, &knobRect);

    SDL_Color textColor = {255, 255, 255, 255};
    F degrees = current;
    char textBuf[64];
    snprintf(textBuf, sizeof(textBuf), "%s|====[]===|%.0fdeg", slider->label ? slider->label : "", degrees);
    drawText("default_font", e->area.x, e->area.y - 16, ANCHOR_TOP_L, textColor, "%s", textBuf);
}

Elem *createSlider(RECT area, const C* label, F *value, F min, F max) {
    SliderElem *slider = (SliderElem *)malloc(sizeof(SliderElem));
    if (slider == NULL) { return NULL; }
    slider->base.area = area;
    slider->base.onPress = sliderPress;
    slider->base.onUpdate = sliderUpdate;
    slider->base.onRender = sliderRender;
    slider->label = label;
    slider->value = value;
    slider->min = min;
    slider->max = max;
    slider->dragging = false;
    return (Elem*)slider;
}

// ------------------------------------------------------------
// Textbox
// ------------------------------------------------------------
static V textboxRender(Elem *e, SDL_Renderer *r) {
    (void)r;
    TextboxElem *tb = (TextboxElem*)e;
    SDL_Color bg = {40, 40, 50, 230};
    SDL_Color border = {90, 90, 110, 255};
    SDL_Color textColor = {220, 220, 230, 255};

    drawRect(e->area.x, e->area.y, e->area.w, e->area.h, ANCHOR_TOP_L, bg);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_Rect borderRect = rectToSdlRect(e->area);
    SDL_RenderDrawRect(renderer, &borderRect);

    drawText("default_font", e->area.x + 8, e->area.y + e->area.h / 2,
             ANCHOR_MID_L, textColor, "%s", tb->text ? tb->text : "");
}

Elem *createTextbox(RECT area, const C* text) {
    TextboxElem *tb = (TextboxElem *)malloc(sizeof(TextboxElem));
    if (tb == NULL) { return NULL; }
    tb->base.area = area;
    tb->base.onPress = NULL;
    tb->base.onUpdate = NULL;
    tb->base.onRender = textboxRender;
    tb->text = text;
    return (Elem*)tb;
}
