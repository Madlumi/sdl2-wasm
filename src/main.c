#include <SDL.h>
#include <stdio.h>
#include <SDL2/SDL_opengl.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
int w=640,h=480;
static SDL_GLContext ctx;
static SDL_Window* window;

SDL_Renderer *renderer;
SDL_Surface *surface;
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
   /*
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
   SDL_DestroyTexture(screenTexture);*/
}
void mainLoop(){
   events();
   tick();
   SDL_GL_MakeCurrent(window, ctx);
   SDL_GetWindowSize(window, &w, &h);
   glViewport(0, 0, w, h);
   render();
   SDL_GL_SwapWindow(window);
   t++;
}
void sdlErr(){
printf("SDL initerror: %si\n",SDL_GetError());
   exit(1);
}

void init(){
   if(SDL_Init(SDL_INIT_VIDEO)<0){sdlErr();};
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   window = SDL_CreateWindow("SDL",0, 0, w, h, SDL_WINDOW_OPENGL);
   if(window == NULL){sdlErr();}
   ctx = SDL_GL_CreateContext(window);
   SDL_GL_SetSwapInterval(1);//vsync

   //magic matrix stuff
   glMatrixMode(GL_PROJECTION); 
   glLoadIdentity();
   glOrtho(-2.0, 2.0, -2.0, 2.0, -20.0, 20.0);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   //render setting
   glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS); //Z-Buffer
   glShadeModel(GL_SMOOTH);// smooth shading? GL_SMOOT : HGL_FLAT 
   //keymap init
   for(int i = 0; i < 322; i++) { // init them all to false
      KEYS[i] = false;
   }
}

int main(int argc, char* argv[]) {
   init();
   #ifdef __EMSCRIPTEN__
   emscripten_set_main_loop(mainLoop, 0, 1);//(func, fpsi[0=requestAnimationFrame], simulate_infinite_loop);
   #else
   while(running) {     
      mainLoop();
      SDL_Delay(16);
   }
   #endif 
}
