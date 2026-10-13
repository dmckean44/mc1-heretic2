/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright 1998 Raven Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

See the GNU General Public License for more details.
*/

// sw_draw.c -- 2D drawing: pics, text, fills, Heretic II's big font, books and cinematics.
// Positions given in the 640x480 virtual screen (big font, books) are scaled to the frame.

#include "header/local.h"

typedef struct
{
	float xl;
	float yt;
	float xr;
	float yb;
	int w;
	int h;
	int baseline;
} glxy_t;

static image_t *draw_chars;
static image_t *r_font1;
static image_t *r_font2;
static glxy_t *font1;
static glxy_t *font2;

static int cin_width;
static int cin_height;

enum
{
	ALPHA_NONE,
	ALPHA_33,
	ALPHA_66,
	ALPHA_OPAQUE
};

static int
R_AlphaLevel(float alpha)
{
	if (alpha < 0.17f)
		return ALPHA_NONE;
	if (alpha < 0.5f)
		return ALPHA_33;
	if (alpha < 0.83f)
		return ALPHA_66;
	return ALPHA_OPAQUE;
}

static inline void
R_PutPixel(pixel_t *dest, pixel_t c, int alpha)
{
	switch (alpha)
	{
	case ALPHA_33:
		*dest = vid_alphamap[c + *dest * 256];
		break;
	case ALPHA_66:
		*dest = vid_alphamap[c * 256 + *dest];
		break;
	default:
		*dest = c;
		break;
	}
}

static void
R_BlitRow(const byte *src, int sfrac, int sstep, pixel_t *dest, int w, int alpha, const byte *remap)
{
	const byte *alphamap = vid_alphamap;
	int u;

	if (alpha == ALPHA_OPAQUE && !remap && sstep == (1 << 16))
	{
		src += sfrac >> 16;
		for (u = 0; u < w; u++)
		{
			const pixel_t c = src[u];

			if (c != TRANSPARENT_COLOR)
				dest[u] = c;
		}
		return;
	}

	for (u = 0; u < w; u++, sfrac += sstep)
	{
		pixel_t c = src[sfrac >> 16];

		if (c == TRANSPARENT_COLOR)
			continue;

		if (remap)
			c = remap[c];

		if (alpha == ALPHA_OPAQUE)
			dest[u] = c;
		else if (alpha == ALPHA_66)
			dest[u] = alphamap[c * 256 + dest[u]];
		else
			dest[u] = alphamap[c + dest[u] * 256];
	}
}

/*
 * Draws the (s, t, sw, th) rectangle of an image's first mip level scaled into (x, y, w, h),
 * skipping transparent texels.
 */
static void
R_BlitImage(const image_t *image, int s, int t, int sw, int th, int x, int y, int w, int h,
	int alpha, const byte *remap)
{
	int sstep, tstep, sfrac0, tfrac, v;

	if (!image || w <= 0 || h <= 0 || sw <= 0 || th <= 0 || alpha == ALPHA_NONE)
		return;

	sstep = (sw << 16) / w;
	tstep = (th << 16) / h;
	sfrac0 = 0;
	tfrac = 0;

	if (x < 0)
	{
		sfrac0 = -x * sstep;
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		tfrac = -y * tstep;
		h += y;
		y = 0;
	}
	if (x + w > vid_buffer_width)
		w = vid_buffer_width - x;
	if (y + h > vid_buffer_height)
		h = vid_buffer_height - y;
	if (w <= 0 || h <= 0)
		return;

	if (pe_active && pe_version >= 9)
	{
		/* the engine draws it (BLIT), after what it has queued */
		PE_Begin(PE_BLIT, remap ? 12 + 64 : 12);
		PE_Ptr(image->pixels[0] + t * image->width + s);
		PE_Word(image->width);
		PE_Word(sstep);
		PE_Word(tstep);
		PE_Word(sfrac0);
		PE_Word(tfrac);
		PE_Ptr(vid_buffer + y * vid_buffer_width + x);
		PE_Word(vid_buffer_width);
		PE_Word(w);
		PE_Word(h);
		PE_Word((alpha == ALPHA_33 ? 1 : alpha == ALPHA_66 ? 2 : 0) | (remap ? 4 : 0));
		PE_Ptr(vid_alphamap);
		if (remap)
		{
			for (v = 0; v < 256; v += 4)
				PE_Word(remap[v] | remap[v + 1] << 8 | remap[v + 2] << 16 | (unsigned)remap[v + 3] << 24);
		}
		return;
	}

	PE_Finish();
	for (v = 0; v < h; v++, tfrac += tstep)
	{
		const byte *src = image->pixels[0] + (t + (tfrac >> 16)) * image->width + s;
		pixel_t *dest = vid_buffer + (y + v) * vid_buffer_width + x;

		R_BlitRow(src, sfrac0, sstep, dest, w, alpha, remap);
	}
}

