/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

See the GNU General Public License for more details.
*/

// sw_part.c -- Heretic II particles: colored diamonds sized like the GL renderer's quads,
// alpha blended or additive.

#include "header/local.h"
#include "ParticleFlags.h"

#define MAX_PARTICLE_PIXELS 32

static vec3_t r_pright, r_pup, r_ppn;

enum
{
	BLEND_33,
	BLEND_66,
	BLEND_OPAQUE,
	BLEND_ADD
};

static void
R_DrawParticle(const particle_t *p, qboolean additive)
{
	vec3_t local, transformed;
	paletteRGBA_t c = p->color;
	int u, v, pix, half, dy, blend;
	pixel_t color;
	zbufvalue_t izi;
	float zi;

	VectorSubtract(p->origin, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

	zi = 1.0F / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5F);
	v = (int)(ycenter - zi * transformed[1] + 0.5F);

	pix = (int)(p->scale * 2.0F * xscale * zi + 0.5F);
	if (pix < 1)
		pix = 1;
	else if (pix > MAX_PARTICLE_PIXELS)
		pix = MAX_PARTICLE_PIXELS;
	half = pix >> 1;

	if (u + half < d_vrectx || u - half > r_refdef.vrectright - 1 ||
		v + half < d_vrecty || v - half > r_refdef.vrectbottom - 1)
	{
		return;
	}

	if (additive)
	{
		color = R_ColorTo8bit(c.r * c.a / 255, c.g * c.a / 255, c.b * c.a / 255);
		blend = BLEND_ADD;
	}
	else
	{
		color = R_ColorTo8bit(c.r, c.g, c.b);
		blend = (c.a > 170) ? BLEND_OPAQUE : (c.a > 85) ? BLEND_66 : BLEND_33;
		if (c.a < 16)
			return;
	}

	izi = (zbufvalue_t)(zi * 0x8000);

	for (dy = -half; dy <= half; dy++)
	{
		const int y = v + dy;
		const int w = half - abs(dy);
		int x0 = u - w, x1 = u + w, x;
		zbufvalue_t *pz;
		pixel_t *pdest;

		if (y < d_vrecty || y >= r_refdef.vrectbottom)
			continue;

		if (x0 < d_vrectx)
			x0 = d_vrectx;
		if (x1 > r_refdef.vrectright - 1)
			x1 = r_refdef.vrectright - 1;

		pz = d_pzbuffer + vid_buffer_width * y;
		pdest = d_viewbuffer + vid_buffer_width * y;

		for (x = x0; x <= x1; x++)
		{
			if (pz[x] > izi)
				continue;

			switch (blend)
			{
			case BLEND_ADD:
				pdest[x] = vid_additivemap[color * 256 + pdest[x]];
				break;
			case BLEND_33:
				pdest[x] = vid_alphamap[color + pdest[x] * 256];
				break;
			case BLEND_66:
				pdest[x] = vid_alphamap[color * 256 + pdest[x]];
				break;
			default:
				pz[x] = izi;
				pdest[x] = color;
				break;
			}
		}
	}
}

void
R_DrawParticles (void)
{
	int i;

	PE_Finish();

	VectorScale( vright, xscaleshrink, r_pright );
	VectorScale( vup, yscaleshrink, r_pup );
	VectorCopy( vpn, r_ppn );

	for (i = 0; i < r_newrefdef.num_particles; i++)
		R_DrawParticle(&r_newrefdef.particles[i], false);

	for (i = 0; i < r_newrefdef.anum_particles; i++)
		R_DrawParticle(&r_newrefdef.aparticles[i], true);
}
