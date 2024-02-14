#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include "SDL_opengles2.h"
#endif
#include <SDL.h>
#include <SDL2/SDL_opengl.h>
#include "renderer.h"

void sdlErr(){ printf("SDL initerror: %si\n",SDL_GetError()); exit(1); }
static SDL_GLContext ctx;
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
void renderr(){
   //resizeable, and or vsync stuff, idk
   SDL_GL_MakeCurrent(window, ctx);
   SDL_GetWindowSize(window, &w, &h);
   glViewport(0, 0, w, h);
   glDrawArrays(GL_TRIANGLES, 0, 3);
   glClearColor(0.1, 0.01, 0.05, 1.0);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   SDL_GL_SwapWindow(window);
}

void initOGL(){
   if(SDL_Init(SDL_INIT_VIDEO)<0){sdlErr();};
   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   window = SDL_CreateWindow("SDL",0, 0, w, h, SDL_WINDOW_OPENGL);
   if(window == NULL){sdlErr();}
   ctx = SDL_GL_CreateContext(window);
   SDL_GL_SetSwapInterval(1);//vsync

   static float pos[6]={ -.5f,-.5f,.0f,.5f,.5f,-.5f}; //vertex pos x1,y1,x2...
   unsigned int buff;
   glGenBuffers(1, &buff);
   glBindBuffer(GL_ARRAY_BUFFER, buff);//->Draws array
   glBufferData(GL_ARRAY_BUFFER,6*sizeof(float), pos, GL_STATIC_DRAW);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*2,0);

   //maybe usefull stuff ? https://webglfundamentals.org/webgl/lessons/webgl-shaders-and-glsl.html 
  // char *vs=  getShaderff("VERT","res/shaders/test.shader");
  // char *fs=  getShaderff("FRAG","res/shaders/test.shader");
   unsigned int shader=mkShader(
"attribute vec4 a_position;\n"
"void main() {\n"
"    gl_Position = a_position;\n"
"}\n"
      ,
"void main() {\n"
"   gl_FragColor =vec4(1.0,.0,.5,1.0);\n"
"}\n"
   );
   //unsigned int shader=mkShader(vs,fs);
   //printf("%s\n----\n%s",vs,fs);
   //free(vs);
   //ree(fs);
   glUseProgram(shader);
   //..........




   //glDeleteProgram(shader);
   printf("%s\n",glGetString(GL_VERSION));
}

char * getShaderff(const char * type, const char* file){
   FILE * iFile = fopen((char *)file, "r");
   
   FILE *stream;
   char *buf;
   size_t slen;
   off_t eob;
   stream = open_memstream (&buf, &slen);
   if (stream == NULL){printf("something bad happen to shader loader :(");}
   
   size_t len;
   char *line;
   int read=0;
   printf("%s\n",type);
   while (-1 != getline(&line, &len, iFile)) {
      if(read ){      
         if(strstr(line,"#!")!=NULL){break;} 
         printf(">%s",line);
         fprintf (stream, line);
      }if(strstr(line,type)!=NULL){read=1;} 
   }
   fclose (stream);
   printf("<---\n %s\n --->\n",buf);
   return buf;
}
