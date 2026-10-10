/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "header/local.h"

#define	MAX_RIMAGES	1024
static image_t		*r_whitetexture_mip = NULL;
image_t			*r_notexture_mip;
static image_t		r_images[MAX_RIMAGES];
static int		numr_images;
static int		image_max = 0;


/*
===============
R_ImageList_f
===============
*/
void
R_ImageList_f (void)
{
	int		i, used, texels;
	image_t	*image;
	qboolean	freeup;

	R_Printf(PRINT_ALL, "------------------\n");
	texels = 0;
	used = 0;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		char *in_use = "";

		if (image->registration_sequence == registration_sequence)
		{
			in_use = "*";
			used++;
		}

		if (image->registration_sequence <= 0)
			continue;
		texels += image->width*image->height;
		switch (image->type)
		{
		case it_skin:
			R_Printf(PRINT_ALL, "M");
			break;
		case it_sprite:
			R_Printf(PRINT_ALL, "S");
			break;
		case it_wall:
			R_Printf(PRINT_ALL, "W");
			break;
		case it_pic:
			R_Printf(PRINT_ALL, "P");
			break;
		default:
			R_Printf(PRINT_ALL, " ");
			break;
		}

		R_Printf(PRINT_ALL,  " %3i %3i : %s (%dx%d) %s\n",
			image->asset_width, image->asset_height, image->name,
			image->width, image->height, in_use);
	}
	R_Printf(PRINT_ALL, "Total texel count: %i\n", texels);
	freeup = R_ImageHasFreeSpace();
	R_Printf(PRINT_ALL, "Used %d of %d images%s.\n", used, image_max, freeup ? ", has free space" : "");
}

//=======================================================

static image_t *
R_FindFreeImage (void)
{
	image_t		*image;
	int			i;

	// find a free image_t
	for (i=0, image=r_images ; i<numr_images ; i++,image++)
	{
		if (!image->registration_sequence)
			break;
	}
	if (i == numr_images)
	{
		if (numr_images == MAX_RIMAGES)
			ri.Sys_Error(ERR_DROP, "%s: Max images", __func__);
		numr_images++;
	}
	image = &r_images[i];

	return image;
}

static void
R_ImageShrink(const unsigned char* src, unsigned char *dst, int width, int realwidth, int height, int realheight)
{
	int x, y;
	float xstep, ystep;

	xstep = (float)width / realwidth;
	ystep = (float)height / realheight;

	for (y=0; y<realheight; y++)
	{
		for (x=0; x<realwidth; x++)
		{
			// inplace update
			dst[x + y * realwidth] = src[(int)(x * xstep)  + (int)(y * ystep) * width];
		}
	}
}

static void
R_RestoreMips(image_t *image, int min_mips)
{
	int i;

	for (i=min_mips; i<(NUM_MIPS-1); i++)
	{
		R_ImageShrink(image->pixels[i], image->pixels[i + 1],
			      image->width >> i, image->width >> (i + 1),
			      image->height >> i, image->height >> (i + 1));
	}
}

static size_t
R_GetImageMipsSize(size_t mip1_size)
{
	size_t full_size = 0;
	int i;

	for (i=0; i<NUM_MIPS; i++)
	{
		full_size += mip1_size >> (i * 2);
	}
	return full_size;
}

static void
R_RestoreImagePointers(image_t *image, int min_mips)
{
	int i;

	for (i=min_mips; i<NUM_MIPS-1; i++)
	{
		image->pixels[i + 1] = image->pixels[i] + (
			image->width * image->height / (1 << (i * 2)));
	}
	image->mip_levels = NUM_MIPS;
}

byte *
Get_BestImageSize(const image_t *image, int *req_width, int *req_height)
{
	int width, height, i;
	width = image->width;
	height = image->height;

	// return last mip before smaller image size
	for (i = 0; i < (image->mip_levels - 1); i++)
	{
		if (image->pixels[i] &&
		    ((width / 2) < *req_width ||
		    (height / 2) < *req_height))
		{
			*req_width = width;
			*req_height = height;
			return image->pixels[i];
		}

		width = width / 2;
		height = height / 2;
	}

	if (image->pixels[image->mip_levels - 1])
	{
		*req_width = image->width >> (image->mip_levels - 1);
		*req_height = image->height >> (image->mip_levels - 1);
		return image->pixels[image->mip_levels - 1];
	}
	else
	{
		*req_width = image->width;
		*req_height = image->height;
		return image->pixels[0];
	}
}