static void
R_FillRect(int x, int y, int w, int h, pixel_t c, int alpha)
{
	int u, v;

	if (alpha == ALPHA_NONE)
		return;

	PE_Finish();

	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > vid_buffer_width)
		w = vid_buffer_width - x;
	if (y + h > vid_buffer_height)
		h = vid_buffer_height - y;

	for (v = 0; v < h; v++)
	{
		pixel_t *dest = vid_buffer + (y + v) * vid_buffer_width + x;

		if (alpha == ALPHA_OPAQUE)
		{
			memset(dest, c, w > 0 ? w : 0);
			continue;
		}

		for (u = 0; u < w; u++)
			R_PutPixel(&dest[u], c, alpha);
	}
}

static image_t *
R_FindPicType(const char *name, imagetype_t type)
{
	char fullname[MAX_QPATH];

	if (name[0] == '/' || name[0] == '\\')
		return R_FindImage(name + 1, type);

	Com_sprintf(fullname, sizeof(fullname), "pics/%s", name);
	return R_FindImage(fullname, type);
}

image_t *
RE_Draw_FindPic (const char *name)
{
	return R_FindPicType(name, it_pic);
}

void
Draw_InitLocal (void)
{
	draw_chars = RE_Draw_FindPic("misc/conchars.m32");
	if (!draw_chars)
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't load pics/misc/conchars.m32", __func__);

	r_font1 = RE_Draw_FindPic("misc/font1.m32");
	r_font2 = RE_Draw_FindPic("misc/font2.m32");

	ri.FS_LoadFile("pics/misc/font1.fnt", (void **)&font1);
	ri.FS_LoadFile("pics/misc/font2.fnt", (void **)&font2);
}

static void
R_DrawCharImpl(int x, int y, int scale, int c, paletteRGBA_t color)
{
	const int size = CONCHAR_SIZE * scale;
	const int cell_w = draw_chars->width / 16;
	const int cell_h = draw_chars->height / 16;

	c &= 255;

	if ((c & 127) == 32 || y <= -size)
		return;

	R_BlitImage(draw_chars, (c & 15) * cell_w, (c >> 4) * cell_h, cell_w, cell_h,
		x, y, size, size, R_AlphaLevel(color.a / 255.0f), R_TintRemap(color));
}

void
RE_Draw_Char (int x, int y, int scale, int c, paletteRGBA_t color, qboolean draw_shadow)
{
	if (scale < 1)
		scale = 1;

	if (draw_shadow)
	{
		const paletteRGBA_t shade = { .r = 32, .g = 32, .b = 32, .a = (byte)(color.a * 0.75f) };

		R_DrawCharImpl(x + scale, y + scale, scale, c, shade);
	}

	R_DrawCharImpl(x, y, scale, c, color);
}

void
RE_Draw_GetPicSize (int *w, int *h, const char *name)
{
	const image_t *image = R_FindImage(name, it_pic);

	if (image)
	{
		*w = image->width;
		*h = image->height;
	}
	else
	{
		*w = 0;
		*h = 0;
	}
}

void
RE_Draw_Pic (int x, int y, int scale, const char *name, float alpha)
{
	const image_t *pic = RE_Draw_FindPic(name);

	if (!pic)
		return;

	R_BlitImage(pic, 0, 0, pic->width, pic->height, x, y, pic->width * scale, pic->height * scale,
		R_AlphaLevel(alpha), NULL);
}

