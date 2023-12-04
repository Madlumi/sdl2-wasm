#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <stdbool.h>
#include <stdlib.h>

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Surface *surface;
#define w 512
#define h 512
int running=1;
int t = 0;
bool KEYS[322];
void quit(){
         SDL_Quit();
         printf("quiting\n");
         running=0;
}
void events(){
   SDL_Event e;
   while (SDL_PollEvent(&e)) {
      if (e.type==SDL_KEYDOWN){
         KEYS[e.key.keysym.sym] = true;
      }else if (e.type == SDL_KEYUP){
         KEYS[e.key.keysym.sym] = false;
      }else if (e.type == SDL_QUIT){
         quit();
      }
   }
   if(KEYS[SDLK_q]) {
      quit();  
   }
}
void tick(){
}
void render(){
   if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
   Uint8 * pixels = surface->pixels;
   for (int i=0; i < w*h*4; i++) {
      pixels[i] = (i+t) % 255 *(i % 4);
   }
   if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
   
   SDL_Texture *screenTexture = SDL_CreateTextureFromSurface(renderer, surface);

   SDL_RenderClear(renderer);
   SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
   SDL_RenderPresent(renderer);
   SDL_DestroyTexture(screenTexture);
}
void mainLoop(){
   events();
   tick();
   render();
   t++;
}
int main(int argc, char* argv[]) {
   SDL_Init(SDL_INIT_VIDEO);
   SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer);
   surface = SDL_CreateRGBSurface(0, w , h, 32, 0, 0, 0, 0);

   for(int i = 0; i < 322; i++) { // init them all to false
    KEYS[i] = false;
}
#ifdef __EMSCRIPTEN__
   emscripten_set_main_loop(mainLoop, 0, 1);
#else
   while(running) {        
      mainLoop();
      SDL_Delay(16);
   }
#endif 
}
