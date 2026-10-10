/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * Shared declarations for the software renderer, on top of Heretic2R's refresh interface.
 */

#ifndef SRC_CLIENT_REFRESH_REF_SHARED_H_
#define SRC_CLIENT_REFRESH_REF_SHARED_H_

#include "client/ref.h"
#include "client/vid.h"
#include "qfiles.h"
#include "Angles.h"
#include "Matrix.h"
#include "Vector.h"

#undef DotProduct
#undef VectorSubtract
#undef VectorAdd
#undef VectorCopy
#undef VectorClear
#undef VectorNegate
#undef VectorSet
#undef VectorScale
#undef VectorMA
#undef VectorInverse

#define DotProduct(x, y)		((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorSubtract(a, b, c)	((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
#define VectorAdd(a, b, c)		((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#define VectorCopy(a, b)		((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define VectorClear(a)			((a)[0] = (a)[1] = (a)[2] = 0)
#define VectorNegate(a, b)		((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])
#define VectorSet(v, x, y, z)	((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
#define VectorScale(v, s, o)	((o)[0] = (v)[0] * (s), (o)[1] = (v)[1] * (s), (o)[2] = (v)[2] * (s))
#define VectorMA(v, s, b, o)	((o)[0] = (v)[0] + (b)[0] * (s), (o)[1] = (v)[1] + (b)[1] * (s), (o)[2] = (v)[2] + (b)[2] * (s))
#define VectorInverse(v)		((v)[0] = -(v)[0], (v)[1] = -(v)[1], (v)[2] = -(v)[2])

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

typedef float vec_t;
typedef vec_t vec5_t[5];

static inline int Q_ceil(float x) { return (int)ceilf(x); }
#define Q_ftol(f) (long)(f)
#define Q_malloc malloc

static inline int FloatToFixed8(float x) { return (int)(x * 128.0f); }
#define Q_min(a, b) ((a) < (b) ? (a) : (b))
#define Q_max(a, b) ((a) > (b) ? (a) : (b))
#define Q_strlcpy(dst, src, size) strcpy_s((dst), (size), (src))
#define Q_strncpyz(dst, src, size) strcpy_s((dst), (size), (src))

#define BOX_ON_PLANE_SIDE(emins, emaxs, p) \
	(((p)->type < 3) ? \
		(((p)->dist <= (emins)[(p)->type]) ? 1 : (((p)->dist >= (emaxs)[(p)->type]) ? 2 : 3)) : \
		BoxOnPlaneSide((emins), (emaxs), (p)))

static inline void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3])
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			out[i][j] = in1[i][0] * in2[0][j] + in1[i][1] * in2[1][j] + in1[i][2] * in2[2][j];
}

// Quake II features Heretic II doesn't have.
#define RF_BEAM			0
#define RDF_IRGOGGLES	0
#define RF_IR_VISIBLE	0

#define MAX_LBM_HEIGHT	480

#define PRINTF_ATTR(f, a) __attribute__((format(printf, f, a)))
#define MC1Q2_VLA(TYPE, VARNAME, NUMELEMS) TYPE VARNAME[NUMELEMS]
#define MC1Q2_VLAFREE(VARNAME)

#define MC1Q2_NO_SANITIZE(x)
#define MC1Q2_ALIGNAS_TYPE(t) __attribute__((aligned(sizeof(t))))
#define MAXLIGHTMAPS	4

// Entity angles arrive in radians; the renderer works in degrees like Quake II.
// Monochrome brightness of a dynamic light's color, 0..1.
#define R_DlightBrightness(dl)	((float)max((dl)->color.r, max((dl)->color.g, (dl)->color.b)) / 255.0f)

#define R_EntityAngles(e, out)	VectorScale((e)->angles, RAD_TO_ANGLE, out)

typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef enum
{
	mod_bad,
	mod_brush,
	mod_sprite,
	mod_alias,
	mod_book
} modtype_t;

extern void R_Printf(int level, const char* msg, ...) PRINTF_ATTR(2, 3);

