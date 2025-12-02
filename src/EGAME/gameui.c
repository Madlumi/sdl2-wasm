#include "gameui.h"
#include "../MENGINE/renderer.h"

void gameuiRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color white = {255, 255, 255, 255};
    drawText("default_font", 10, 10, ANCHOR_TOP_L, white,
             "WASD to move. Avoid water and walls.");
    drawText("default_font", WINW - 10, 10, ANCHOR_TOP_R, white,
             "Zoom %.1fx", ZOOM);
}
