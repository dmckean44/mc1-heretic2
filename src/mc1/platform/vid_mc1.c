// Video modes, the window glue the client expects (GLimp_*) and input.
// On the native build frames go to a RAM buffer and can be dumped to PPM files with vid_dumpframes.

#include "client.h"
#include "input.h"
#include "platform.h"
#include "glimp_sdl3.h"

uint sys_frame_time;

static viddef_t modes[] =
{
	{ 320, 240 },
	{ 320, 240 },
	{ 400, 300 },
	{ 512, 384 },
	{ 640, 480 },
};

static vid_frame_t frame;
static uint8_t palette[768];
static cvar_t* vid_dumpframes;
static int frame_count;

qboolean GLimp_Init(void)
{
	VID_InitModes(modes, sizeof(modes) / sizeof(modes[0]));
	vid_dumpframes = Cvar_Get("vid_dumpframes", "0", 0);

	return true;
}

void GLimp_Shutdown(void)
{
}

qboolean GLimp_InitGraphics(const int width, const int height)
{
	if (!VIDMC1_SetMode(width, height, &frame))
		return false;

	if (!re.InitContext(&frame))
		return false;

	viddef.width = width;
	viddef.height = height;

	return true;
}

void GLimp_ShutdownGraphics(void)
{
}

void GLimp_GrabInput(const qboolean grab)
{
}

#ifndef MC1

int VIDMC1_SetMode(const int width, const int height, vid_frame_t* f)
{
	free(f->pixels);

	f->width = width;
	f->height = height;
	f->pitch = width;
	f->pixels = calloc(width, height);

	return (f->pixels != NULL);
}

void VIDMC1_SetPalette(const uint8_t* rgb)
{
	memcpy(palette, rgb, sizeof(palette));
}

static void DumpFrame(const vid_frame_t* f)
{
	char name[MAX_OSPATH];

	Com_sprintf(name, sizeof(name), "frames/%05i.ppm", frame_count);
	Sys_Mkdir("frames");

	FILE* out = fopen(name, "wb");
	if (out == NULL)
		return;

	fprintf(out, "P6\n%i %i\n255\n", f->width, f->height);

	for (int y = 0; y < f->height; y++)
	{
		const uint8_t* src = f->pixels + y * f->pitch;
		for (int x = 0; x < f->width; x++)
			fwrite(&palette[src[x] * 3], 3, 1, out);
	}

	fclose(out);
}

void VIDMC1_EndFrame(vid_frame_t* f)
{
	const int every = (int)vid_dumpframes->value;
	if (every > 0 && frame_count % every == 0)
		DumpFrame(f);

	frame_count++;
}

#endif

void IN_Init(void)
{
}

void IN_Shutdown(void)
{
}

void IN_Update(void)
{
	sys_frame_time = curtime;
}

void IN_Move(usercmd_t* cmd)
{
}

void In_FlushQueue(void)
{
}

void IN_GetClipboardText(char* out, const size_t n)
{
	*out = 0;
}