byte *d_16to8table = NULL; // 16 to 8 bit conversion table

/* The nearest palette color for each RGB565 color, never TRANSPARENT_COLOR. */
static void
R_Build16to8Table(void)
{
	int c, i;

	for (c = 0; c < 0x10000; c++)
	{
		const int r = ((c & 31) << 3) | 4;
		const int g = (((c >> 5) & 63) << 2) | 2;
		const int b = (((c >> 11) & 31) << 3) | 4;
		int best = 0, best_dist = INT_MAX;

		for (i = 0; i < 255; i++)
		{
			const int dr = r - d_8to24table[i * 4 + 0];
			const int dg = g - d_8to24table[i * 4 + 1];
			const int db = b - d_8to24table[i * 4 + 2];
			const int dist = dr * dr + dg * dg + db * db;

			if (dist < best_dist)
			{
				best_dist = dist;
				best = i;
			}
		}

		d_16to8table[c] = best;
	}
}

byte
R_ColorTo8bit(int r, int g, int b)
{
	return d_16to8table[((r >> 3) & 31) | (((g >> 2) & 63) << 5) | (((b >> 3) & 31) << 11)];
}

void
R_Convert32To8bit(const unsigned char* pic_in, pixel_t* pic_out, size_t size,
	qboolean transparent)
{
	size_t i;

	if (!d_16to8table)
		return;

	for(i=0; i < size; i++)
	{
		if (pic_in[3] > 128 || !transparent)
		{
			unsigned int r, g, b, c;

			r = ( pic_in[0] >> 3 ) & 31;
			g = ( pic_in[1] >> 2 ) & 63;
			b = ( pic_in[2] >> 3 ) & 31;

			c = r | ( g << 5 ) | ( b << 11 );

			pic_out[i] = d_16to8table[c & 0xFFFF];
		}
		else
		{
			pic_out[i] = TRANSPARENT_COLOR;
		}

		pic_in += 4;
	}
}

static unsigned
R_ImageNameHash(const char *name)
{
	unsigned hash = 2166136261u;

	while (*name)
	{
		hash = (hash ^ (unsigned char)*name++) * 16777619u;
	}
	return hash;
}

static image_t *
R_NewImage(const char *name, int width, int height, imagetype_t type)
{
	image_t *image = R_FindFreeImage();
	const size_t full_size = R_GetImageMipsSize(width * height);

	if (strlen(name) >= sizeof(image->name))
		ri.Sys_Error(ERR_DROP, "%s: '%s' is too long", __func__, name);

	memset(image, 0, sizeof(*image));
	strcpy(image->name, name);
	image->hash = R_ImageNameHash(name);
	image->registration_sequence = registration_sequence;
	image->width = image->asset_width = width;
	image->height = image->asset_height = height;
	image->type = type;

	image->pixels[0] = malloc(full_size);
	if (!image->pixels[0])
		ri.Sys_Error(ERR_FATAL, "%s: Can't allocate %s", __func__, name);

	R_RestoreImagePointers(image, 0);

	return image;
}

static int
R_MipsInFile(const uint *widths, const uint *heights, int width, int height)
{
	int mips = 0;

	while (mips < NUM_MIPS && widths[mips] == (uint)(width >> mips) && heights[mips] == (uint)(height >> mips))
		mips++;

	return mips;
}