void
RE_Draw_StretchPic (int x, int y, int w, int h, const char *name, float alpha, DrawStretchPicScaleMode_t mode)
{
	const image_t *pic = R_FindPicType(name, it_sky);

	if (!pic)
		return;

	if (mode == DSP_SCALE_SCREEN)
	{
		const int xr = x + w;
		const int yb = y + h;

		x = vid.width * x / DEF_WIDTH;
		y = vid.height * y / DEF_HEIGHT;
		w = vid.width * xr / DEF_WIDTH - x;
		h = vid.height * yb / DEF_HEIGHT - y;
	}
	else if (mode == DSP_SCALE_4x3)
	{
		const int xr = x + w;
		const int yb = y + h;
		const int screen_width = vid.height * 4 / 3;
		const int offset_x = (vid.width - screen_width) / 2;

		x = screen_width * x / DEF_WIDTH + offset_x;
		y = vid.height * y / DEF_HEIGHT;
		w = screen_width * xr / DEF_WIDTH + offset_x - x;
		h = vid.height * yb / DEF_HEIGHT - y;
	}

	R_BlitImage(pic, 0, 0, pic->width, pic->height, x, y, w, h, R_AlphaLevel(alpha), NULL);
}

void
RE_Draw_TileClear (int x, int y, int w, int h, const char *name)
{
	const image_t *pic = RE_Draw_FindPic(name);
	int u, v;

	if (!pic)
		return;

	PE_Finish();

	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > vid_buffer_width)
		w = vid_buffer_width - x;
	if (y + h > vid_buffer_height)
		h = vid_buffer_height - y;

	for (v = 0; v < h; v++)
	{
		const byte *src = pic->pixels[0] + ((y + v) % pic->height) * pic->width;
		pixel_t *dest = vid_buffer + (y + v) * vid_buffer_width + x;

		for (u = 0; u < w; u++)
			dest[u] = src[(x + u) % pic->width];
	}
}

void
RE_Draw_Fill (int x, int y, int w, int h, paletteRGBA_t color)
{
	R_FillRect(x, y, w, h, R_ColorTo8bit(color.r, color.g, color.b), R_AlphaLevel(color.a / 255.0f));
}

void
RE_Draw_FadeScreen (paletteRGBA_t color)
{
	R_FillRect(r_newrefdef.x, r_newrefdef.y, r_newrefdef.width, r_newrefdef.height,
		R_ColorTo8bit(color.r, color.g, color.b), R_AlphaLevel(color.a / 255.0f));
}

static const glxy_t *
R_BigFontChar(byte c, const glxy_t *font)
{
	const glxy_t *def = &font[c - 32];

	return (def->w == 0) ? &font[14] : def;
}

void
RE_Draw_BigFont (int x, int y, const char *text, float alpha)
{
	const glxy_t *font = font1;
	const image_t *image = r_font1;
	int ox = x, oy = y;
	int width = vid.width, height = vid.height, offset_x = 0;

	if (!font1 || !font2 || !r_font1 || !r_font2)
		return;

	if (width * 3 > height * 4)
	{
		width = height * 4 / 3;
		offset_x = (vid.width - width) / 2;
	}

	for (;;)
	{
		const byte c = *text++;

		if (c == 0)
			break;

		switch (c)
		{
		case 1:
			ox = (DEF_WIDTH - RE_BF_Strlen(text)) / 2;
			break;
		case 2:
			font = font1;
			image = r_font1;
			break;
		case 3:
			font = font2;
			image = r_font2;
			break;
		case '\t':
			ox += 63;
			break;
		case '\n':
			oy += 18;
			ox = x;
			break;
		case '\r':
			break;
		case ' ':
			ox += 8;
			break;
		default:
			if (c > 32)
			{
				const glxy_t *def = R_BigFontChar(c, font);
				const int cy = oy - def->baseline;
				const int xl = width * ox / DEF_WIDTH + offset_x;
				const int xr = width * (ox + def->w) / DEF_WIDTH + offset_x;
				const int yt = height * cy / DEF_HEIGHT;
				const int yb = height * (cy + def->h) / DEF_HEIGHT;
				const int s = (int)(def->xl * image->width);
				const int t = (int)(def->yt * image->height);

				R_BlitImage(image, s, t, (int)(def->xr * image->width) - s, (int)(def->yb * image->height) - t,
					xl, yt, xr - xl, yb - yt, R_AlphaLevel(alpha), NULL);
				ox += def->w;
			}
			break;
		}
	}
}

