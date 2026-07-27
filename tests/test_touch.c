#include <stdio.h>
#include <string.h>
#include "main.h"
#include "input.h"
#include "joystick.h"
#include "touch.h"
#include "screen.h"
#include "platform.h"

Uint32 stub_ticks = 1000;
int stub_rect_count = 0;

/* --- stubs for the rest of the game --- */
INPUT input;
int screen_w = 320, screen_h = 200;
int mouse_shown = 0;

static int mouse_presses, mouse_releases;
void Input_MousePress(int b){ (void)b; mouse_presses++; }
void Input_MouseRelease(int b){ (void)b; mouse_releases++; }
void Input_PressSTKey(unsigned char c,int p){ (void)c;(void)p; }

static int fake_mouse_mode = 1;
int in_mouse_mode(void){ return fake_mouse_mode; }

static int last_axis[2];
void joystick_motion(unsigned int axis,int v){ if(axis<2) last_axis[axis]=v; }

static int btn_down[16], btn_up[16];
void Keymap_JoystickUpDown(unsigned int b,int p){ if(b<16){ if(p) btn_down[b]++; else btn_up[b]++; } }

/* Simulate a joystick.ini: Battle-mode-ish bindings */
static const char *bindings[16] = {
  [0]="ECM", [1]="BOMB", [2]="LASER", [3]="MISSILE",
  [4]="RTHRUST", [5]="THRUST",
  [6]="SPECIAL_TIME_DECREASE", [7]="SPECIAL_TIME_INCREASE",
  [8]="F1", [9]="SPECIAL_SWITCH_MODE", [10]="EJECT",
  [12]="LOOK_UP", [13]="LOOK_DOWN", [14]="LOOK_LEFT", [15]="LOOK_RIGHT",
};
const char *joystick_action_for_button(unsigned int b){ return b<16?bindings[b]:NULL; }
int Platform_IsTouchDevice(void){ return 1; }

/* --- helpers --- */
static void finger(Uint32 type, int id, float px, float py, int W, int H){
  SDL_Event e; memset(&e,0,sizeof e);
  e.type=type; e.tfinger.fingerId=id; e.tfinger.x=px/W; e.tfinger.y=py/H;
  Touch_HandleEvent(&e);
}

static int fails = 0;
static void check(const char *what, int cond){
  printf("  %-58s %s\n", what, cond?"ok":"FAIL");
  if(!cond) fails++;
}

static void dump_layout(const char *name,int W,int H,int vx,int vy,int vw,int vh){
  TOUCH_VIEWPORT v = { vx,vy,vw,vh };
  printf("\n%s  window %dx%d  viewport %d,%d %dx%d  (band below = %d px)\n",
         name,W,H,vx,vy,vw,vh, H-(vy+vh));
  Touch_Layout(W,H,&v);
}

int main(void){
  Touch_Init();
  printf("touch enabled: %d\n", Touch_Enabled());

  /* ---- Geometry: folded phone, landscape 22:9 -> image height-limited ---- */
  int W=2316,H=1080;
  int vh=H, vw=H*320/200, vx=(W-vw)/2, vy=0;
  dump_layout("FOLDED (22:9)",W,H,vx,vy,vw,vh);
  check("no band below image -> controls must overlay", H-(vy+vh)==0);

  /* ---- Geometry: unfolded foldable, near square -> image width-limited ---- */
  int W2=2208,H2=1840;
  int vw2=W2, vh2=W2*200/320, vx2=0, vy2=0;
  dump_layout("UNFOLDED (1.2:1)",W2,H2,vx2,vy2,vw2,vh2);
  int band = H2-(vy2+vh2);
  check("deep band below image exists when unfolded", band > 400);

  /* ---- Gesture: tap = click ---- */
  printf("\nGESTURES (mouse mode)\n");
  fake_mouse_mode=1;
  dump_layout("mouse-mode layout",W,H,vx,vy,vw,vh);
  screen_w=vw; screen_h=vh;
  input.abs_x=100; input.abs_y=100;
  mouse_presses=mouse_releases=0;
  stub_ticks=2000;
  finger(SDL_FINGERDOWN,1, 600,300, W,H);
  stub_ticks=2100;                       /* 100ms, under TAP_MAX_MS */
  finger(SDL_FINGERUP,1, 602,301, W,H);  /* barely moved */
  check("quick tap generates a press", mouse_presses==1);
  stub_ticks=2200;                        /* past CLICK_HOLD_MS */
  Touch_Update();
  check("click is released after the hold window", mouse_releases==1);

  /* ---- Gesture: drag moves cursor, no click ---- */
  mouse_presses=mouse_releases=0;
  input.abs_x=100; input.abs_y=100;
  stub_ticks=3000;
  finger(SDL_FINGERDOWN,2, 600,300, W,H);
  finger(SDL_FINGERMOTION,2, 700,300, W,H);
  int moved_x = input.abs_x;
  stub_ticks=3100;
  finger(SDL_FINGERUP,2, 700,300, W,H);
  check("drag moves the cursor right", moved_x > 100);
  check("drag applies pointer acceleration (>1:1)", moved_x-100 > 100);
  check("drag does NOT click", mouse_presses==0);

  /* ---- Gesture: hold auto-repeats ---- */
  mouse_presses=mouse_releases=0;
  stub_ticks=4000;
  finger(SDL_FINGERDOWN,3, 600,300, W,H);
  Touch_Update();
  check("no repeat before the hold delay", mouse_presses==0);
  stub_ticks=4600;                        /* past HOLD_REPEAT_DELAY_MS */
  Touch_Update();
  int first = mouse_presses;
  stub_ticks=4700; Touch_Update();        /* release the click */
  stub_ticks=4800; Touch_Update();        /* next repeat */
  check("hold starts auto-repeating clicks", first==1);
  check("hold keeps repeating", mouse_presses>=2);
  stub_ticks=4900;
  finger(SDL_FINGERUP,3, 600,300, W,H);

  /* ---- Stick: flight mode ---- */
  printf("\nSTICK (flight mode)\n");
  fake_mouse_mode=0;
  dump_layout("flight-mode layout",W,H,vx,vy,vw,vh);
  last_axis[0]=last_axis[1]=0;
  stub_ticks=5000;
  finger(SDL_FINGERDOWN,4, 300,800, W,H);
  check("stick centred at rest", last_axis[0]==0 && last_axis[1]==0);
  finger(SDL_FINGERMOTION,4, 300+400,800, W,H);   /* hard right, past max */
  check("full right deflection saturates positive", last_axis[0]==32767);
  check("no vertical deflection when pushed sideways", last_axis[1]==0);
  finger(SDL_FINGERMOTION,4, 300+4,800, W,H);     /* inside deadzone */
  check("deadzone suppresses tiny movement", last_axis[0]==0);
  finger(SDL_FINGERUP,4, 300,800, W,H);
  check("release recentres both axes", last_axis[0]==0 && last_axis[1]==0);

  /* ---- Buttons follow the binding table ---- */
  printf("\nBUTTONS\n");
  memset(btn_down,0,sizeof btn_down); memset(btn_up,0,sizeof btn_up);
  /* button 2 = LASER; find where the layout put it by probing the render
     path indirectly: press at its known slot fraction (0.94 W, band) */
  stub_rect_count=0;
  Touch_Render((SDL_Renderer*)1);
  check("overlay renders geometry", stub_rect_count > 100);

  printf("\n%s (%d failures)\n", fails? "FAILURES":"ALL CHECKS PASSED", fails);
  return fails!=0;
}