/* .m8: 8-bit texels with their own palette, remapped to the global palette. */
static image_t *
R_LoadM8(const char *name, imagetype_t type)
{
	miptex_t *mt;
	byte remap[256];
	image_t *image;
	int width, height, mips, i, m;

	const int size = ri.FS_LoadFile(name, (void **)&mt);
	if (!mt)
		return NULL;

	if (size < (int)sizeof(miptex_t) || mt->version != MIP_VERSION)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, name);
		ri.FS_FreeFile(mt);
		return NULL;
	}

	width = mt->width[0];
	height = mt->height[0];
	if (width <= 0 || height <= 0 || mt->offsets[0] + width * height > (uint)size)
	{
		R_Printf(PRINT_ALL, "%s: %s is truncated\n", __func__, name);
		ri.FS_FreeFile(mt);
		return NULL;
	}

	for (i = 0; i < 256; i++)
		remap[i] = R_ColorTo8bit(mt->palette[i].r, mt->palette[i].g, mt->palette[i].b);

	image = R_NewImage(name, width, height, type);
	image->num_frames = mt->value;

	mips = R_MipsInFile(mt->width, mt->height, width, height);
	for (m = 0; m < mips; m++)
	{
		const byte *src = (const byte *)mt + mt->offsets[m];
		const int count = (width >> m) * (height >> m);

		if (mt->offsets[m] + count > (uint)size)
			break;

		for (i = 0; i < count; i++)
			image->pixels[m][i] = remap[src[i]];
	}

	if (m < NUM_MIPS)
		R_RestoreMips(image, m - 1);

	ri.FS_FreeFile(mt);

	return image;
}

/* .m32: RGBA texels; mostly transparent texels become TRANSPARENT_COLOR. */
static image_t *
R_LoadM32(const char *name, imagetype_t type)
{
	miptex32_t *mt;
	image_t *image;
	int width, height;

	const int size = ri.FS_LoadFile(name, (void **)&mt);
	if (!mt)
		return NULL;

	if (size < (int)sizeof(miptex32_t) || mt->version != MIP32_VERSION)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, name);
		ri.FS_FreeFile(mt);
		return NULL;
	}

	width = mt->width[0];
	height = mt->height[0];
	if (width <= 0 || height <= 0 || mt->offsets[0] + width * height * 4 > (uint)size)
	{
		R_Printf(PRINT_ALL, "%s: %s is truncated\n", __func__, name);
		ri.FS_FreeFile(mt);
		return NULL;
	}

	image = R_NewImage(name, width, height, type);
	image->num_frames = mt->num_frames;

	R_Convert32To8bit((const byte *)mt + mt->offsets[0], image->pixels[0], width * height, type != it_wall);
	image->transparent = memchr(image->pixels[0], TRANSPARENT_COLOR, width * height) != NULL;
	R_RestoreMips(image, 0);

	ri.FS_FreeFile(mt);

	return image;
}

