#include "res.h"
#include <string.h>
#include <stdlib.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#define IMAGE(name, path) {#name, path, NULL},
#define SOUND(name, path)
#define FONT(name, path)
static ImageRes image_list[] = {
#include "../LoadRes.h"
};
#undef IMAGE
#undef SOUND
#undef FONT

#define SOUND(name, path) {#name, path, NULL},
#define IMAGE(name, path)
#define FONT(name, path)
static SoundRes sound_list[] = {
#include "../LoadRes.h"
};
#undef SOUND
#undef IMAGE
#undef FONT

#define FONT(name, path) {#name, path, NULL},
#define IMAGE(name, path)
#define SOUND(name, path)
static FontRes font_list[] = {
#include "../LoadRes.h"
};
#undef FONT
#undef IMAGE
#undef SOUND

static int image_count = sizeof(image_list)/sizeof(ImageRes);
static int sound_count = sizeof(sound_list)/sizeof(SoundRes);
static int font_count  = sizeof(font_list)/sizeof(FontRes);

void resLoadAll(SDL_Renderer *r){
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    for(int i=0;i<image_count;i++){
        image_list[i].tex = IMG_LoadTexture(r, image_list[i].path);
    }
    for(int i=0;i<sound_count;i++){
        sound_list[i].chunk = Mix_LoadWAV(sound_list[i].path);
    }
    for(int i=0;i<font_count;i++){
        font_list[i].font = TTF_OpenFont(font_list[i].path, 16);
    }
}

SDL_Texture *resGetTexture(const char *name){
    for(int i=0;i<image_count;i++){
        if(strcmp(image_list[i].name, name)==0){
            return image_list[i].tex;
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
