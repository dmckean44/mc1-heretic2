#include "ref.h"
#include "platform.h"

static refimport_t ri;
static vid_frame_t* frame;

static qboolean R_Init(void)
{
	int width;
	int height;

	if (!ri.Vid_GetModeInfo(&width, &height, (int)ri.Cvar_Get("vid_mode", "1", CVAR_ARCHIVE)->value))
		return false;

	return ri.GLimp_InitGraphics(width, height);
}

static void R_Shutdown(void) { }
static void R_BeginRegistration(const char* map) { }
static struct model_s* R_RegisterModel(const char* name) { return NULL; }
static struct image_s* R_RegisterSkin(const char* name, qboolean* retval) { return NULL; }
static struct image_s* R_RegisterPic(const char* name) { return NULL; }
static void R_SetSky(const char* name, float rotate, const vec3_t axis) { }
static void R_EndRegistration(void) { }
static int R_GetReferencedID(const struct model_s* model) { return 0; }
static int R_RenderFrame(const refdef_t* fd) { return 0; }
static void Draw_GetPicSize(int* w, int* h, const char* name) { *w = 0; *h = 0; }
static void Draw_Pic(int x, int y, int scale, const char* name, float alpha) { }
static void Draw_StretchPic(int x, int y, int w, int h, const char* name, float alpha, DrawStretchPicScaleMode_t mode) { }
static void Draw_Char(int x, int y, int scale, int c, paletteRGBA_t color, qboolean draw_shadow) { }
static void Draw_TileClear(int x, int y, int w, int h, const char* name) { }
static void Draw_Fill(int x, int y, int w, int h, paletteRGBA_t color) { }
static void Draw_FadeScreen(paletteRGBA_t color) { }
static void Draw_BigFont(int x, int y, const char* text, float alpha) { }
static int Draw_BigFontStrlen(const char* text) { return 0; }
static void Draw_BookPic(const char* name, float scale, float alpha) { }
static void Draw_InitCinematic(int width, int height) { }
static void Draw_CloseCinematic(void) { }
static void Draw_Cinematic(const byte* data, const paletteRGB_t* palette) { }
static void Draw_Name(const vec3_t origin, const char* name, paletteRGBA_t color) { }
static void R_BeginFrame(float camera_separation) { }
static void R_EndFrame(void) { VIDMC1_EndFrame(frame); }
static int R_FindSurface(const vec3_t start, const vec3_t end, struct Surface_s* surface) { return 0; }
static qboolean R_InitContext(void* window) { frame = window; return true; }
static void R_ShutdownContext(void) { }
static int R_PrepareForWindow(void) { return 0; }

refexport_t GetRefAPI(const refimport_t rimp)
{
	refexport_t re = { 0 };

	ri = rimp;

	re.api_version = REF_API_VERSION;
	re.title = "Software";
	re.Init = R_Init;
	re.Shutdown = R_Shutdown;
	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = R_RegisterPic;
	re.SetSky = R_SetSky;
	re.EndRegistration = R_EndRegistration;
	re.GetReferencedID = R_GetReferencedID;
	re.RenderFrame = R_RenderFrame;
	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawFadeScreen = Draw_FadeScreen;
	re.DrawBigFont = Draw_BigFont;
	re.BF_Strlen = Draw_BigFontStrlen;
	re.BookDrawPic = Draw_BookPic;
	re.DrawInitCinematic = Draw_InitCinematic;
	re.DrawCloseCinematic = Draw_CloseCinematic;
	re.DrawCinematic = Draw_Cinematic;
	re.Draw_Name = Draw_Name;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = R_EndFrame;
	re.FindSurface = R_FindSurface;
	re.InitContext = R_InitContext;
	re.ShutdownContext = R_ShutdownContext;
	re.PrepareForWindow = R_PrepareForWindow;

	return re;
}
