/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright 1998 Raven Software

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

See the GNU General Public License for more details.
*/

// sw_sprite.c -- Heretic II sprites: standard, dynamic, variable and line sprites, drawn as
// textured polygons with the entity's blending.

#include "header/local.h"
#include "q_Sprite.h"

extern polydesc_t r_polydesc;
extern vec5_t r_clip_verts[2][MAXWORKINGVERTS+2];

/*
 * Draws a planar polygon whose vertices carry texture coordinates in texels. The texture
 * mapping is the affine map through the first three vertices.
 */
static void
R_DrawSpritePoly(const vec3_t *points, const float (*st)[2], int count, const image_t *image,
	int blend, const byte *remap, qboolean nodepth)
{
	vec3_t e1, e2, normal, saxis, taxis, origin_to_view;
	float d11, d12, d22, det, a, b;
	vec5_t *pverts = r_clip_verts[0];
	int i;

	if (count < 3 || count > MAXWORKINGVERTS)
		return;

	VectorSubtract(points[1], points[0], e1);
	VectorSubtract(points[2], points[0], e2);
	CrossProduct(e1, e2, normal);
	if (VectorNormalize(normal) == 0.0f)
		return;

	d11 = DotProduct(e1, e1);
	d12 = DotProduct(e1, e2);
	d22 = DotProduct(e2, e2);
	det = d11 * d22 - d12 * d12;
	if (fabsf(det) < 1e-6f)
		return;

	// Axis X maps a point P to s(P) = (P - points[0]) . X + st[0][0].
	a = ((st[1][0] - st[0][0]) * d22 - (st[2][0] - st[0][0]) * d12) / det;
	b = ((st[2][0] - st[0][0]) * d11 - (st[1][0] - st[0][0]) * d12) / det;
	VectorScale(e1, a, saxis);
	VectorMA(saxis, b, e2, saxis);

	a = ((st[1][1] - st[0][1]) * d22 - (st[2][1] - st[0][1]) * d12) / det;
	b = ((st[2][1] - st[0][1]) * d11 - (st[1][1] - st[0][1]) * d12) / det;
	VectorScale(e1, a, taxis);
	VectorMA(taxis, b, e2, taxis);

	VectorSubtract(r_origin, points[0], origin_to_view);
	if (DotProduct(origin_to_view, normal) > 0.0f)
		VectorInverse(normal);

	for (i = 0; i < count; i++)
	{
		VectorCopy(points[i], pverts[i]);
		pverts[i][3] = st[i][0];
		pverts[i][4] = st[i][1];
	}

	r_polydesc.pixels = image->pixels[0];
	r_polydesc.pixel_width = image->width;
	r_polydesc.pixel_height = image->height;
	r_polydesc.dist = 0;
	VectorCopy(normal, r_polydesc.vpn);
	VectorCopy(saxis, r_polydesc.vright);
	VectorCopy(taxis, r_polydesc.vup);
	VectorCopy(origin_to_view, r_polydesc.viewer_position);
	r_polydesc.s_offset = st[0][0];
	r_polydesc.t_offset = st[0][1];
	r_polydesc.nump = count;
	r_polydesc.stipple_parity = 1;

	R_ClipAndDrawPolyBlend(blend, remap, nodepth);

	r_polydesc.stipple_parity = 0;
}

static void
R_SpriteQuad(const entity_t *e, const vec3_t up, const vec3_t right, const svertex_t *verts, int count,
	const image_t *image, int blend, const byte *remap)
{
	vec3_t points[MAXWORKINGVERTS];
	float st[MAXWORKINGVERTS][2];
	int i;

	if (count > MAXWORKINGVERTS)
		count = MAXWORKINGVERTS;

	for (i = 0; i < count; i++)
	{
		VectorMA(e->origin, e->scale * verts[i].y, up, points[i]);
		VectorMA(points[i], e->scale * verts[i].x, right, points[i]);
		st[i][0] = verts[i].s * image->width;
		st[i][1] = verts[i].t * image->height;
	}

	R_DrawSpritePoly((const vec3_t *)points, (const float (*)[2])st, count, image, blend, remap,
		(e->flags & RF_NODEPTHTEST) != 0);
}

/*
** R_DrawSprite
**
** Draw currententity / currentmodel as a single texture
** mapped polygon
*/
void
R_DrawSprite(entity_t *currententity, const model_t *currentmodel)
{
	const dsprite_t *sprite = currentmodel->extradata;
	const dsprframe_t *frame;
	const image_t *image;
	const byte *remap;
	vec3_t up, right;
	int blend;

	if (currententity->frame < 0 || currententity->frame >= sprite->numframes)
		currententity->frame = 0;

	frame = &sprite->frames[currententity->frame];
	image = currentmodel->skins[currententity->frame];
	if (!image)
		return;

	blend = R_EntityBlend(currententity, &remap);

	if (currententity->flags & RF_FIXED)
	{
		vec3_t dir;

		DirAndUpFromAngles(currententity->angles, dir, up);
		CrossProduct(up, dir, right);
		VectorNormalize(right);
	}
	else
	{
		VectorCopy(vup, up);
		VectorCopy(vright, right);
	}

	switch (currententity->spriteType)
	{
	case SPRITE_EDICT:
	case SPRITE_STANDARD:
	{
		const float xl = -frame->origin_x;
		const float xr = frame->width - frame->origin_x;
		const float yt = -frame->origin_y;
		const float yb = frame->height - frame->origin_y;
		svertex_t verts[4];

		SVERTEX_SET(verts[0], xl, yt, 0.0f, 1.0f);
		SVERTEX_SET(verts[1], xl, yb, 0.0f, 0.0f);
		SVERTEX_SET(verts[2], xr, yb, 1.0f, 0.0f);
		SVERTEX_SET(verts[3], xr, yt, 1.0f, 1.0f);

		R_SpriteQuad(currententity, up, right, verts, 4, image, blend, remap);
		break;
	}

	case SPRITE_DYNAMIC:
		R_SpriteQuad(currententity, up, right, currententity->verts, 4, image, blend, remap);
		break;

	case SPRITE_VARIABLE:
		R_SpriteQuad(currententity, up, right, currententity->verts_p, currententity->numVerts, image, blend, remap);
		break;

	case SPRITE_LINE:
	{
		vec3_t diff, dir, start_offset, end_offset, points[4];
		float st[4][2];
		const float tile = (currententity->tile > 0.0f) ? currententity->tile : 1.0f;
		const float t0 = currententity->tileoffset * image->height;
		const float t1 = (currententity->tileoffset + tile) * image->height;

		VectorSubtract(currententity->endpos, currententity->startpos, diff);
		CrossProduct(diff, vpn, dir);
		VectorNormalize(dir);
		VectorScale(dir, currententity->scale * 0.5f, start_offset);
		VectorScale(dir, currententity->scale2 * 0.5f, end_offset);

		VectorSubtract(currententity->startpos, start_offset, points[0]);
		VectorAdd(currententity->startpos, start_offset, points[1]);
		VectorAdd(currententity->endpos, end_offset, points[2]);
		VectorSubtract(currententity->endpos, end_offset, points[3]);

		st[0][0] = 0.0f;			st[0][1] = t0;
		st[1][0] = image->width;	st[1][1] = t0;
		st[2][0] = image->width;	st[2][1] = t1;
		st[3][0] = 0.0f;			st[3][1] = t1;

		R_DrawSpritePoly((const vec3_t *)points, (const float (*)[2])st, 4, image, blend, remap,
			(currententity->flags & RF_NODEPTHTEST) != 0);
		break;
	}

	default:
		break;
	}
}
