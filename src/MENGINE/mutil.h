#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef int    I;
typedef uint32_t U32;
typedef bool   B;
typedef float  F;
typedef double D;
typedef void   V;
typedef char   C;
typedef char*  S;
#define E extern
#define CON const
#define ST static
#define R return 
#define TDS typedef struct


#define SW switch
#define CA(x,y) case x: y; break;
#define BR break
#define IN(x,l,h) ((l)<=(x)&&(x)<=(h))


#define eif else if
#define el else
#define W(x) while((x))
#define FOR(n,x) {for(int i=0;i<n;++i){x;}}
#define FORX(n,a) {for(int x=0;x<n;++x){a;}}
#define FORY(n,a) {for(int y=0;y<n;++y){a;}}

#define FORR(n,x) {for(int i=n-1;i>=0;--i){x;}}
#define FORYX(yN,xN,a) {for(int y=0;y<yN;++y){for(int x=0;x<xN;++x){a;}}}
#define FOR_RYX(yN,xN,a) {for(int y=yN-1;y>=0;--y){for(int x=0;x<xN;++x){a;}}}

#define LOG(fmt, ...) printf(fmt, __VA_ARGS__); printf("\n");

#define POOLMAX 10

#define new_pool_h(name, obj) \
V name##_add (obj o); 

#define new_pool(name, obj) \
   obj* name##_pool=NULL;                                   \
                                       \
                                       \
    size_t name##_max = POOLMAX; \
    size_t name##_num =  0; \
    V name##_add (obj o){ \
       if (name##_num==0) { \
         name##_pool=malloc(sizeof(obj)); \
       }\
       if (name##_num>=name##_max) { \
           size_t newSize =name##_max == 0 ? 1 : name##_max* 2; \
           obj* n = realloc(name##_pool, newSize * sizeof(obj)); \
           if (n  == NULL) { fprintf(stderr, "Memory allocation failed.\n"); return; }\
           name##_pool = n; \
           name##_max = newSize;\
       }\
         name##_pool[name##_num++] = o;\
    }\
V name##_free(){\
    free(name##_pool);\
    name##_pool = NULL;\
    name##_max= POOLMAX;\
    name##_num= 0;\
}\

