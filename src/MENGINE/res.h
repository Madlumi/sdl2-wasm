#ifndef M_RESOURCE_MANAGER
#define M_RESOURCE_MANAGER
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

typedef struct { const char *name; const char *path; SDL_Texture *tex; } ImageRes;
typedef struct { const char *name; const char *path; Mix_Chunk *chunk; } SoundRes;
typedef struct { const char *name; const char *path; TTF_Font *font; } FontRes;

void resLoadAll(SDL_Renderer *r);
SDL_Texture *resGetTexture(const char *name);
Mix_Chunk *resGetSound(const char *name);
TTF_Font *resGetFont(const char *name);
void resFreeAll();

#endif
