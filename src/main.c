/*
  Hatari - main.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Main initialization and event handling routines.
*/

#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

#include <SDL.h>

#include "main.h"
#include "audio.h"
#include "../m68000.h"
#include "hostcall.h"
#include "input.h"
#include "joystick.h"
#include "keymap.h"
#include "platform.h"
#include "screen.h"
#include "shortcut.h"
#include "touch.h"


#define FORCE_WORKING_DIR                 /* Set default directory to cwd */


BOOL bQuitProgram=FALSE;                  /* Flag to quit program cleanly */
BOOL bUseFullscreen=FALSE;
BOOL bEmulationActive=TRUE;               /* Run emulation when started */
BOOL bAppActive = FALSE;

/* Set while the app is in the background. The emulation loop must not
 * keep running there: Android suspends rendering, so every frame is
 * wasted work, and a process that spins in the background gets killed. */
static BOOL bInBackground = FALSE;

/* -1 = use the platform default, 0/1 = forced off/on from the command line. */
static int touch_override = -1;
char szBootDiscImage[MAX_FILENAME_LENGTH] = { "" };

char szWorkingDir[MAX_FILENAME_LENGTH] = { "" };
char szCurrentDir[MAX_FILENAME_LENGTH] = { "" };

extern enum RENDERERS use_renderer;

int delta_x, delta_y, abs_delta_x, abs_delta_y;

/*-----------------------------------------------------------------------*/
/*
  Error handler
*/
void Main_SysError(char *Error,char *Title)
{
  fprintf(stderr,"%s : %s\n",Title,Error);
}


/*-----------------------------------------------------------------------*/
/*
  Bring up message(handles full-screen as well as Window)
*/
int Main_Message(char *lpText, char *lpCaption/*,unsigned int uType*/)
{
  int Ret=0;

  /* Show message */
  fprintf(stderr,"%s: %s\n", lpCaption, lpText);

  return(Ret);
}


/*-----------------------------------------------------------------------*/
/*
  Pause emulation, stop sound
*/
void Main_PauseEmulation(void)
{
  if( bEmulationActive )
  {
    Audio_EnableAudio(FALSE);
    bEmulationActive = FALSE;
  }
}

/*-----------------------------------------------------------------------*/
/*
  Start emulation
*/
void Main_UnPauseEmulation(void)
{
  if( !bEmulationActive )
  {
    Audio_EnableAudio(1);
    bEmulationActive = TRUE;
  }
}

/* ----------------------------------------------------------------------- */
/*
  Message handler
  Here we process the SDL events (keyboard, mouse, ...) and map it to
  Atari IKBD events.
*/

static void Main_HandleEvent (SDL_Event *event)
{
  switch( event->type )
   {
    case SDL_QUIT:
       bQuitProgram = TRUE;
       SDL_Quit ();
       exit (0);
       break;

    /* Android lifecycle. WILLENTERBACKGROUND arrives while we can still
     * act on it; anything touching the GPU after that point is invalid,
     * so stop the world here and pick up again on foreground. */
    case SDL_APP_WILLENTERBACKGROUND:
       bInBackground = TRUE;
       Main_PauseEmulation ();
       break;
    case SDL_APP_DIDENTERFOREGROUND:
       bInBackground = FALSE;
       Main_UnPauseEmulation ();
       break;
    case SDL_APP_LOWMEMORY:
       break;

    case SDL_WINDOWEVENT:
       /* A foldable being unfolded shows up here, as does rotation and
        * any multi-window resize. The letterbox and the touch control
        * layout both depend on the window size, so recompute them. */
       if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
           event->window.event == SDL_WINDOWEVENT_RESIZED)
          Screen_HandleResize ();
       break;

    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP:
       Touch_HandleEvent (event);
       break;

    case SDL_MOUSEMOTION:               /* Read/Update internal mouse position */
       input.motion_x += event->motion.xrel;
       input.motion_y += event->motion.yrel;
       /* Mouse coordinates are window-relative; the game's are relative
        * to the letterboxed image, which need not start at the origin. */
       input.abs_x = event->motion.x - Screen_ViewportX ();
       input.abs_y = event->motion.y - Screen_ViewportY ();
       break;
	case SDL_JOYAXISMOTION:
	   if (event->jaxis.axis <= 1)
		   joystick_motion(event->jaxis.axis, event->jaxis.value);
	   break;
	case SDL_JOYBUTTONDOWN:
	   Keymap_JoystickUpDown(event->jbutton.button, 1);
	   break;
	case SDL_JOYBUTTONUP:
	   Keymap_JoystickUpDown(event->jbutton.button, 0);
	   break;
	case SDL_JOYHATMOTION:
	   if (event->jhat.value == SDL_HAT_UP)
		   Keymap_JoystickUpDown(12, 1);
	   else if (event->jhat.value == SDL_HAT_DOWN)
		   Keymap_JoystickUpDown(13, 1);
	   else if (event->jhat.value == SDL_HAT_LEFT)
		   Keymap_JoystickUpDown(14, 1);
	   else if (event->jhat.value == SDL_HAT_RIGHT)
		   Keymap_JoystickUpDown(15, 1);
	   else {
		   int i;
		   for (i = 12; i < 16; i++)
			   Keymap_JoystickUpDown(i, 0);
	   }
	   break;
    case SDL_MOUSEBUTTONDOWN:
       Input_MousePress (event->button.button);
       break;
    case SDL_MOUSEBUTTONUP:
       Input_MouseRelease (event->button.button);
       break;
    case SDL_KEYDOWN:
       Keymap_KeyDown(&event->key.keysym);
       break;
    case SDL_KEYUP:
       Keymap_KeyUp(&event->key.keysym);
       break;
   }
}

