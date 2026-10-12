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
// sw_alias.c: routines for setting up to draw alias models

/*
** use a real variable to control lerping
*/
#include <stdint.h>
#include "header/local.h"
#include "header/flexmodel.h"
#include "Skeletons/r_SkeletonLerp.h"
#include "anorms.h"
#include "Skeletons.h"

#define LIGHT_MIN	5	// lowest light value we'll allow, to avoid the
				//  need for inner-loop light clamping

int				r_amodels_drawn;

affinetridesc_t	r_affinetridesc;

static vec3_t	r_plightvec;
static vec3_t	r_lerp_frontv, r_lerp_backv, r_lerp_move;

static light3_t	r_ambientlight;
int		r_aliasblendcolor;
static vec3_t		r_shadelight;


static const fmaliasframe_t	*r_thisframe, *r_lastframe;
static const fmdl_t	*s_pmdl;
static float	r_alias_scale;

static float	aliastransform[3][4];
static float	aliasworldtransform[3][4];
static float	aliasoldworldtransform[3][4];

static float	s_ziscale;
static vec3_t	s_alias_forward, s_alias_right, s_alias_up;


#define r_avertexnormals bytedirs


static void R_AliasTransformVector(const vec3_t in, vec3_t out, const float xf[3][4]);
static void R_AliasTransformFinalVerts(int numpoints, finalvert_t *fv, const fmtrivertx_t *oldv, const fmtrivertx_t *newv);

void R_AliasProjectAndClipTestFinalVert(finalvert_t *fv);
#if defined(__MRISC32_VECTOR_OPS__)
static void R_AliasTransformFinalVertsVec(int numpoints, finalvert_t *fv, const fmtrivertx_t *oldv,
	const fmtrivertx_t *newv);
#endif


/*
================
R_AliasCheckBBox
================
*/

#define BBOX_TRIVIAL_ACCEPT 0
#define BBOX_MUST_CLIP_XY   1
#define BBOX_MUST_CLIP_Z    2
#define BBOX_TRIVIAL_REJECT 8

/*
** R_AliasCheckFrameBBox
**
** Checks a specific alias frame bounding box
*/
static unsigned long
R_AliasCheckFrameBBox( const fmaliasframe_t *frame, float worldxf[3][4] )
{
	// FIXME: should this really be using long and not int32_t or sth?
	unsigned long aggregate_and_clipcode = ~0U,
		          aggregate_or_clipcode = 0;
	int           i;
	vec3_t        mins, maxs;
	vec3_t        transformed_min, transformed_max;
	qboolean      zfullyclipped = true;

	/*
	** get the exact frame bounding box
	*/
	for (i=0 ; i<3 ; i++)
	{
		mins[i] = frame->translate[i] * r_alias_scale;
		maxs[i] = mins[i] + frame->scale[i] * 255 * r_alias_scale;
	}

	/*
	** transform the min and max values into view space
	*/
	R_AliasTransformVector( mins, transformed_min, aliastransform );
	R_AliasTransformVector( maxs, transformed_max, aliastransform );

	if ( transformed_min[2] >= ALIAS_Z_CLIP_PLANE )
		zfullyclipped = false;
	if ( transformed_max[2] >= ALIAS_Z_CLIP_PLANE )
		zfullyclipped = false;

	if ( zfullyclipped )
	{
		return BBOX_TRIVIAL_REJECT;
	}

	/*
	** build a transformed bounding box from the given min and max
	*/
	for ( i = 0; i < 8; i++ )
	{
		int      j;
		vec3_t   tmp, transformed;
		unsigned long clipcode = 0;

		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		R_AliasTransformVector( tmp, transformed, worldxf );

		for ( j = 0; j < 4; j++ )
		{
			float dp = DotProduct( transformed, view_clipplanes[j].normal );

			if ( ( dp - view_clipplanes[j].dist ) < 0.0F )
				clipcode |= 1 << j;
		}

		aggregate_and_clipcode &= clipcode;
		aggregate_or_clipcode  |= clipcode;
	}

	if ( aggregate_and_clipcode )
	{
		return BBOX_TRIVIAL_REJECT;
	}
	if ( !aggregate_or_clipcode )
	{
		return BBOX_TRIVIAL_ACCEPT;
	}

	return BBOX_MUST_CLIP_XY;
}

static int
R_AliasCheckBBox (const entity_t *currententity)
{
	unsigned long ccodes[2] = { 0, 0 };

	ccodes[0] = R_AliasCheckFrameBBox( r_thisframe, aliasworldtransform );

	/*
	** non-lerping model
	*/
	if ( currententity->backlerp == 0 )
	{
		if ( ccodes[0] == BBOX_TRIVIAL_ACCEPT )
			return BBOX_TRIVIAL_ACCEPT;
		else if ( ccodes[0] & BBOX_TRIVIAL_REJECT )
			return BBOX_TRIVIAL_REJECT;
		else
			return ( ccodes[0] & ~BBOX_TRIVIAL_REJECT );
	}

	ccodes[1] = R_AliasCheckFrameBBox( r_lastframe, aliasoldworldtransform );

	if ( ( ccodes[0] | ccodes[1] ) == BBOX_TRIVIAL_ACCEPT )
		return BBOX_TRIVIAL_ACCEPT;
	else if ( ( ccodes[0] & ccodes[1] ) & BBOX_TRIVIAL_REJECT )
		return BBOX_TRIVIAL_REJECT;
	else
		return ( ccodes[0] | ccodes[1] ) & ~BBOX_TRIVIAL_REJECT;
}


