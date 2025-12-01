#include "res.h"
#include <string.h>
#include <stdlib.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#define IMAGE(name, path) {#name, path, NULL, NULL},
#define SOUND(name, path)
#define FONT(name, path)
#define PALET(name, c0, c1, c2, c3)
static ImageRes image_list[] = {
#include "../LoadRes.h"
};
#undef IMAGE
#undef SOUND
#undef FONT
#undef PALET

#define SOUND(name, path) {#name, path, NULL},
#define IMAGE(name, path)
#define FONT(name, path)
#define PALET(name, c0, c1, c2, c3)
static SoundRes sound_list[] = {
#include "../LoadRes.h"
};
#undef SOUND
#undef IMAGE
#undef FONT
#undef PALET

#define FONT(name, path) {#name, path, NULL},
#define IMAGE(name, path)
#define SOUND(name, path)
#define PALET(name, c0, c1, c2, c3)
static FontRes font_list[] = {
#include "../LoadRes.h"
};
#undef FONT
#undef IMAGE
#undef SOUND
#undef PALET

#define PALET(name, c0, c1, c2, c3) {#name, {c0, c1, c2, c3}, 4},
#define IMAGE(name, path)
#define SOUND(name, path)
#define FONT(name, path)
static PalRes pal_list[] = {
#include "../LoadRes.h"
};
#undef PALET
#undef IMAGE
#undef SOUND
#undef FONT

static int pal_count = sizeof(pal_list)/sizeof(PalRes);

static int image_count = sizeof(image_list)/sizeof(ImageRes);
static int sound_count = sizeof(sound_list)/sizeof(SoundRes);
static int font_count  = sizeof(font_list)/sizeof(FontRes);



void resLoadAll(SDL_Renderer *r){
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    
    printf("Loading %d images...\n", image_count);
    for(int i=0;i<image_count;i++){
        printf("Loading image: %s from %s\n", image_list[i].name, image_list[i].path);
        image_list[i].surface = IMG_Load(image_list[i].path);
        if(image_list[i].surface) {
            printf("  Success! Dimensions: %dx%d\n", image_list[i].surface->w, image_list[i].surface->h);
            if (r) {
                image_list[i].tex = SDL_CreateTextureFromSurface(r, image_list[i].surface);
                if (!image_list[i].tex) {
                    printf("  Failed to create texture: %s\n", SDL_GetError());
                }
            }
        } else {
            printf("  Failed to load image: %s\n", IMG_GetError());
        }
    }
    
    printf("Loading %d sounds...\n", sound_count);
    for(int i=0;i<sound_count;i++){
        printf("Loading sound: %s from %s\n", sound_list[i].name, sound_list[i].path);
        sound_list[i].chunk = Mix_LoadWAV(sound_list[i].path);
        if (!sound_list[i].chunk) {
            printf("  Failed to load sound: %s\n", Mix_GetError());
        }
    }
    
    printf("Loading %d fonts...\n", font_count);
    for(int i=0;i<font_count;i++){
        printf("Loading font: %s from %s\n", font_list[i].name, font_list[i].path);
        font_list[i].font = TTF_OpenFont(font_list[i].path, 16);
        if (!font_list[i].font) {
            printf("  Failed to load font: %s\n", TTF_GetError());
        }
    }
    
    printf("Resource loading complete.\n");
}






SDL_Texture *resGetTexture(const char *name){
    for(int i=0;i<image_count;i++){
        if(strcmp(image_list[i].name, name)==0){
            return image_list[i].tex;
        }
    }
    return NULL;
}

SDL_Surface *resGetSurface(const char *name){
    for(int i=0;i<image_count;i++){
        if(strcmp(image_list[i].name, name)==0){
            return image_list[i].surface;
        }
    }
    return NULL;
}

PalRes *resGetPalette(const char *name){
    for(int i=0;i<pal_count;i++){
        if(strcmp(pal_list[i].name, name)==0){
            return &pal_list[i];
        }
    }
    return NULL;
}

Mix_Chunk *resGetSound(const char *name){
    for(int i=0;i<sound_count;i++){
        if(strcmp(sound_list[i].name, name)==0){
            return sound_list[i].chunk;
        }
    }
    return NULL;
}

TTF_Font *resGetFont(const char *name){
    for(int i=0;i<font_count;i++){
        if(strcmp(font_list[i].name, name)==0){
            return font_list[i].font;
        }
    }
    return NULL;
}

void resFreeAll(){
    for(int i=0;i<image_count;i++){
        if(image_list[i].tex) SDL_DestroyTexture(image_list[i].tex);
        if(image_list[i].surface) SDL_FreeSurface(image_list[i].surface);
    }
    for(int i=0;i<sound_count;i++){
        if(sound_list[i].chunk) Mix_FreeChunk(sound_list[i].chunk);
    }
    for(int i=0;i<font_count;i++){
        if(font_list[i].font) TTF_CloseFont(font_list[i].font);
    }
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
}