int
RE_BF_Strlen (const char *text)
{
	const glxy_t *font = font1;
	int width = 0;

	if (!font1 || !font2)
		return 0;

	for (;;)
	{
		const byte c = *text++;

		switch (c)
		{
		case 0:
		case 1:
		case '\t':
		case '\n':
			return width;
		case 2:
			font = font1;
			break;
		case 3:
			font = font2;
			break;
		case '\r':
			break;
		case ' ':
			width += 8;
			break;
		default:
			if (c > 32)
				width += R_BigFontChar(c, font)->w;
			break;
		}
	}
}

void
RE_Draw_BookPic (const char *name, float scale, float alpha)
{
	const model_t *mod = RE_RegisterModel(name);
	const book_t *book;
	float vid_w = vid.width, vid_h = vid.height;
	int offset_x = 0, offset_y, i;

	if (!mod || mod->type != mod_book)
	{
		R_Printf(PRINT_ALL, "%s: can't find book '%s'\n", __func__, name);
		return;
	}

	book = mod->extradata;

	if (vid_w * 0.75f > vid_h)
	{
		vid_w = vid_h * 4 / 3;
		offset_x = (vid.width - (int)vid_w) / 2;
	}

	offset_x += (int)((book->bheader.total_w - book->bheader.total_w * scale) * 0.5f * (vid_w / book->bheader.total_w));
	offset_y = (int)((book->bheader.total_h - book->bheader.total_h * scale) * 0.5f * (vid_h / book->bheader.total_h));

	for (i = 0; i < book->bheader.num_segments; i++)
	{
		const bookframe_t *frame = &book->bframes[i];
		const image_t *pic = mod->skins[i];
		const int x = (int)floorf(frame->x * vid_w / book->bheader.total_w * scale);
		const int y = (int)floorf(frame->y * vid_h / book->bheader.total_h * scale);
		const int w = (int)ceilf(frame->w * vid_w / book->bheader.total_w * scale);
		const int h = (int)ceilf(frame->h * vid_h / book->bheader.total_h * scale);

		if (pic)
			R_BlitImage(pic, 0, 0, pic->width, pic->height, offset_x + x, offset_y + y, w, h, R_AlphaLevel(alpha), NULL);
	}
}

void
RE_Draw_InitCinematic (int width, int height)
{
	cin_width = width;
	cin_height = height;
}

void
RE_Draw_CloseCinematic (void)
{
	cin_width = 0;
	cin_height = 0;
}

void
RE_Draw_Cinematic (const byte *data, const paletteRGB_t *palette)
{
	byte remap[256];
	int scale, w, h, x0, y0, x, y, i;

	if (cin_width <= 0 || cin_height <= 0)
		return;

	PE_Finish();

	for (i = 0; i < 256; i++)
		remap[i] = R_ColorTo8bit(palette[i].r, palette[i].g, palette[i].b);

	scale = Q_max(1, Q_min(vid.width / cin_width, vid.height / cin_height));
	w = cin_width * scale;
	h = cin_height * scale;
	x0 = (vid.width - w) / 2;
	y0 = (vid.height - h) / 2;

	memset(vid_buffer, 0, vid_buffer_width * vid_buffer_height);

	for (y = 0; y < h && y0 + y < vid_buffer_height; y++)
	{
		const byte *src = data + (y / scale) * cin_width;
		pixel_t *dest = vid_buffer + (y0 + y) * vid_buffer_width + x0;

		for (x = 0; x < w && x0 + x < vid_buffer_width; x++)
			dest[x] = remap[src[x / scale]];
	}
}

void
RE_Draw_Name (const vec3_t origin, const char *name, paletteRGBA_t color)
{
	vec3_t local, transformed;
	int x, y, i, len;

	VectorSubtract(origin, r_origin, local);
	TransformVector(local, transformed);

	if (transformed[2] < NEAR_CLIP)
		return;

	x = (int)(xcenter + xscale * transformed[0] / transformed[2]);
	y = (int)(ycenter - yscale * transformed[1] / transformed[2]);

	len = (int)strlen(name);
	x -= len * CONCHAR_SIZE / 2;
	y -= CONCHAR_SIZE / 2;

	for (i = 0; i < len; i++, x += CONCHAR_SIZE)
		RE_Draw_Char(x, y, 1, name[i], color, true);
}
