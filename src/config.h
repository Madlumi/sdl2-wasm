#pragma once

#include <SDL.h>

#define DEFAULT_TOP_BG_IMAGE "res/textures/island.png"

typedef struct {
    char prompt[16];
    float topPercentage;
    char backgroundImage[128];
    SDL_Color topBgColor;
    SDL_Color termBgColor;
    SDL_Color textColorUser;
    SDL_Color textColorAI;
    SDL_Color promptColor;
    SDL_Color cursorColor;
    SDL_Color selectionColor;
    SDL_Color overflowTint;
    char aiType[32];
    char model[64];
    char systemFile[128];
    char systemPrompt[256];
} TerminalConfig;

extern TerminalConfig gConfig;

void configInit(void);
