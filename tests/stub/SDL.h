#ifndef STUB_SDL_H
#define STUB_SDL_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
typedef unsigned int Uint32; typedef int Sint32; typedef unsigned char Uint8;
typedef long long SDL_FingerID; typedef long long SDL_TouchID;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_RWops SDL_RWops;
typedef struct { int x,y,w,h; } SDL_Rect;
#define SDL_NUM_SCANCODES 512
typedef int SDL_Scancode; typedef int SDL_Keycode;
typedef struct { SDL_Scancode scancode; SDL_Keycode sym; unsigned short mod; } SDL_Keysym;
enum { SDL_FIRSTEVENT=0, SDL_FINGERDOWN=0x700, SDL_FINGERUP, SDL_FINGERMOTION };
typedef struct { Uint32 type; SDL_TouchID touchId; SDL_FingerID fingerId;
                 float x,y,dx,dy,pressure; } SDL_TouchFingerEvent;
typedef struct { Uint32 type; SDL_TouchFingerEvent tfinger; } SDL_Event;
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_RIGHT 3
#define SDL_BUTTON_MIDDLE 2
#define SDL_BLENDMODE_BLEND 1
#define SDL_BLENDMODE_NONE 0
#define SDL_HINT_TOUCH_MOUSE_EVENTS "SDL_TOUCH_MOUSE_EVENTS"
#define SDL_arraysize(a) (sizeof(a)/sizeof((a)[0]))
#define SDL_min(a,b) ((a)<(b)?(a):(b))
#define SDL_sqrt sqrt
#define SDL_sqrtf sqrtf
#define SDL_fabsf fabsf
#define SDL_strchr strchr
#define SDL_strlen strlen
#define SDL_strcmp strcmp
#define SDL_memset memset
#define SDL_malloc malloc
#define SDL_free free
/* recording stubs */
extern Uint32 stub_ticks;
static inline Uint32 SDL_GetTicks(void){ return stub_ticks; }
static inline void SDL_SetHint(const char*a,const char*b){(void)a;(void)b;}
static inline void SDL_SetRenderDrawColor(SDL_Renderer*r,int a,int b,int c,int d){(void)r;(void)a;(void)b;(void)c;(void)d;}
static inline void SDL_SetRenderDrawBlendMode(SDL_Renderer*r,int m){(void)r;(void)m;}
extern int stub_rect_count;
static inline void SDL_RenderFillRects(SDL_Renderer*r,const SDL_Rect*x,int n){(void)r;(void)x; stub_rect_count+=n;}
static inline void SDL_RenderFillRect(SDL_Renderer*r,const SDL_Rect*x){(void)r;(void)x; stub_rect_count++;}
#endif
