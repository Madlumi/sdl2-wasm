#include <SDL.h>
#include <alloca.h>
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL_opengl.h>
#include <strings.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include "SDL_opengles2.h"
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
bool KEYS[99322];

static unsigned int compileShader(unsigned int type, const char* src){
   unsigned int id = glCreateShader(type);
   glShaderSource(id, 1, &src, NULL);
   glCompileShader(id);
   int res;
   glGetShaderiv(id, GL_COMPILE_STATUS,&res);
   if(!res){
      int l;
      glGetShaderiv(id,GL_INFO_LOG_LENGTH, &l);
      char* msg = alloca(l * sizeof(char));
      glGetShaderInfoLog(id,l,&l,msg);
      printf("Shader Compile<%s> error:\n<----------\n %s \n----------->\n",(type == GL_VERTEX_SHADER ? "vert" : "frag") ,msg);
      
   glDeleteShader(id);
      return 0;;
   }
   return id;
}
static unsigned int mkShader(const char* vss,const  char* fss){
   unsigned int program=glCreateProgram();
   unsigned int vs = compileShader(GL_VERTEX_SHADER, vss);
   unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fss);
   glAttachShader(program,vs);
   glAttachShader(program,fs);
   glLinkProgram(program);
   glValidateProgram(program);
   glDeleteShader(vs);
   glDeleteShader(fs);

   return program;
}

void quit(){
         SDL_Quit();
         printf("quiting\n");
         running=0;
}
void events(){
   SDL_Event e;
   while (SDL_PollEvent(&e)) {
      /*if (e.type==SDL_KEYDOWN){
         KEYS[e.key.keysym.sym] = true;
      }else if (e.type == SDL_KEYUP){
         if(e.key.keysym.sym!=NULL)KEYS[e.key.keysym.sym] = false;
      }else*/ if (e.type == SDL_QUIT){
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
   
   //resizeable, and or vsync stuff, idk
   SDL_GL_MakeCurrent(window, ctx);
   SDL_GetWindowSize(window, &w, &h);
   glViewport(0, 0, w, h);



   glDrawArrays(GL_TRIANGLES, 0, 3);


    //glClearColor(0.1, 0.01, 0.05, 1.0);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
   
    //glMatrixMode(GL_MODELVIEW);
    //glRotatef(3.0, 3.0, 1.0, 1.0);
   
   SDL_GL_SwapWindow(window);
   /* Soft renderer
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
   render();
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
  /*
   glMatrixMode(GL_PROJECTION); 
   glLoadIdentity();
   glOrtho(.0, .0, 10.0, 10.0, -20.0, 20.0);//clipping plane?
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   */
   //render setting
   //glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS); //Z-Buffer
   //glShadeModel(GL_SMOOTH);// smooth shading? GL_SMOOT : HGL_FLAT 
   //keymap init




   //data, remove from loop...
   static float pos[6]={ -.5f,-.5f,.0f,.5f,.5f,-.5f}; //vertex pos x1,y1,x2...
   unsigned int buff;
   glGenBuffers(1, &buff);
   glBindBuffer(GL_ARRAY_BUFFER, buff);//->Draws array
   glBufferData(GL_ARRAY_BUFFER,6*sizeof(float), pos, GL_DYNAMIC_DRAW);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*2,0);

   unsigned int shader=mkShader(
      "attribute vec4 a_position;\n"
   "void main() {\n"
   "    gl_Position = a_position;\n"
   " }\n"
      ,
   " precision mediump float;\n"
   "  \n"
   " void main() {\n"
   "    gl_FragColor =vec4(1.0,.0,.5,1.0);\n"
   " }\n");

   /*"#version 330 core\n"
   "layout(location = 0) in vec4 position;\n"
   "attribute vec4 position;    \n"
   "void main(){\n"
   "  gl_Position = position; \n"
   "}\n"
    , 
   "#version 330 core\n"
   "layout(location = 0) out vec4 color;\n"
      
   "void main(){\n"
   "  color=vec4(1.0,.0,.5,1.0);\n"
   "}\n"
   );*/
   glUseProgram(shader);
   //..........



   printf("%s\n",glGetString(GL_VERSION));
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