/*
===============
R_FindImage

Finds or loads the given image or NULL
===============
*/
image_t	*
R_FindImage(const char *name, imagetype_t type)
{
	image_t	*image;
	int	i;
	unsigned hash;
	char path[MAX_QPATH];
	const char *ext;

	if (!name || !name[0])
		return NULL;

	if ((type == it_wall || type == it_skin) && r_lightmap->value)
		return r_whitetexture_mip;

	Q_strncpyz(path, name, sizeof(path));
	for (i = 0; path[i]; i++)
	{
		if (path[i] == '\\')
			path[i] = '/';
	}

	hash = R_ImageNameHash(path);
	for (i=0, image=r_images ; i<numr_images ; i++,image++)
	{
		if (image->hash == hash && !strcmp(path, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	ext = COM_FileExtension(path);
	if (!Q_stricmp(ext, "m8"))
		image = R_LoadM8(path, type);
	else if (!Q_stricmp(ext, "m32"))
		image = R_LoadM32(path, type);
	else
		image = NULL;

	if (!image && r_validation->value)
		R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, path);

	return image;
}

/*
================
R_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void
R_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
		{
			continue; // used this sequence
		}
		if (!image->registration_sequence)
			continue; // free texture
		if (image->type == it_pic)
			continue; // don't free pics
		// free it
		free (image->pixels[0]); // the other mip levels just follow
		memset(image, 0, sizeof(*image));
	}
}

qboolean
R_ImageHasFreeSpace(void)
{
	int		i, used;
	image_t	*image;

	used = 0;

	for (i = 0, image = r_images; i < numr_images; i++, image++)
	{
		if (!image->name[0])
			continue;
		if (image->registration_sequence == registration_sequence)
		{
			used ++;
		}
	}

	if (image_max < used)
	{
		image_max = used;
	}

	// should same size of free slots as currently used
	return (numr_images + used) < MAX_RIMAGES;
}

static struct texture_buffer {
	image_t	image;
	byte	buffer[4096];
} r_notexture_buffer, r_whitetexture_buffer;

static void
R_InitNoTexture(void)
{
	int	m;

	// create a simple checkerboard texture for the default
	r_notexture_mip = &r_notexture_buffer.image;

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->asset_width = r_notexture_mip->asset_height = 16;

	r_notexture_mip->pixels[0] = r_notexture_buffer.buffer;
	R_RestoreImagePointers(r_notexture_mip, 0);

	for (m=0 ; m<NUM_MIPS ; m++)
	{
		int		x, y;
		byte	*dest;

		dest = r_notexture_mip->pixels[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )

					*dest++ = d_16to8table[0x0000];
				else
					*dest++ = d_16to8table[0xFFFF];
			}
	}
}

static void
R_InitWhiteTexture(void)
{
	// create a simple white texture for the default
	r_whitetexture_mip = &r_whitetexture_buffer.image;

	r_whitetexture_mip->width = r_whitetexture_mip->height = 16;
	r_whitetexture_mip->asset_width = r_whitetexture_mip->asset_height = 16;

	r_whitetexture_mip->pixels[0] = r_whitetexture_buffer.buffer;
	R_RestoreImagePointers(r_whitetexture_mip, 0);

	memset(r_whitetexture_buffer.buffer, d_16to8table[0xFFFF],
		sizeof(r_whitetexture_buffer.buffer));
}

/*
==================
R_InitTextures
==================
*/
static void
R_InitTextures (void)
{
	R_InitNoTexture();
	/* empty white texture for r_lightmap = 1*/
	R_InitWhiteTexture();
}

/*
===============
R_InitImages
===============
*/
void
R_InitImages (void)
{
	unsigned char * table16to8;
	registration_sequence = 1;
	image_max = 0;

	d_16to8table = malloc(0x10000);
	if (!d_16to8table)
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't allocate memory for d_16to8table", __func__);

	if (ri.FS_LoadFile("pics/16to8.dat", (void **)&table16to8) == 0x10000)
	{
		memcpy(d_16to8table, table16to8, 0x10000);
		ri.FS_FreeFile(table16to8);
	}
	else
	{
		if (table16to8)
			ri.FS_FreeFile(table16to8);
		R_Build16to8Table();
	}

	R_InitTextures ();
}

/*
===============
R_ShutdownImages
===============
*/
void
R_ShutdownImages (void)
{
	int	i;
	image_t	*image;

	for (i=0, image=r_images ; i<numr_images ; i++, image++)
	{
		if (!image->registration_sequence)
			continue; // free texture

		// free it
		if (image->pixels[0])
			free(image->pixels[0]); // the other mip levels just follow

		memset(image, 0, sizeof(*image));
	}

	if (d_16to8table)
		free(d_16to8table);
}

/*
 * Loads pics/colormap.pcx: 64 light levels and a 256x256 translucency table, plus the global
 * palette the 8-bit renderer draws with.
 */
void
LoadColormap(byte **colormap, byte *palette32)
{
	byte *raw;
	const int len = ri.FS_LoadFile("pics/colormap.pcx", (void **)&raw);
	const byte *in, *end, *pal;
	byte *out;
	int width, height, i, size;

	if (!raw || len < 128 + 769 || raw[0] != 0x0a || raw[2] != 1 || raw[3] != 8)
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't load pics/colormap.pcx", __func__);

	width = (raw[8] | (raw[9] << 8)) - (raw[4] | (raw[5] << 8)) + 1;
	height = (raw[10] | (raw[11] << 8)) - (raw[6] | (raw[7] << 8)) + 1;
	size = width * height;

	if (width != 256 || height < 64 + 256)
		ri.Sys_Error(ERR_FATAL, "%s: pics/colormap.pcx is %dx%d", __func__, width, height);

	*colormap = out = malloc(size);
	if (!out)
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't allocate the colormap", __func__);

	in = raw + 128;
	end = raw + len - 769;
	for (i = 0; i < size && in < end;)
	{
		int c = *in++, run = 1;

		if ((c & 0xc0) == 0xc0)
		{
			run = c & 0x3f;
			c = *in++;
		}

		while (run-- > 0 && i < size)
			out[i++] = c;
	}

	pal = raw + len - 768;
	for (i = 0; i < 256; i++)
	{
		palette32[i * 4 + 0] = pal[i * 3 + 0];
		palette32[i * 4 + 1] = pal[i * 3 + 1];
		palette32[i * 4 + 2] = pal[i * 3 + 2];
		palette32[i * 4 + 3] = 255;
	}

	ri.FS_FreeFile(raw);
}


void
R_BuildAdditiveMap(void)
{
	int s, d;

	if (!vid_additivemap)
		vid_additivemap = malloc(256 * 256);

	for (s = 0; s < 256; s++)
	{
		for (d = 0; d < 256; d++)
		{
			const int r = Q_min(255, d_8to24table[s * 4 + 0] + d_8to24table[d * 4 + 0]);
			const int g = Q_min(255, d_8to24table[s * 4 + 1] + d_8to24table[d * 4 + 1]);
			const int b = Q_min(255, d_8to24table[s * 4 + 2] + d_8to24table[d * 4 + 2]);

			vid_additivemap[s * 256 + d] = R_ColorTo8bit(r, g, b);
		}
	}
}

#define TINT_CACHE_SIZE 16

/*
 * Remaps palette colors to a tint, like modulating a texture with a vertex color. Returns NULL
 * for white. Remaps are cached for the last few colors.
 */
const byte *
R_TintRemap(paletteRGBA_t color)
{
	static byte remaps[TINT_CACHE_SIZE][256];
	static unsigned colors[TINT_CACHE_SIZE];
	static int next;
	const unsigned rgb = color.c | 0xff000000;
	byte *remap;
	int i;

	if (rgb == 0xffffffff)
		return NULL;

	for (i = 0; i < TINT_CACHE_SIZE; i++)
	{
		if (colors[i] == rgb)
			return remaps[i];
	}

	remap = remaps[next];
	colors[next] = rgb;
	next = (next + 1) % TINT_CACHE_SIZE;

	for (i = 0; i < 256; i++)
	{
		remap[i] = R_ColorTo8bit(d_8to24table[i * 4 + 0] * color.r / 255,
			d_8to24table[i * 4 + 1] * color.g / 255,
			d_8to24table[i * 4 + 2] * color.b / 255);
	}
	remap[TRANSPARENT_COLOR] = TRANSPARENT_COLOR;

	return remap;
}

/*
 * How an entity is blended, following Heretic2R's R_HandleTransparency: additive entities are
 * tinted by their color (scaled by alpha for RF_TRANS_ADD_ALPHA), others are blended by alpha.
 */
int
R_EntityBlend(const entity_t *e, const byte **remap)
{
	paletteRGBA_t tint = e->color;

	if (e->flags & RF_TRANS_ADD)
	{
		if (e->flags & (RF_TRANS_ADD_ALPHA | RF_ALPHA_TEXTURE))
		{
			tint.r = tint.r * tint.a / 255;
			tint.g = tint.g * tint.a / 255;
			tint.b = tint.b * tint.a / 255;
		}

		*remap = R_TintRemap(tint);
		return RB_ADD;
	}

	*remap = (e->flags & RF_TRANS_GHOST) ? NULL : R_TintRemap(tint);

	if (e->flags & RF_TRANS_GHOST)
		return RB_33;

	if (e->color.a == 255 && !(e->flags & RF_TRANSLUCENT))
		return RB_OPAQUE;

	if (e->color.a > 170)
		return RB_66;

	return RB_33;
}
