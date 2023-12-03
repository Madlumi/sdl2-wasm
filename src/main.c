#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <stdlib.h>

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Surface *surface;
#define w 512
#define h 512
int t = 0;
void tick(){
}
void render(){
    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

    Uint8 * pixels = surface->pixels;
    
    for (int i=0; i < w*h*4; i++) {
        char randomByte =  (i+t) % 255 *(i % 4);
        pixels[i] = randomByte;
    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

    SDL_Texture *screenTexture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(screenTexture);
}
void mainLoop(){
   tick();
   render();
   t++;
}
void drawRandomPixels() {
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer);
    surface = SDL_CreateRGBSurface(0, w , h, 32, 0, 0, 0, 0);
    
    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
    #else
    while(1) {        
        mainLoop();
        SDL_Delay(16);
    }
    #endif 
}