/* ----------------------------------------------------------------------- */
/*
  Pump the event queue.

  While backgrounded this blocks instead of returning, so the 68k loop
  stops where it stands. SDL_WaitEvent sleeps rather than spinning, which
  is what keeps a backgrounded game from draining the battery or being
  killed for burning CPU.
*/
void Main_EventHandler()
{
  SDL_Event event;
  int new_abs_x, new_abs_y;

  SDL_JoystickUpdate();

  while (SDL_PollEvent (&event))
     Main_HandleEvent (&event);

  while (bInBackground && !bQuitProgram) {
     if (SDL_WaitEvent (&event))
        Main_HandleEvent (&event);
     else
        break;
  }

  input.motion_x += delta_x;
  input.motion_y += delta_y;

  new_abs_x = input.abs_x + abs_delta_x;
  if (new_abs_x > 0 && new_abs_x < screen_w)
	  input.abs_x = new_abs_x;

  new_abs_y = input.abs_y + abs_delta_y;
  if (new_abs_y > 0 && new_abs_y < screen_h)
	  input.abs_y = new_abs_y;

  Input_Update ();
}


/*-----------------------------------------------------------------------*/
/*
  Check for any passed parameters
*/
void Main_ReadParameters(int argc, char *argv[])
{
  int i;

  /* Scan for any which we can use */
  for(i=1; i<argc; i++)
  {
    if (strlen(argv[i])>0)
    {
      if (!strcmp(argv[i],"--help") || !strcmp(argv[i],"-h"))
      {
        printf("Usage:\n frontier [options]\n"
               "Where options are:\n"
               "  --help or -h          Print this help text and exit.\n"
               "  --fullscreen or -f    Try to use fullscreen mode.\n"
               "  --nosound             Disable sound (faster!).\n"
               "  --size w h            Start at specified window size.\n"
			   "  --old-renderer        Start with the old renderer.\n"
			   "  --touch               Force the on-screen touch controls on.\n"
			   "  --no-touch            Force the on-screen touch controls off.\n"
              );
        exit(0);
      }
      else if (!strcmp(argv[i],"--touch"))
      {
        touch_override = 1;
      }
      else if (!strcmp(argv[i],"--no-touch"))
      {
        touch_override = 0;
      }
      else if (!strcmp(argv[i],"--fullscreen") || !strcmp(argv[i],"-f"))
      {
        bUseFullscreen=TRUE;
      }
      else if ( !strcmp(argv[i],"--nosound") )
      {
        bDisableSound=TRUE;
      }
	  else if (!strcmp(argv[i], "--old-renderer"))
		use_renderer = R_OLD;
      else if ( !strcmp(argv[i],"--size") )
      {
	screen_h = 0;
	if (++i < argc)	screen_w = atoi (argv[i]);
	if (++i < argc)	screen_h = atoi (argv[i]);
	/* fe2 likes 1.6 aspect ratio until i fix the mouse position
	 * to 3d object position code... */
	if (screen_h == 0) screen_h = 5*screen_w/8;
      }
      else
      {
	      /* some time make it possible to read alternative
	       * names for fe2.bin from command line */
	      fprintf(stderr,"Illegal parameter: %s\n",argv[i]);
      }
    }
  }
}


/*-----------------------------------------------------------------------*/
/*
  Initialise emulation
*/
void Main_Init(void)
{
  /* Init SDL's video subsystem. Note: Audio and joystick subsystems
     will be initialized later (failures there are not fatal). */
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) < 0)
  {
    fprintf(stderr, "Could not initialize the SDL library:\n %s\n", SDL_GetError() );
    exit(-1);
  }
  
  Platform_Init();
  Screen_Init();
  Touch_Init();
  Init680x0();                  /* Init CPU emulation */
  Audio_Init();
  Keymap_Init();

  /* Command line overrides the per-platform default, and the layout
   * depends on which controls exist, so apply it before laying out. */
  if (touch_override >= 0)
    Touch_SetEnabled (touch_override);
  Screen_HandleResize ();

  if(bQuitProgram)
  {
    SDL_Quit();
    exit(-2);
  }
}


/*-----------------------------------------------------------------------*/
/*
  Un-Initialise emulation
*/
void Main_UnInit(void)
{
  Keymap_UnInit();
  Audio_UnInit();
  Touch_UnInit();
  Screen_UnInit();
  Platform_UnInit();

  /* SDL uninit: */
  SDL_Quit();
}

static Uint32 vbl_callback ()
{
	FlagException (0);
	return 20;
}

void sig_handler (int signum)
{
	if (signum == SIGSEGV) {
		printf ("Segfault! All is lost! Abandon ship!\n");
		Call_DumpDebug ();
		abort ();
	}
}

/*-----------------------------------------------------------------------*/
/*
  Main
*/
int main(int argc, char *argv[])
{
  signal (SIGSEGV, sig_handler);
	
  /* Generate random seed */
  srand( time(NULL) );

  /* Check for any passed parameters */
  Main_ReadParameters(argc, argv);

  /* Init emulator system */
  Main_Init();

  /* Switch immediately to fullscreen if user wants to */
  if( bUseFullscreen )
    Screen_ToggleFullScreen();

  SDL_AddTimer (20, &vbl_callback, NULL);
  
  /* Run emulation */
  Main_UnPauseEmulation();
  Start680x0();                 /* Start emulation */

  /* Un-init emulation system */
  Main_UnInit();

  return(0);
}


