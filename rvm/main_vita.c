#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <vitaGL.h>
#include <vitasdk.h>
#include "GlobalAppDefinitions.h"
#include "GraphicsSystem.h"

SceUInt32 sceUserMainThreadStackSize = 1 * 1024 * 1024;
unsigned int _newlib_heap_size_user = 96 * 1024 * 1024;

#define logprintf(f, ...) fprintf(stderr, f, __VA_ARGS__)

#define SCR_WIDTH 960
#define SCR_HEIGHT 544

static SDL_Window* gWindow;

static void initAttributes()
{
	// Setup attributes we want for the OpenGL context

	int value;

	// Don't set color bit sizes (SDL_GL_RED_SIZE, etc)
	//    Mac OS X will always use 8-8-8-8 ARGB for 32-bit screens and
	//    5-5-5 RGB for 16-bit screens

	// Request a 16-bit depth buffer (without this, there is no depth buffer)
	value = 16;
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, value);


	// Request double-buffered OpenGL
	//     The fact that windows are double-buffered on Mac OS X has no effect
	//     on OpenGL double buffering.
	value = 1;
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, value);

	// Request GL2.0 compatibility profile
	//     SDL seems to default to ES for some reason
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
}

static void printAttributes()
{
	// Print out attributes of the context we created
	int nAttr;
	int i;

	int  attr[] = { SDL_GL_RED_SIZE, SDL_GL_BLUE_SIZE, SDL_GL_GREEN_SIZE,
		SDL_GL_ALPHA_SIZE, SDL_GL_BUFFER_SIZE, SDL_GL_DEPTH_SIZE };

	char *desc[] = { "Red size: %d bits\n", "Blue size: %d bits\n", "Green size: %d bits\n",
		"Alpha size: %d bits\n", "Color buffer size: %d bits\n",
		"Depth bufer size: %d bits\n" };

	nAttr = sizeof(attr) / sizeof(int);

	for (i = 0; i < nAttr; i++) {

		int value;
		SDL_GL_GetAttribute(attr[i], &value);
		printf(desc[i], value);
	}
}

static void createSurface(int fullscreen)
{
	Uint32 flags = 0;

	flags = SDL_WINDOW_OPENGL;
	if (fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;

	// Create window
	gWindow = SDL_CreateWindow("RetroVM - Sonic CD",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		SCR_WIDTH, SCR_HEIGHT, flags);
	if (gWindow == NULL) {
		logprintf("Couldn't set %dx%d OpenGL video mode: %s\n", SCR_WIDTH, SCR_HEIGHT, SDL_GetError());
		SDL_Quit();
		exit(2);
	}
	SDL_GL_CreateContext(gWindow);

	// do a couple swaps to initialize all framebuffers
	SDL_GL_SwapWindow(gWindow);
	SDL_GL_SwapWindow(gWindow);

	SDL_GL_SetSwapInterval(1);

	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	//glViewport(0, 0, 864, 480);
	//glViewport(0, 0, 1040, 480);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(-2.0, 2.0, -2.0, 2.0, -20.0, 20.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_DEPTH_TEST);

	glDepthFunc(GL_LESS);
}

void UpdateIO() {
	InputSystem_CheckKeyboardInput();
	InputSystem_CheckGamepadInput();
	InputSystem_ClearTouchData();

	if (stageMode != 2)
	{
		EngineCallbacks_ProcessMainLoop();
	}
}
void HandleNextFrame() {
	if (stageMode == 2)
	{
		EngineCallbacks_ProcessMainLoop();
	}
	if (highResMode == 0)
	{
		RenderDevice_FlipScreen();
	}
	else
	{
		RenderDevice_FlipScreenHRes();
	}
}

static void mainLoop()
{
	SDL_Event event;
	int done = 0;

	while (!done) {
		/* Check for events */
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_MOUSEMOTION:
				break;
			case SDL_MOUSEBUTTONDOWN:
				break;
			case SDL_KEYDOWN:
				//DumpTexBuffer();
				break;
			case SDL_QUIT:
				done = 1;
				break;
			default:
				break;
			}
		}
		
		UpdateIO();
		HandleNextFrame();
		SDL_GL_SwapWindow(gWindow);
	}
}

void Init_RetroVM() {
	Init_GlobalAppDefinitions();
	GlobalAppDefinitions_CalculateTrigAngles();
	InitRenderDevice();
	Init_FileIO();
	Init_InputSystem();
	Init_ObjectSystem();
	Init_AnimationSystem();
	Init_PlayerSystem();
	Init_StageSystem();
	Init_Scene3D();
	RenderDevice_SetScreenDimensions(SCR_WIDTH, SCR_HEIGHT);
	EngineCallbacks_StartupRetroEngine();
	gameLanguage = 0;
	gameTrialMode = GAME_FULL;
}

int main (int argc, char **argv)
{
	// Init SDL video subsystem
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {

		logprintf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	// Set GL context attributes
	initAttributes();

	// Create GL context
	createSurface(0);

	// Get GL context attributes
	printAttributes();

	printf("Trying to init\n");
	Init_RetroVM();
	printf("Init finished\n");

	// Draw, get events...
	mainLoop();

	// Cleanup
	SDL_Quit();

	return 0;
}
 