extern float Mod_RadiusFromBounds(const vec3_t mins, const vec3_t maxs);
extern const byte* Mod_DecompressVis(const byte *in, int row);

enum {
	SIDE_FRONT = 0,
	SIDE_BACK = 1,
	SIDE_ON = 2
};

// Flags in msurface_t::flags (texinfo SURF_ flags are separate).
enum {
	SURF_PLANEBACK = 0x02,
	SURF_DRAWSKY = 0x04,
	SURF_DRAWTURB = 0x10,
	SURF_DRAWBACKGROUND = 0x40,
	SURF_UNDERWATER = 0x80
};

typedef struct mvertex_s
{
	vec3_t		position;
} mvertex_t;

typedef struct medge_s
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s
{
	float	vecs[2][4];
	int	flags;
	int	numframes;
	struct mtexinfo_s	*next; /* animation chain */
	struct image_s	*image;
} mtexinfo_t;

#define	CONTENTS_NODE	-1

typedef struct mnode_s
{
	int		contents;	/* CONTENTS_NODE, to differentiate from leafs */
	int		visframe;	/* node needs to be traversed if current */
	float		minmaxs[6];	/* for bounding box culling */
	struct mnode_s	*parent;

	cplane_t	*plane;
	struct mnode_s	*children[2];
	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
	int		contents;
	int		visframe;
	float		minmaxs[6];
	struct mnode_s	*parent;

	int		cluster;
	int		area;
	struct msurface_s	**firstmarksurface;
	int		nummarksurfaces;
	int		key;	/* BSP sequence number for leaf's contents */
} mleaf_t;

typedef struct image_s* (*findimage_t)(const char *name, imagetype_t type);

extern void Mod_LoadNodes(const char *name, cplane_t *planes, int numplanes,
	mleaf_t *leafs, int numleafs, mnode_t **nodes, int *numnodes,
	const byte *mod_base, const lump_t *l);
extern void Mod_LoadVertexes(const char *name, mvertex_t **vertexes, int *numvertexes,
	const byte *mod_base, const lump_t *l, int extra);
extern void Mod_LoadVisibility(dvis_t **vis, const byte *mod_base, const lump_t *l);
extern void Mod_LoadLighting(byte **lightdata, const byte *mod_base, const lump_t *l);
extern void Mod_LoadTexinfo(const char *name, mtexinfo_t **texinfo, int *numtexinfo,
	const byte *mod_base, const lump_t *l, findimage_t find_image,
	struct image_s *notexture, int extra);
extern void Mod_LoadEdges(const char *name, medge_t **edges, int *numedges,
	const byte *mod_base, const lump_t *l, int extra);
extern void Mod_LoadPlanes (const char *name, cplane_t **planes, int *numplanes,
	const byte *mod_base, const lump_t *l, int extra);
extern void Mod_LoadSurfedges (const char *name, int **surfedges, int *numsurfedges,
	const byte *mod_base, const lump_t *l, int extra);
extern int Mod_CalcLumpHunkSize(const lump_t *l, int inSize, int outSize, int extra);
extern mleaf_t *Mod_PointInLeaf(const vec3_t p, mnode_t *node);

#define DLIGHT_CUTOFF 64

typedef void (*marksurfacelights_t)(dlight_t *light, int bit, mnode_t *node,
	int r_dlightframecount);
extern void R_MarkLights (dlight_t *light, int bit, mnode_t *node, int r_dlightframecount,
	marksurfacelights_t mark_surface_lights);
extern struct image_s *R_TextureAnimation(const entity_t *currententity,
	const mtexinfo_t *tex);
extern qboolean R_AreaVisible(const byte *areabits, mleaf_t *pleaf);
extern qboolean R_CullBox(vec3_t mins, vec3_t maxs, cplane_t *frustum);
extern void R_SetFrustum(vec3_t vup, vec3_t vpn, vec3_t vright, vec3_t r_origin,
	float fov_x, float fov_y, cplane_t *frustum);

#endif /* SRC_CLIENT_REFRESH_REF_SHARED_H_ */
