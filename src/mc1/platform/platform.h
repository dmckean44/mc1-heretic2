// Interfaces between the MC1 platform layer, the statically linked modules and the software renderer.

#pragma once

#include <stdint.h>

typedef struct
{
	const char* name;
	void* address;
} h2_symbol_t;

typedef struct h2_module_s
{
	const char* name;
	const h2_symbol_t* symbols;
} h2_module_t;

// modules_gen.c, generated from the exports of Player, Client Effects and game.
extern const h2_module_t h2_modules[];

// h2common_init.c: what H2Common's DllMain did on process attach.
extern void H2Common_Init(void);

// The video back end the software renderer draws through.
typedef struct
{
	int width;
	int height;
	int pitch;
	uint8_t* pixels;
} vid_frame_t;

extern int VIDMC1_SetMode(int width, int height, vid_frame_t* frame);
extern void VIDMC1_SetPalette(const uint8_t* rgb);
extern void VIDMC1_EndFrame(vid_frame_t* frame);

// Fast video memory for the Z buffer, or NULL when there is none to spare.
extern void* VIDMC1_AllocFast(int size);
extern void VIDMC1_FreeFast(void* ptr);