/*
================
R_AliasTransformVector
================
*/
static void
R_AliasTransformVector(const vec3_t in, vec3_t out, const float xf[3][4] )
{
	out[0] = DotProduct(in, xf[0]) + xf[0][3];
	out[1] = DotProduct(in, xf[1]) + xf[1][3];
	out[2] = DotProduct(in, xf[2]) + xf[2][3];
}


/*
================
R_AliasPreparePoints

General clipped case
================
*/

extern void (*d_pdrawspans)(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_Opaque(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_33(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_66(const entity_t *currententity, spanpackage_t *pspanpackage);

static void
R_AliasTransformLerpedVerts(int numpoints, finalvert_t *fv, const vec3_t *lerped, const fmtrivertx_t *newv)
{
	int i;

	for (i = 0; i < numpoints; i++, fv++, lerped++, newv++)
	{
		const float *plightnormal = r_avertexnormals[newv->lightnormalindex];
		float lightcos;

		fv->xyz[0] = DotProduct(*lerped, aliastransform[0]) + aliastransform[0][3];
		fv->xyz[1] = DotProduct(*lerped, aliastransform[1]) + aliastransform[1][3];
		fv->xyz[2] = DotProduct(*lerped, aliastransform[2]) + aliastransform[2][3];

		fv->flags = 0;

		lightcos = DotProduct(plightnormal, r_plightvec);
		if (lightcos < 0)
		{
			int j;

			for (j = 0; j < 3; j++)
			{
				int temp = r_ambientlight[j] + (int)(r_shadelight[j] * lightcos);

				fv->cv.l[j] = (temp < 0) ? 0 : temp;
			}
		}
		else
		{
			memcpy(fv->cv.l, r_ambientlight, sizeof(light3_t));
		}

		if (fv->xyz[2] < ALIAS_Z_CLIP_PLANE)
			fv->flags |= ALIAS_Z_CLIP;
		else
			R_AliasProjectAndClipTestFinalVert(fv);
	}
}

static const image_t *
R_AliasSkin(const entity_t *e, const model_t *mod)
{
	const int skinnum = (e->skinnum >= 0 && e->skinnum < MAX_FRAMES) ? e->skinnum : 0;

	if (e->skin)
		return e->skin;
	if (mod->skins[skinnum])
		return mod->skins[skinnum];
	return mod->skins[0];
}

static const image_t *
R_AliasNodeSkin(const entity_t *e, const model_t *mod, int skin_index)
{
	const image_t *skin;

	if (e->skin && e->skins)
		skin = e->skins[skin_index];
	else
		skin = (skin_index >= 0 && skin_index < MAX_FRAMES) ? mod->skins[skin_index] : NULL;

	if (skin)
		return skin;
	if (e->skin)
		return e->skin;
	return mod->skins[0];
}

static void
R_AliasSetSkin(const image_t *skin)
{
	r_affinetridesc.pskin = skin->pixels[0];
	r_affinetridesc.skinwidth = skin->width;
	r_affinetridesc.skinheight = skin->height;

	R_PolysetUpdateTables ();

	if (pe_alias)
		PE_SetSkin(r_affinetridesc.pskin, r_affinetridesc.skinwidth);
}

// The test R_DrawTriangle makes, done before the texture coordinates are set up.
static inline qboolean
R_TriangleFacesViewer(const finalvert_t *a, const finalvert_t *b, const finalvert_t *c)
{
	const int dv0_ab = a->cv.u - b->cv.u;
	const int dv1_ab = a->cv.v - b->cv.v;
	const int dv0_ac = a->cv.u - c->cv.u;
	const int dv1_ac = a->cv.v - c->cv.v;

	if (!(dv0_ab | dv1_ab) || !(dv0_ac | dv1_ac))
		return false;

	return dv0_ac * dv1_ab - dv0_ab * dv1_ac < 0;
}

static qboolean r_alias_mesh;		// the engine draws the unclipped triangles from memory
static unsigned r_alias_mesh_fence;	// the last MESH command, which reads finalverts

static void
R_AliasMesh(const finalvert_t *verts, int first, int count)
{
	if (count <= 0)
		return;

	r_alias_mesh_fence = PE_Begin(PE_MESH, 6);
	PE_Ptr(verts);
	PE_Word(sizeof(finalvert_t));
	PE_Ptr(s_pmdl->tris);
	PE_Ptr(&s_pmdl->node_tris[first]);
	PE_Word(count);
	PE_Ptr(s_pmdl->st_verts);
}

static void
R_AliasDrawTriangles(const entity_t *currententity, finalvert_t *verts, int first, int count,
	qboolean clipped)
{
	const fmstvert_t *pstverts = s_pmdl->st_verts;
	int mesh_first = first;
	int i;

	if (r_alias_mesh && !clipped)
	{
		R_AliasMesh(verts, first, count);
		return;
	}

	for (i = first; i < first + count; i++)
	{
		const fmtriangle_t *ptri = &s_pmdl->tris[s_pmdl->node_tris[i]];
		finalvert_t *pfv[3];
		int k;

		pfv[0] = &verts[ptri->index_xyz[0]];
		pfv[1] = &verts[ptri->index_xyz[1]];
		pfv[2] = &verts[ptri->index_xyz[2]];

		if (!(pfv[0]->flags | pfv[1]->flags | pfv[2]->flags))
		{
			if (r_alias_mesh || !R_TriangleFacesViewer(pfv[0], pfv[1], pfv[2]))
				continue;
		}
		else if (r_alias_mesh)
		{
			R_AliasMesh(verts, mesh_first, i - mesh_first);
			mesh_first = i + 1;
		}

		if (pfv[0]->flags & pfv[1]->flags & pfv[2]->flags)
			continue;	// completely clipped

		for (k = 0; k < 3; k++)
		{
			pfv[k]->cv.s = pstverts[ptri->index_st[k]].s << SHIFT16XYZ;
			pfv[k]->cv.t = pstverts[ptri->index_st[k]].t << SHIFT16XYZ;
		}

		if (!(pfv[0]->flags | pfv[1]->flags | pfv[2]->flags))
			R_DrawTriangle(currententity, pfv[0], pfv[1], pfv[2]);
		else
			R_AliasClipTriangle(currententity, pfv[0], pfv[1], pfv[2]);
	}

	if (r_alias_mesh)
		R_AliasMesh(verts, mesh_first, first + count - mesh_first);
}

/*
================
R_AliasPreparePoints

Transforms the vertices, then draws the triangles of each visible mesh node with its skin.
================
*/
static void
R_AliasPreparePoints (entity_t *currententity, const model_t *currentmodel, finalvert_t *verts,
	const finalvert_t *verts_max, qboolean lerped)
{
	const fmnodeinfo_t *nodeinfo = currententity->fmnodeinfo;
	const image_t *skin = R_AliasSkin(currententity, currentmodel);
	void (*drawspans)(const entity_t *, spanpackage_t *) = d_pdrawspans;
	const qboolean model_pe = pe_alias;
	int clip_flags = 0;
	int n;

	if ((verts + s_pmdl->header.num_xyz) >= verts_max)
	{
		r_outofverts = true;
		return;
	}

	if (lerped)
	{
		R_AliasTransformLerpedVerts(s_pmdl->header.num_xyz, verts, (const vec3_t *)s_lerped, r_thisframe->verts);
	}
	else
#if defined(__MRISC32_VECTOR_OPS__) && !defined(ENABLE_SW_COLORLIGHT)
	if (s_pmdl->header.num_xyz > 0)
	{
		R_AliasTransformFinalVertsVec(s_pmdl->header.num_xyz, verts, r_lastframe->verts, r_thisframe->verts);
	}
	else
#endif
	{
		R_AliasTransformFinalVerts(s_pmdl->header.num_xyz, verts, r_lastframe->verts, r_thisframe->verts);
	}

	if (pe_alias && pe_version >= 5)
	{
		for (n = 0; n < s_pmdl->header.num_xyz; n++)
			clip_flags |= verts[n].flags;
	}

	R_AliasSetSkin(skin);

	for (n = 0; n < s_pmdl->num_nodes; n++)
	{
		const fmnodeinfo_t *info = (nodeinfo && n < s_pmdl->header.num_mesh_nodes) ? &nodeinfo[n] : NULL;
		const image_t *node_skin = skin;

		d_pdrawspans = drawspans;

		if (info)
		{
			if (info->flags & FMNI_NO_DRAW)
				continue;

			if (info->flags & FMNI_USE_SKIN)
				node_skin = R_AliasNodeSkin(currententity, currentmodel, info->skin);

			if ((info->flags & FMNI_USE_COLOR) && info->color.a < 255 && drawspans == R_PolysetDrawSpans8_Opaque)
				d_pdrawspans = (info->color.a > 170) ? R_PolysetDrawSpans8_66 : R_PolysetDrawSpans8_33;
		}

		if (!node_skin)
			continue;

		// Translucent nodes are drawn by the CPU.
		pe_alias = model_pe && d_pdrawspans == R_PolysetDrawSpans8_Opaque;
		if (model_pe && !pe_alias)
			PE_Finish();
		r_alias_mesh = pe_alias && pe_version >= 5;

		if (node_skin != skin)
			R_AliasSetSkin(node_skin);

		R_AliasDrawTriangles(currententity, verts, s_pmdl->node_tris_range[n].first,
			s_pmdl->node_tris_range[n].count, clip_flags != 0);

		if (node_skin != skin)
			R_AliasSetSkin(skin);
	}

	d_pdrawspans = drawspans;
	pe_alias = model_pe;
}


/*
================
R_AliasSetUpTransform
================
*/
static void
R_AliasSetUpTransform(const entity_t *currententity)
{
	int				i;
	static float	viewmatrix[3][4];

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity
	//
	// AngleVectors never change angles, we can convert from const
	vec3_t angles;

	R_EntityAngles(currententity, angles);
	AngleVectors(angles, s_alias_forward, s_alias_right, s_alias_up );

	// TODO: can do this with simple matrix rearrangement
	memset( aliasworldtransform, 0, sizeof( aliasworldtransform ) );
	memset( aliasoldworldtransform, 0, sizeof( aliasworldtransform ) );

	for (i=0 ; i<3 ; i++)
	{
		aliasoldworldtransform[i][0] = aliasworldtransform[i][0] =  s_alias_forward[i];
		aliasoldworldtransform[i][1] = aliasworldtransform[i][1] = -s_alias_right[i];
		aliasoldworldtransform[i][2] = aliasworldtransform[i][2] =  s_alias_up[i];
	}

	aliasworldtransform[0][3] = currententity->origin[0]-r_origin[0];
	aliasworldtransform[1][3] = currententity->origin[1]-r_origin[1];
	aliasworldtransform[2][3] = currententity->origin[2]-r_origin[2];

	aliasoldworldtransform[0][3] = currententity->origin[0]-r_origin[0];
	aliasoldworldtransform[1][3] = currententity->origin[1]-r_origin[1];
	aliasoldworldtransform[2][3] = currententity->origin[2]-r_origin[2];

	// FIXME: can do more efficiently than full concatenation
	// memcpy( rotationmatrix, t2matrix, sizeof( rotationmatrix ) );

	// R_ConcatTransforms (t2matrix, tmatrix, rotationmatrix);

	// TODO: should be global, set when vright, etc., set
	VectorCopy (vright, viewmatrix[0]);
	VectorCopy (vup, viewmatrix[1]);
	VectorInverse (viewmatrix[1]);
	VectorCopy (vpn, viewmatrix[2]);

	if ( currententity->flags & RF_WEAPONMODEL )
	{
		viewmatrix[0][3] = 0;
		viewmatrix[1][3] = 0;
		viewmatrix[2][3] = sw_gunzposition->value;
	}
	else
	{
		viewmatrix[0][3] = 0;
		viewmatrix[1][3] = 0;
		viewmatrix[2][3] = 0;
	}

	// memcpy( aliasworldtransform, rotationmatrix, sizeof( aliastransform ) );

	R_ConcatTransforms (viewmatrix, aliasworldtransform, aliastransform);

	aliasworldtransform[0][3] = currententity->origin[0];
	aliasworldtransform[1][3] = currententity->origin[1];
	aliasworldtransform[2][3] = currententity->origin[2];

	aliasoldworldtransform[0][3] = currententity->origin[0];
	aliasoldworldtransform[1][3] = currententity->origin[1];
	aliasoldworldtransform[2][3] = currententity->origin[2];
}


/*
================
R_AliasTransformFinalVerts
================
*/
static void
R_AliasTransformFinalVerts(int numpoints, finalvert_t *fv, const fmtrivertx_t *oldv, const fmtrivertx_t *newv)
{
	int i;

	for ( i = 0; i < numpoints; i++, fv++, oldv++, newv++ )
	{
		float	lightcos;
		const float	*plightnormal;
		vec3_t  lerped_vert;

		lerped_vert[0] = r_lerp_move[0] + oldv->v[0]*r_lerp_backv[0] + newv->v[0]*r_lerp_frontv[0];
		lerped_vert[1] = r_lerp_move[1] + oldv->v[1]*r_lerp_backv[1] + newv->v[1]*r_lerp_frontv[1];
		lerped_vert[2] = r_lerp_move[2] + oldv->v[2]*r_lerp_backv[2] + newv->v[2]*r_lerp_frontv[2];

		plightnormal = r_avertexnormals[newv->lightnormalindex];

		fv->xyz[0] = DotProduct(lerped_vert, aliastransform[0]) + aliastransform[0][3];
		fv->xyz[1] = DotProduct(lerped_vert, aliastransform[1]) + aliastransform[1][3];
		fv->xyz[2] = DotProduct(lerped_vert, aliastransform[2]) + aliastransform[2][3];

		fv->flags = 0;

		// lighting
		lightcos = DotProduct (plightnormal, r_plightvec);

		if (lightcos < 0)
		{
			int		j;

			for(j=0; j<3; j++)
			{
				int temp;

				temp = r_ambientlight[j];

				temp += (r_shadelight[j] * lightcos);

				// clamp; because we limited the minimum ambient and shading light, we
				// don't have to clamp low light, just bright
				if (temp < 0)
					temp = 0;

				fv->cv.l[j] = temp;
			}
		}
		else
			memcpy(fv->cv.l, r_ambientlight, sizeof(light3_t));

		if ( fv->xyz[2] < ALIAS_Z_CLIP_PLANE )
		{
			fv->flags |= ALIAS_Z_CLIP;
		}
		else
		{
			R_AliasProjectAndClipTestFinalVert( fv );
		}
	}

}

#if defined(__MRISC32_VECTOR_OPS__)
/*
 * The same arithmetic as R_AliasTransformFinalVerts and
 * R_AliasProjectAndClipTestFinalVert, VL vertices at a time. Only l[0] of the
 * light is used without colour light, so it is stored in all three. Vertices
 * behind the z clip plane get projected too; those values are never read.
 */
static void
R_AliasTransformFinalVertsVec(int numpoints, finalvert_t *fv, const fmtrivertx_t *oldv,
	const fmtrivertx_t *newv)
{
	const float k1[21] = {
		r_lerp_move[0], r_lerp_move[1], r_lerp_move[2],
		r_lerp_backv[0], r_lerp_backv[1], r_lerp_backv[2],
		r_lerp_frontv[0], r_lerp_frontv[1], r_lerp_frontv[2],
		aliastransform[0][0], aliastransform[0][1], aliastransform[0][2], aliastransform[0][3],
		aliastransform[1][0], aliastransform[1][1], aliastransform[1][2], aliastransform[1][3],
		aliastransform[2][0], aliastransform[2][1], aliastransform[2][2], aliastransform[2][3]
	};
	union { float f; int i; const void *p; } k2[18];
	int n;
	const fmtrivertx_t *o, *nv;
	finalvert_t *f;

	n = numpoints; o = oldv; nv = newv; f = fv;
	__asm__ volatile(
		"    getsr   vl, #0x10\n"
		"    ldw     r9, [%[k], #0]\n   mov v9, r9\n"
		"    ldw     r9, [%[k], #4]\n   mov v10, r9\n"
		"    ldw     r9, [%[k], #8]\n   mov v11, r9\n"
		"    ldw     r9, [%[k], #12]\n  mov v12, r9\n"
		"    ldw     r9, [%[k], #16]\n  mov v13, r9\n"
		"    ldw     r9, [%[k], #20]\n  mov v14, r9\n"
		"    ldw     r9, [%[k], #24]\n  mov v15, r9\n"
		"    ldw     r9, [%[k], #28]\n  mov v16, r9\n"
		"    ldw     r9, [%[k], #32]\n  mov v17, r9\n"
		"    ldw     r9, [%[k], #36]\n  mov v18, r9\n"
		"    ldw     r9, [%[k], #40]\n  mov v19, r9\n"
		"    ldw     r9, [%[k], #44]\n  mov v20, r9\n"
		"    ldw     r9, [%[k], #48]\n  mov v21, r9\n"
		"    ldw     r9, [%[k], #52]\n  mov v22, r9\n"
		"    ldw     r9, [%[k], #56]\n  mov v23, r9\n"
		"    ldw     r9, [%[k], #60]\n  mov v24, r9\n"
		"    ldw     r9, [%[k], #64]\n  mov v25, r9\n"
		"    ldw     r9, [%[k], #68]\n  mov v26, r9\n"
		"    ldw     r9, [%[k], #72]\n  mov v27, r9\n"
		"    ldw     r9, [%[k], #76]\n  mov v28, r9\n"
		"    ldw     r9, [%[k], #80]\n  mov v29, r9\n"
		"1:\n"
		"    min     vl, vl, %[n]\n"
		"    sub     %[n], %[n], vl\n"
		"    ldub    v1, [%[o], #4]\n"
		"    add     r9, %[o], #1\n"
		"    ldub    v2, [r9, #4]\n"
		"    add     r9, %[o], #2\n"
		"    ldub    v3, [r9, #4]\n"
		"    ldub    v4, [%[nv], #4]\n"
		"    add     r9, %[nv], #1\n"
		"    ldub    v5, [r9, #4]\n"
		"    add     r9, %[nv], #2\n"
		"    ldub    v6, [r9, #4]\n"
		"    itof    v1, v1, z\n"
		"    itof    v2, v2, z\n"
		"    itof    v3, v3, z\n"
		"    itof    v4, v4, z\n"
		"    itof    v5, v5, z\n"
		"    itof    v6, v6, z\n"
		"    fmul    v1, v1, v12\n"
		"    fadd    v1, v9, v1\n"
		"    fmul    v4, v4, v15\n"
		"    fadd    v1, v1, v4\n"
		"    fmul    v2, v2, v13\n"
		"    fadd    v2, v10, v2\n"
		"    fmul    v5, v5, v16\n"
		"    fadd    v2, v2, v5\n"
		"    fmul    v3, v3, v14\n"
		"    fadd    v3, v11, v3\n"
		"    fmul    v6, v6, v17\n"
		"    fadd    v3, v3, v6\n"
		"    fmul    v4, v1, v18\n"
		"    fmul    v5, v2, v19\n"
		"    fadd    v4, v4, v5\n"
		"    fmul    v5, v3, v20\n"
		"    fadd    v4, v4, v5\n"
		"    fadd    v4, v4, v21\n"
		"    add     r9, %[f], #36\n"
		"    stw     v4, [r9, #48]\n"
		"    fmul    v5, v1, v22\n"
		"    fmul    v6, v2, v23\n"
		"    fadd    v5, v5, v6\n"
		"    fmul    v6, v3, v24\n"
		"    fadd    v5, v5, v6\n"
		"    fadd    v5, v5, v25\n"
		"    add     r9, %[f], #40\n"
		"    stw     v5, [r9, #48]\n"
		"    fmul    v6, v1, v26\n"
		"    fmul    v7, v2, v27\n"
		"    fadd    v6, v6, v7\n"
		"    fmul    v7, v3, v28\n"
		"    fadd    v6, v6, v7\n"
		"    fadd    v6, v6, v29\n"
		"    add     r9, %[f], #44\n"
		"    stw     v6, [r9, #48]\n"
		"    ldea    %[o], [%[o], vl*4]\n"
		"    ldea    %[nv], [%[nv], vl*4]\n"
		"    mul     r9, vl, #48\n"
		"    add     %[f], %[f], r9\n"
		"    bnz     %[n], 1b\n"
		: [n] "+r"(n), [o] "+r"(o), [nv] "+r"(nv), [f] "+r"(f)
		: [k] "r"(k1)
		: "vl", "r9", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v9", "v10", "v11", "v12", "v13",
		  "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25",
		  "v26", "v27", "v28", "v29", "memory"
	);

	k2[0].f = r_plightvec[0];
	k2[1].f = r_plightvec[1];
	k2[2].f = r_plightvec[2];
	k2[3].f = r_shadelight[0];
	k2[4].f = (float)r_ambientlight[0];
	k2[5].i = r_ambientlight[0];
	k2[6].f = aliasxscale;
	k2[7].f = aliasyscale;
	k2[8].f = aliasxcenter;
	k2[9].f = aliasycenter;
	k2[10].f = s_ziscale;
	k2[11].i = r_refdef.aliasvrect.x;
	k2[12].i = r_refdef.aliasvrect.y;
	k2[13].i = r_refdef.aliasvrectright;
	k2[14].i = r_refdef.aliasvrectbottom;
	k2[15].f = ALIAS_Z_CLIP_PLANE;
	k2[16].f = 1.0F;
	k2[17].p = r_avertexnormals;

	n = numpoints; nv = newv; f = fv;
	__asm__ volatile(
		"    getsr   vl, #0x10\n"
		"    ldw     r9, [%[k], #0]\n"
		"    ldw     r10, [%[k], #4]\n"
		"    ldw     r11, [%[k], #8]\n"
		"    ldw     r12, [%[k], #12]\n"
		"    ldw     r13, [%[k], #16]\n"
		"    ldw     r14, [%[k], #20]\n"
		"    ldw     r15, [%[k], #24]\n"
		"    ldw     r16, [%[k], #28]\n"
		"    ldw     r17, [%[k], #32]\n"
		"    ldw     r18, [%[k], #36]\n"
		"    ldw     r19, [%[k], #40]\n"
		"    ldw     r20, [%[k], #44]\n"
		"    ldw     r21, [%[k], #48]\n"
		"    ldw     r22, [%[k], #52]\n"
		"    ldw     r23, [%[k], #56]\n"
		"    ldw     r24, [%[k], #60]\n"
		"    ldw     r25, [%[k], #64]\n"
		"    mov     v20, r25\n"
		"    ldw     r25, [%[k], #68]\n"
		"1:\n"
		"    min     vl, vl, %[n]\n"
		"    sub     %[n], %[n], vl\n"
		"    add     r26, %[nv], #3\n"
		"    ldub    v1, [r26, #4]\n"
		"    mul     v1, v1, #12\n"
		"    ldw     v2, [r25, v1]\n"
		"    add     r26, r25, #4\n"
		"    ldw     v3, [r26, v1]\n"
		"    add     r26, r25, #8\n"
		"    ldw     v4, [r26, v1]\n"
		"    fmul    v2, v2, r9\n"
		"    fmul    v3, v3, r10\n"
		"    fadd    v2, v2, v3\n"
		"    fmul    v4, v4, r11\n"
		"    fadd    v2, v2, v4\n"
		"    fslt    v3, v2, z\n"
		"    fmul    v4, v2, r12\n"
		"    fadd    v4, v4, r13\n"
		"    ftoi    v4, v4, z\n"
		"    max     v4, v4, z\n"
		"    sel     v3, v4, r14\n"
		"    add     r26, %[f], #16\n"
		"    stw     v3, [r26, #48]\n"
		"    add     r26, %[f], #20\n"
		"    stw     v3, [r26, #48]\n"
		"    add     r26, %[f], #24\n"
		"    stw     v3, [r26, #48]\n"
		"    add     r26, %[f], #36\n"
		"    ldw     v5, [r26, #48]\n"
		"    add     r26, %[f], #40\n"
		"    ldw     v6, [r26, #48]\n"
		"    add     r26, %[f], #44\n"
		"    ldw     v7, [r26, #48]\n"
		"    fslt    v8, v7, r24\n"
		"    fdiv    v9, v20, v7\n"
		"    fmul    v10, v9, r19\n"
		"    ftoi    v10, v10, z\n"
		"    add     r26, %[f], #28\n"
		"    stw     v10, [r26, #48]\n"
		"    fmul    v11, v5, r15\n"
		"    fmul    v11, v11, v9\n"
		"    fadd    v11, v11, r17\n"
		"    ftoi    v11, v11, z\n"
		"    stw     v11, [%[f], #48]\n"
		"    fmul    v12, v6, r16\n"
		"    fmul    v12, v12, v9\n"
		"    fadd    v12, v12, r18\n"
		"    ftoi    v12, v12, z\n"
		"    add     r26, %[f], #4\n"
		"    stw     v12, [r26, #48]\n"
		"    slt     v13, v11, r20\n"
		"    and     v13, v13, #1\n"
		"    slt     v14, v12, r21\n"
		"    and     v14, v14, #2\n"
		"    or      v13, v13, v14\n"
		"    sle     v14, v11, r22\n"
		"    xor     v14, v14, #-1\n"
		"    and     v14, v14, #4\n"
		"    or      v13, v13, v14\n"
		"    sle     v14, v12, r23\n"
		"    xor     v14, v14, #-1\n"
		"    and     v14, v14, #8\n"
		"    or      v13, v13, v14\n"
		"    and     v14, v8, #16\n"
		"    sel     v8, v14, v13\n"
		"    add     r26, %[f], #32\n"
		"    stw     v8, [r26, #48]\n"
		"    ldea    %[nv], [%[nv], vl*4]\n"
		"    mul     r26, vl, #48\n"
		"    add     %[f], %[f], r26\n"
		"    bnz     %[n], 1b\n"
		: [n] "+r"(n), [nv] "+r"(nv), [f] "+r"(f)
		: [k] "r"(k2)
		: "vl", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17", "r18", "r19", "r20",
		  "r21", "r22", "r23", "r24", "r25", "r26", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8",
		  "v9", "v10", "v11", "v12", "v13", "v14", "v20", "memory"
	);
}
#endif

/*
================
R_AliasProjectAndClipTestFinalVert
================
*/
void
R_AliasProjectAndClipTestFinalVert( finalvert_t *fv )
{
	float	zi;
	float	x, y, z;

	// project points
	x = fv->xyz[0];
	y = fv->xyz[1];
	z = fv->xyz[2];
	zi = 1.0F / z;

	fv->cv.zi = zi * s_ziscale;

	fv->cv.u = (x * aliasxscale * zi) + aliasxcenter;
	fv->cv.v = (y * aliasyscale * zi) + aliasycenter;

	if (fv->cv.u < r_refdef.aliasvrect.x)
		fv->flags |= ALIAS_LEFT_CLIP;
	if (fv->cv.v < r_refdef.aliasvrect.y)
		fv->flags |= ALIAS_TOP_CLIP;
	if (fv->cv.u > r_refdef.aliasvrectright)
		fv->flags |= ALIAS_RIGHT_CLIP;
	if (fv->cv.v > r_refdef.aliasvrectbottom)
		fv->flags |= ALIAS_BOTTOM_CLIP;
}

/*
================
R_AliasSetupLighting

  FIXME: put lighting into tables
================
*/
static void
R_AliasSetupLighting(entity_t *currententity)
{
	const vec3_t lightvec = {-1, 0, 0};
	vec3_t light;
	int i;

	// all components of light should be identical in software
	if ( currententity->flags & RF_TRANS_ADD_ALPHA )
	{
		for (i=0 ; i<3 ; i++)
			light[i] = currententity->color.a / 255.0F;
	}
	else if ( currententity->flags & RF_FULLBRIGHT )
	{
		for (i=0 ; i<3 ; i++)
			light[i] = 1.0F;
	}
	else if ( currententity->absLight.r || currententity->absLight.g || currententity->absLight.b )
	{
		light[0] = currententity->absLight.r / 255.0F;
		light[1] = currententity->absLight.g / 255.0F;
		light[2] = currententity->absLight.b / 255.0F;
	}
	else
	{
		R_LightPoint (currententity, currententity->origin, light);
	}


	if ( currententity->flags & RF_MINLIGHT )
	{
		for (i=0 ; i<3 ; i++)
			if (light[i] < 0.1F)
				light[i] = 0.1F;
	}

	if ( currententity->flags & RF_GLOW )
	{	// bonus items will pulse with time
		const float val = sinf(r_newrefdef.time * 7.0F) * 0.3F + 0.7F;

		for (i=0 ; i<3 ; i++)
			light[i] = val;
	}

	if(r_colorlight->value == 0)
	{
		float temp = (light[0] + light[1] + light[2]) * (1.0F / 3.0F);

		light[0] = light[1] = light[2] = temp;
	}

	for(i=0; i<3; i++)
	{
		int j;

		j = light[i] * 255;

		r_ambientlight[i] = j;
		r_shadelight[i] = j;

		// clamp lighting so it doesn't overbright as much
		if (r_ambientlight[i] > 128)
			r_ambientlight[i] = 128;
		if (r_ambientlight[i] + r_shadelight[i] > 192)
			r_shadelight[i] = 192 - r_ambientlight[i];

		// guarantee that no vertex will ever be lit below LIGHT_MIN, so we don't have
		// to clamp off the bottom
		if (r_ambientlight[i] < LIGHT_MIN)
			r_ambientlight[i] = LIGHT_MIN;

		r_ambientlight[i] = (255 - r_ambientlight[i]) << VID_CBITS;

		if (r_shadelight[i] < 0)
			r_shadelight[i] = 0;

		r_shadelight[i] *= VID_GRADES;
	}

	// rotate the lighting vector into the model's frame of reference
	r_plightvec[0] =  DotProduct( lightvec, s_alias_forward );
	r_plightvec[1] = -DotProduct( lightvec, s_alias_right );
	r_plightvec[2] =  DotProduct( lightvec, s_alias_up );
}


/*
=================
R_AliasSetupFrames

=================
*/
static void
R_AliasSetupFrames(entity_t *currententity, const fmdl_t *pmdl)
{
	if (currententity->frame < 0 || currententity->frame >= pmdl->header.num_frames)
		currententity->frame = 0;
	if (currententity->oldframe < 0 || currententity->oldframe >= pmdl->header.num_frames)
		currententity->oldframe = 0;

	r_thisframe = (const fmaliasframe_t *)((const byte *)pmdl->frames + currententity->frame * pmdl->header.framesize);
	r_lastframe = (const fmaliasframe_t *)((const byte *)pmdl->frames + currententity->oldframe * pmdl->header.framesize);
}

/*
** R_AliasSetUpLerpData
**
** Lerp coefficients for the compressed vertices, scaled like Heretic2R's StandardFrameLerp.
*/
static void
R_AliasSetUpLerpData(const entity_t *currententity, float backlerp)
{
	const float frontlerp = 1.0F - backlerp;
	int i;

	for (i=0 ; i<3 ; i++)
	{
		r_lerp_move[i] = (backlerp * r_lastframe->translate[i] + frontlerp * r_thisframe->translate[i]) * r_alias_scale;
		r_lerp_frontv[i] = frontlerp * r_thisframe->scale[i] * r_alias_scale;
		r_lerp_backv[i]  = backlerp  * r_lastframe->scale[i] * r_alias_scale;
	}
}

finalvert_t *finalverts = NULL, *finalverts_max = NULL;

/*
 * The engine reads the vertices of MESH commands after the CPU has moved on, so each model gets
 * its own part of finalverts, and the buffer is only reused from the start once those are done.
 */
static finalvert_t *
R_AliasVertexBuffer(int numverts)
{
	static int next;
	finalvert_t *verts;

	if (!pe_alias || pe_version < 5)
		return finalverts;

	if (next + numverts >= r_numallocatedverts)
	{
		PE_WaitFor(r_alias_mesh_fence);
		next = 0;
	}

	verts = &finalverts[next];
	next += numverts;
	return verts;
}

extern void (*d_pdrawspans)(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_Opaque(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_33(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpans8_66(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpansConstant8_33(const entity_t *currententity, spanpackage_t *pspanpackage);
void R_PolysetDrawSpansConstant8_66(const entity_t *currententity, spanpackage_t *pspanpackage);

void R_PolysetDrawSpans8_Add(const entity_t *currententity, spanpackage_t *pspanpackage);
extern const byte *r_polyset_remap;

/*
================
R_AliasDrawModel
================
*/
void
R_AliasDrawModel(entity_t *currententity, const model_t *currentmodel)
{
	const byte *remap;
	qboolean lerped;
	int blend;

	s_pmdl = currentmodel->extradata;

	if ( r_lerpmodels->value == 0 )
		currententity->backlerp = 0;

	r_alias_scale = (currententity->scale > 0.0F) ? currententity->scale : 1.0F;

	R_AliasSetupFrames(currententity, s_pmdl);
	R_AliasSetUpTransform(currententity);

	if ( R_AliasCheckBBox(currententity) == BBOX_TRIVIAL_REJECT )
		return;

	if ( !R_AliasSkin(currententity, currentmodel) )
		return;

	r_amodels_drawn++;
	R_AliasSetupLighting(currententity);

	blend = R_EntityBlend(currententity, &remap);
	r_polyset_remap = remap;

	switch (blend)
	{
	case RB_ADD:
		d_pdrawspans = R_PolysetDrawSpans8_Add;
		break;
	case RB_33:
		d_pdrawspans = R_PolysetDrawSpans8_33;
		break;
	case RB_66:
		d_pdrawspans = R_PolysetDrawSpans8_66;
		break;
	default:
		d_pdrawspans = R_PolysetDrawSpans8_Opaque;
		break;
	}

	pe_alias = pe_active && d_pdrawspans == R_PolysetDrawSpans8_Opaque;
	if (!pe_alias)
		PE_Finish();

	/*
	** Skeleton joints (Corvus turning his head and torso) and the reference points that client
	** effects follow come from Heretic2R's FrameLerp. Other models lerp their compressed vertices.
	*/
	if (!(int)r_frameswap->value)
		currententity->swapFrame = NO_SWAP_FRAME;

	lerped = s_pmdl->skeletalType != SKEL_NULL ||
		(s_pmdl->referenceType != REF_NULL && currententity->referenceInfo && !(currententity->flags & RF_IGNORE_REFS));

	if (lerped)
		FrameLerp(s_pmdl, currententity);
	else
		R_AliasSetUpLerpData(currententity, currententity->backlerp);

	if (currententity->flags & RF_DEPTHHACK)
		s_ziscale = (float)(0x8000 * 3) * (float)SHIFT16XYZ_MULT;
	else
		s_ziscale = (float)0x8000 * (float)SHIFT16XYZ_MULT;

	R_AliasPreparePoints(currententity, currentmodel, R_AliasVertexBuffer(s_pmdl->header.num_xyz),
		finalverts_max, lerped);

	r_polyset_remap = NULL;
}
