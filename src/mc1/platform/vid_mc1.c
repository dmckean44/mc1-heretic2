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
	{ 288, 216 },	// smaller 4:3 modes, scaled up by the video output
	{ 256, 192 },
	{ 224, 168 },
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
		uint8_t row[3 * 1024];
		const uint8_t* src = f->pixels + y * f->pitch;

		for (int x = 0; x < f->width; x++)
			memcpy(&row[x * 3], &palette[src[x] * 3], 3);

		fwrite(row, 3, f->width, out);
	}

	fclose(out);
}

void VIDMC1_SetPalette(const uint8_t* rgb)
{
	memcpy(palette, rgb, sizeof(palette));
}

#ifdef MC1

#include <mc1/framebuffer.h>
#include <mc1/memory.h>

// Two framebuffers in VRAM: draw into one while the other is shown.
static fb_t* fbs[2];
static int back;

static void FreeFramebuffers(void)
{
	for (int i = 0; i < 2; i++)
	{
		if (fbs[i] != NULL)
			fb_destroy(fbs[i]);
		fbs[i] = NULL;
	}
}

int VIDMC1_SetMode(const int width, const int height, vid_frame_t* f)
{
	FreeFramebuffers();

	fbs[0] = fb_create(width, height, CMODE_PAL8);
	if (fbs[0] == NULL)
	{
		Com_Printf("Failed to create a %dx%d framebuffer\n", width, height);
		return false;
	}

	fbs[1] = fb_create(width, height, CMODE_PAL8);
	back = (fbs[1] != NULL) ? 1 : 0;
	fb_show(fbs[0], LAYER_1);

	Com_Printf("Framebuffer %dx%d, page flipping %s\n", width, height, back ? "on" : "off");

	f->width = width;
	f->height = height;
	f->pitch = width;
	f->pixels = fbs[back]->pixels;

	return true;
}

void VIDMC1_EndFrame(vid_frame_t* f)
{
	const int every = (int)vid_dumpframes->value;
	uint32_t* pal = fbs[back]->palette;

	if (every > 0 && frame_count % every == 0)
		DumpFrame(f);

	frame_count++;

	for (int i = 0; i < 256; i++)
		pal[i] = palette[i * 3] | (palette[i * 3 + 1] << 8) | (palette[i * 3 + 2] << 16) | 0xff000000u;

	if (fbs[1] != NULL)
	{
		fb_show(fbs[back], LAYER_1);
		back ^= 1;
		f->pixels = fbs[back]->pixels;
	}
}

void* VIDMC1_AllocFast(const int size)
{
	return vmem_alloc(size);
}

void VIDMC1_FreeFast(void* ptr)
{
	vmem_free(ptr);
}

#else

int VIDMC1_SetMode(const int width, const int height, vid_frame_t* f)
{
	free(f->pixels);

	f->width = width;
	f->height = height;
	f->pitch = width;
	f->pixels = calloc(width, height);

	return (f->pixels != NULL);
}

void VIDMC1_EndFrame(vid_frame_t* f)
{
	const int every = (int)vid_dumpframes->value;

	if (every > 0 && frame_count % every == 0)
		DumpFrame(f);

	frame_count++;
}

void* VIDMC1_AllocFast(const int size)
{
	return NULL;
}

void VIDMC1_FreeFast(void* ptr)
{
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
