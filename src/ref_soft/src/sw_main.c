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

// sw_main.c
#include <stdint.h>
#include <limits.h>

#include "header/local.h"
#include "header/flexmodel.h"
#include "platform.h"

#include "header/local.h"

#define NUMSTACKEDGES		2048
#define NUMSTACKSURFACES	1024
#define MAXALIASVERTS		2048
#define MAXLIGHTS		1024 // allow some very large lightmaps

viddef_t	vid;
pixel_t		*vid_buffer = NULL;
static vid_frame_t	*s_frame;
espan_t		*vid_polygon_spans = NULL;
pixel_t		*vid_colormap = NULL;
pixel_t		*vid_alphamap = NULL;
pixel_t		*vid_additivemap = NULL;
light_t		vid_lightthreshold = 0;
static int	vid_minu, vid_minv, vid_maxu, vid_maxv;
int	vid_zminu, vid_zminv, vid_zmaxu, vid_zmaxv;

// last position  on map
static vec3_t	lastvieworg;
static vec3_t	lastviewangles;
qboolean	fastmoving;
static qboolean	palette_changed;

refimport_t	ri;

byte		d_8to24table[256 * 4];

vec3_t		skyaxis;

refdef_t	r_newrefdef;

model_t		*r_worldmodel;

pixel_t		*r_warpbuffer;

typedef struct swstate_s
{
	qboolean	fullscreen;
	int		prev_mode; // last valid SW mode

	unsigned char	gammatable[256];
	unsigned char	currentpalette[768];
} swstate_t;

static swstate_t sw_state;

void	*colormap;
float	r_time1;
int	r_numallocatededges;
int	r_numallocatedverts;
int	r_numallocatedtriangles;
int	r_numallocatedlights;
int	r_numallocatededgebasespans;
float	r_aliasuvscale = 1.0F;
qboolean	r_outofsurfaces;
qboolean	r_outofedges;
qboolean	r_outofverts;
qboolean	r_outoftriangles;
qboolean	r_outoflights;
qboolean	r_outedgebasespans;

qboolean	r_dowarp;

mvertex_t	*r_pcurrentvertbase;

int		c_surf;
static int	r_cnumsurfs;
int	r_clipflags;

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

//
// screen size info
//
oldrefdef_t	r_refdef;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

cplane_t	screenedge[4];

//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		r_polycount;
int		r_drawnpolycount;

int		*pfrustum_indexes[4];
int			r_viewcluster, r_oldviewcluster;

image_t  	*r_notexture_mip;

float	da_time1, da_time2, dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2;

cvar_t	*sw_lodsize;
cvar_t	*r_lefthand;
cvar_t	*r_gunfov;
cvar_t	*r_farsee;
cvar_t	*r_lightmap;
cvar_t	*r_colorlight;
static cvar_t	*sw_aliasstats;
cvar_t	*sw_clearcolor;
cvar_t	*sw_drawflat;
cvar_t	*sw_draworder;
cvar_t  *sw_stipplealpha;
cvar_t	*sw_surfcacheoverride;
cvar_t	*sw_waterwarp;
cvar_t	*sw_custom_particles;
cvar_t	*sw_texture_filtering;
cvar_t	*r_retexturing;
cvar_t	*r_scale8bittextures;
cvar_t	*sw_gunzposition;
cvar_t	*r_validation;

cvar_t	*r_drawworld;
static cvar_t	*r_drawentities;
static cvar_t	*r_dspeeds;
cvar_t	*r_fullbright;
cvar_t  *r_lerpmodels;
static cvar_t  *r_novis;
cvar_t  *r_modulate;

static cvar_t	*r_speeds;
cvar_t	*r_lightlevel;	//FIXME HACK
cvar_t	*r_fixsurfsky;

static cvar_t	*vid_gamma;
static cvar_t	*vid_brightness;
static cvar_t	*vid_contrast;

static cvar_t	*r_lockpvs;
cvar_t	*r_cull;
cvar_t	*r_references;
cvar_t	*r_frameswap;
static cvar_t	*cl_camera_under_surface;
static cvar_t	*r_underwater_color;

// sw_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver


// d_vars.c

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

float	d_sdivzstepu, d_tdivzstepu;
float	d_sdivzstepv, d_tdivzstepv;
float	d_sdivzorigin, d_tdivzorigin;

int	sadjust, tadjust, bbextents, bbextentt;

pixel_t		*cacheblock;
int		cachewidth;
pixel_t		*d_viewbuffer;
zbufvalue_t	*d_pzbuffer;

static void RE_BeginFrame(float camera_separation);
static void Draw_BuildGammaTable(void);
static void RE_EndFrame(void);

int VID_GetTicks(void)
{
	return (int)(Sys_Microseconds() / 1000);
}


// clean z damage state
static void
VID_NoDamageZBuffer(void)
{
	vid_zminu = vid_buffer_width;
	vid_zmaxu = 0;
	vid_zminv = vid_buffer_height;
	vid_zmaxv = 0;
}

// Need to recalculate whole z buffer
static void
VID_WholeDamageZBuffer(void)
{
	vid_zminu = 0;
	vid_zmaxu = vid_buffer_width;
	vid_zminv = 0;
	vid_zmaxv = vid_buffer_height;
}

/*
================
VID_DamageBuffer

Mark VID buffer as damaged and need to be rewritten
================
*/
void
VID_DamageBuffer(int u, int v)
{
	// update U
	if (vid_minu > u)
	{
		vid_minu = u;
	}
	if (vid_maxu < u)
	{
		vid_maxu = u;
	}
	// update V
	if (vid_minv > v)
	{
		vid_minv = v;
	}
	if (vid_maxv < v)
	{
		vid_maxv = v;
	}
}

// clean damage state
static void
VID_NoDamageBuffer(void)
{
	vid_minu = vid_buffer_width;
	vid_maxu = 0;
	vid_minv = vid_buffer_height;
	vid_maxv = 0;
}

// Need to rewrite whole buffer
static void
VID_WholeDamageBuffer(void)
{
	vid_minu = 0;
	vid_maxu = vid_buffer_width;
	vid_minv = 0;
	vid_maxv = vid_buffer_height;
}

/*
================
R_InitTurb
================
*/
static void
R_InitTurb (int width)
{
	int		i;

	memset(blanktable, 0, (width+CYCLE) * sizeof(int));
	for (i = 0; i < (width+CYCLE); i++)
	{
		sintable[i] = AMP + sinf(i*(3.14159F*2.0F/CYCLE))*AMP;
		intsintable[i] = AMP2 + sinf(i*(3.14159F*2.0F/CYCLE))*AMP2; // AMP2, not 20
	}
}

void R_ImageList_f(void);
static void R_ScreenShot_f(void);

static void
R_RegisterVariables (void)
{
	sw_aliasstats = ri.Cvar_Get ("sw_polymodelstats", "0", 0);
	sw_clearcolor = ri.Cvar_Get ("sw_clearcolor", "2", 0);
	sw_drawflat = ri.Cvar_Get ("sw_drawflat", "0", 0);
	sw_draworder = ri.Cvar_Get ("sw_draworder", "0", 0);
	sw_mipcap = ri.Cvar_Get ("sw_mipcap", "0", 0);
	sw_mipscale = ri.Cvar_Get ("sw_mipscale", "1", 0);
	sw_lodsize = ri.Cvar_Get ("sw_lodsize", "48", CVAR_ARCHIVE);
	sw_stipplealpha = ri.Cvar_Get( "sw_stipplealpha", "0", CVAR_ARCHIVE );
	sw_surfcacheoverride = ri.Cvar_Get ("sw_surfcacheoverride", "0", 0);
	sw_waterwarp = ri.Cvar_Get ("sw_waterwarp", "1", 0);
	sw_custom_particles = ri.Cvar_Get("sw_custom_particles", "0", CVAR_ARCHIVE);
	sw_texture_filtering = ri.Cvar_Get("sw_texture_filtering", "0", CVAR_ARCHIVE);
	r_retexturing = ri.Cvar_Get("r_retexturing", "1", CVAR_ARCHIVE);
	r_scale8bittextures = ri.Cvar_Get("r_scale8bittextures", "0", CVAR_ARCHIVE);
	sw_gunzposition = ri.Cvar_Get("sw_gunzposition", "8", CVAR_ARCHIVE);
	r_validation = ri.Cvar_Get("r_validation", "0", CVAR_ARCHIVE);

	r_lefthand = ri.Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_gunfov = ri.Cvar_Get( "r_gunfov", "80", CVAR_ARCHIVE );
	r_farsee = ri.Cvar_Get("r_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
	r_lightmap = ri.Cvar_Get("r_lightmap", "0", 0);
	r_colorlight = ri.Cvar_Get("sw_colorlight", "0", CVAR_ARCHIVE);
	r_speeds = ri.Cvar_Get ("r_speeds", "0", 0);
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", 0);
	r_dspeeds = ri.Cvar_Get ("r_dspeeds", "0", 0);
	r_lightlevel = ri.Cvar_Get ("r_lightlevel", "0", 0);
	r_lerpmodels = ri.Cvar_Get( "r_lerpmodels", "1", 0 );
	r_novis = ri.Cvar_Get( "r_novis", "0", 0 );
	r_modulate = ri.Cvar_Get("r_modulate", "1", CVAR_ARCHIVE);
	r_fixsurfsky = ri.Cvar_Get("r_fixsurfsky", "0", CVAR_ARCHIVE);
	r_cull = ri.Cvar_Get("r_cull", "1", 0);
	r_references = ri.Cvar_Get("r_references", "1", 0);
	r_frameswap = ri.Cvar_Get("r_frameswap", "1", 0);
	cl_camera_under_surface = ri.Cvar_Get("cl_camera_under_surface", "0", 0);
	r_underwater_color = ri.Cvar_Get("r_underwater_color", "0x70c06000", 0);

	vid_gamma = ri.Cvar_Get( "vid_gamma", "0.5", CVAR_ARCHIVE );
	vid_brightness = ri.Cvar_Get( "vid_brightness", "0.5", CVAR_ARCHIVE );
	vid_contrast = ri.Cvar_Get( "vid_contrast", "0.5", CVAR_ARCHIVE );

	ri.Cmd_AddCommand("modellist", Mod_Modellist_f);
	ri.Cmd_AddCommand("screenshot", R_ScreenShot_f);
	ri.Cmd_AddCommand("imagelist", R_ImageList_f);

	vid_gamma->modified = true; // force us to rebuild the gamma table later

	r_lockpvs = ri.Cvar_Get ("r_lockpvs", "0", 0);
}

static void
R_UnRegister (void)
{
	ri.Cmd_RemoveCommand( "screenshot" );
	ri.Cmd_RemoveCommand( "modellist" );
	ri.Cmd_RemoveCommand( "imagelist" );
}

static void RE_ShutdownContext(void);
static void SWimp_CreateRender(int width, int height);
static qboolean RE_SetMode(void);
void LoadColormap(byte **colormap, byte *palette);

/*
===============
R_Init
===============
*/
static qboolean
RE_Init(void)
{
	R_RegisterVariables ();
	PE_Init ();

	LoadColormap (&vid_colormap, d_8to24table);
	vid_alphamap = vid_colormap + 64*256;

	R_InitImages ();
	R_BuildAdditiveMap ();
	Mod_Init ();
	Draw_InitLocal ();

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
			view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
			view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	r_aliasuvscale = 1.0F;

	if (!RE_SetMode())
	{
		R_Printf(PRINT_ALL, "%s() could not R_SetMode()\n", __func__);
		return false;
	}

	R_Printf(PRINT_ALL, "ref_soft version: "REF_VERSION"\n");

	return true;
}

/*
===============
RE_Shutdown
===============
*/
static void
R_FreeZBuffer(void)
{
	free(d_pzbuffer);
	d_pzbuffer = NULL;
}

static void
RE_Shutdown (void)
{
	PE_Finish();
	R_FreeZBuffer();
	// free surface cache
	if (sc_base)
	{
		D_FlushCaches ();
		free (sc_base);
		sc_base = NULL;
	}

	// free colormap
	if (vid_colormap)
	{
		free (vid_colormap);
		vid_colormap = NULL;
	}

	R_UnRegister ();
	Mod_FreeAll ();
	R_ShutdownImages ();

	RE_ShutdownContext();
}

/*
===============
R_NewMap
===============
*/
void
R_NewMap (void)
{
	r_viewcluster = -1;
}

static surf_t	*lsurfs;

/*
===============
R_ReallocateMapBuffers
===============
*/
static void
R_ReallocateMapBuffers (void)
{
	if (!r_cnumsurfs || r_outofsurfaces)
	{
		if(lsurfs)
		{
			free(lsurfs);
		}

		if (r_outofsurfaces)
		{
			r_cnumsurfs *= 2;
			r_outofsurfaces = false;
		}

		if ((r_farsee->value > 0) && (r_cnumsurfs < NUMSTACKSURFACES))
			r_cnumsurfs = NUMSTACKSURFACES * 2;
		else if (r_cnumsurfs < NUMSTACKSURFACES)
			r_cnumsurfs = NUMSTACKSURFACES;

		// edge_t->surf limited size to short
		if (r_cnumsurfs > SURFINDEX_MAX)
		{
			r_cnumsurfs = SURFINDEX_MAX;
			R_Printf(PRINT_ALL, "%s: Code has limitation to surfaces count.\n",
				 __func__);
		}

		lsurfs = malloc (r_cnumsurfs * sizeof(surf_t));
		if (!lsurfs)
		{
			R_Printf(PRINT_ALL, "%s: Couldn't malloc %d bytes\n",
				 __func__, (int)(r_cnumsurfs * sizeof(surf_t)));
			return;
		}

		surfaces = lsurfs;
		// set limits
		surf_max = &surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;

		surface_p = &surfaces[2];	// background is surface 1,
						//  surface 0 is a dummy

		R_Printf(PRINT_ALL, "Allocated %d surfaces.\n", r_cnumsurfs);
	}

	if (!r_numallocatedlights || r_outoflights)
	{
		if (!blocklights)
		{
			free(blocklights);
		}

		if (r_outoflights)
		{
			r_numallocatedlights *= 2;
			r_outoflights = false;
		}

		if (r_numallocatedlights < MAXLIGHTS)
			r_numallocatedlights = MAXLIGHTS;

		blocklights = malloc (r_numallocatedlights * sizeof(light_t));
		if (!blocklights)
		{
			R_Printf(PRINT_ALL, "%s: Couldn't malloc %d bytes\n",
				 __func__, (int)(r_numallocatedlights * sizeof(light_t)));
			return;
		}

		// set limits
		blocklight_max = &blocklights[r_numallocatedlights];

		R_Printf(PRINT_ALL, "Allocated %d lights.\n", r_numallocatedlights);
	}

	if (!r_numallocatededges || r_outofedges)
	{
		if (!r_edges)
		{
			free(r_edges);
		}

		if (r_outofedges)
		{
			r_numallocatededges *= 2;
			r_outofedges = false;
		}

		if ((r_farsee->value > 0) && (r_numallocatededges < NUMSTACKEDGES * 2))
			r_numallocatededges = NUMSTACKEDGES * 2;
		else if (r_numallocatededges < NUMSTACKEDGES)
			r_numallocatededges = NUMSTACKEDGES;

		r_edges = malloc (r_numallocatededges * sizeof(edge_t));
		if (!r_edges)
		{
			R_Printf(PRINT_ALL, "%s: Couldn't malloc %d bytes\n",
				 __func__, (int)(r_numallocatededges * sizeof(edge_t)));
			return;
		}

		// set limits
		edge_max = &r_edges[r_numallocatededges];
		edge_p = r_edges;

		R_Printf(PRINT_ALL, "Allocated %d edges.\n", r_numallocatededges);
	}

	if (!r_numallocatedverts || r_outofverts)
	{
		if (finalverts)
		{
			free(finalverts);
		}

		if (r_outofverts)
		{
			r_numallocatedverts *= 2;
			r_outofverts = false;
		}

		if (r_numallocatedverts < MAXALIASVERTS)
			r_numallocatedverts = MAXALIASVERTS;

		finalverts = Q_malloc(r_numallocatedverts * sizeof(finalvert_t));
		if (!finalverts)
		{
			R_Printf(PRINT_ALL, "%s: Couldn't malloc %d bytes\n",
				 __func__, (int)(r_numallocatedverts * sizeof(finalvert_t)));
			return;
		}
		finalverts_max = &finalverts[r_numallocatedverts];

		R_Printf(PRINT_ALL, "Allocated %d verts.\n", r_numallocatedverts);
	}

	if (!r_numallocatedtriangles || r_outoftriangles)
	{
		if (triangle_spans)
		{
			free(triangle_spans);
		}

		if (r_outoftriangles)
		{
			r_numallocatedtriangles *= 2;
			r_outoftriangles = false;
		}

		if (r_numallocatedtriangles < vid.height)
			r_numallocatedtriangles = vid.height;

		triangle_spans  = Q_malloc(r_numallocatedtriangles * sizeof(spanpackage_t));
		if (!triangle_spans)
		{
			R_Printf(PRINT_ALL, "%s: Couldn't malloc %d bytes\n",
				 __func__, (int)(r_numallocatedtriangles * sizeof(spanpackage_t)));
			return;
		}
		triangles_max = &triangle_spans[r_numallocatedtriangles];

		R_Printf(PRINT_ALL, "Allocated %d triangles.\n", r_numallocatedtriangles);
	}

	if (!r_numallocatededgebasespans || r_outedgebasespans)
	{
		if (edge_basespans)
		{
			PE_Finish();
			free(edge_basespans);
		}

		if (r_outedgebasespans)
		{
			r_numallocatededgebasespans *= 2;
			r_outedgebasespans = false;
		}

		// used up to 8 * width spans for render, allocate once  before use
		if (r_numallocatededgebasespans < vid_buffer_width * (pe_present ? 64 : 8))
			r_numallocatededgebasespans = vid_buffer_width * (pe_present ? 64 : 8);

		edge_basespans  = Q_malloc(r_numallocatededgebasespans * sizeof(espan_t));
		if (!edge_basespans)
		{
			R_Printf(PRINT_ALL, "%s: Couldn't malloc %d bytes\n",
				 __func__, (int)(r_numallocatededgebasespans * sizeof(espan_t)));
			return;
		}
		max_span_p = &edge_basespans[r_numallocatededgebasespans];

		R_Printf(PRINT_ALL, "Allocated %d edgespans.\n", r_numallocatededgebasespans);
	}
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
static void
R_MarkLeaves (void)
{
	const byte	*vis;
	mnode_t	*node;
	int		i;
	mleaf_t	*leaf;

	if (r_oldviewcluster == r_viewcluster && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if (r_lockpvs->value)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;

	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_worldmodel->numleafs ; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldmodel->numnodes ; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldmodel);

	for (i=0,leaf=r_worldmodel->leafs ; i<r_worldmodel->numleafs ; i++, leaf++)
	{
		int cluster;

		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

/*
** R_DrawNullModel
**
** IMPLEMENT THIS!
*/
static void
R_DrawEntity (entity_t *currententity)
{
	const model_t *currentmodel = R_EntityModel(currententity);

	if (!currentmodel)
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	switch (currentmodel->type)
	{
	case mod_sprite:
		R_DrawSprite(currententity, currentmodel);
		break;

	case mod_alias:
		R_AliasDrawModel(currententity, currentmodel);
		break;

	default:
		break;
	}
}

/*
=============
R_DrawEntitiesOnList

Brush models were drawn with the world. Alpha entities are drawn after the alpha surfaces,
farthest first.
=============
*/
static void
R_DrawEntitiesOnList (void)
{
	int i;

	if (!r_drawentities->value)
		return;

	for (i=0 ; i<r_newrefdef.num_entities ; i++)
		R_DrawEntity(r_newrefdef.entities[i]);
}

static int
R_AlphaEntityCompare(const void *a, const void *b)
{
	const float da = (*(const entity_t * const *)a)->depth;
	const float db = (*(const entity_t * const *)b)->depth;

	return (da < db) - (da > db);
}

static void
R_DrawAlphaEntitiesOnList (void)
{
	entity_t *sorted[MAX_ALPHA_ENTITIES];
	int i, count;

	if (!r_drawentities->value)
		return;

	count = Q_min(r_newrefdef.num_alpha_entities, MAX_ALPHA_ENTITIES);
	memcpy(sorted, r_newrefdef.alpha_entities, count * sizeof(entity_t *));
	qsort(sorted, count, sizeof(entity_t *), R_AlphaEntityCompare);

	for (i = 0; i < count; i++)
		R_DrawEntity(sorted[i]);
}

static int
R_BmodelCheckBBox (const float *minmaxs)
{
	int i, clipflags;

	clipflags = 0;

	for (i=0 ; i<4 ; i++)
	{
		vec3_t acceptpt, rejectpt;
		int *pindex;
		float d;

		// generate accept and reject points
		// FIXME: do with fast look-ups or integer tests based on the sign bit
		// of the floating point values
		pindex = pfrustum_indexes[i];

		rejectpt[0] = minmaxs[pindex[0]];
		rejectpt[1] = minmaxs[pindex[1]];
		rejectpt[2] = minmaxs[pindex[2]];

		d = DotProduct (rejectpt, view_clipplanes[i].normal);
		d -= view_clipplanes[i].dist;

		if (d <= 0)
			return BMODEL_FULLY_CLIPPED;

		acceptpt[0] = minmaxs[pindex[3+0]];
		acceptpt[1] = minmaxs[pindex[3+1]];
		acceptpt[2] = minmaxs[pindex[3+2]];

		d = DotProduct (acceptpt, view_clipplanes[i].normal);
		d -= view_clipplanes[i].dist;

		if (d <= 0)
			clipflags |= (1<<i);
	}

	return clipflags;
}


/*
===================
R_FindTopnode

Find the first node that splits the given box
===================
*/
static mnode_t *
R_FindTopnode (vec3_t mins, vec3_t maxs)
{
	mnode_t *node;

	node = r_worldmodel->nodes;

	while (1)
	{
		cplane_t *splitplane;
		int sides;

		if (node->visframe != r_visframecount)
			return NULL;		// not visible at all

		if (node->contents != CONTENTS_NODE)
		{
			if (node->contents != CONTENTS_SOLID)
				return	node;	// we've reached a non-solid leaf, so it's
						//  visible and not BSP clipped
			return NULL;	// in solid, so not visible
		}

		splitplane = node->plane;
		sides = BOX_ON_PLANE_SIDE(mins, maxs, splitplane);

		if (sides == 3)
			return node;	// this is the splitter

		// not split yet; recurse down the contacted side
		if (sides & 1)
			node = node->children[0];
		else
			node = node->children[1];
	}
}


/*
=============
RotatedBBox

Returns an axially aligned box that contains the input box at the given rotation
=============
*/
static void
RotatedBBox (const vec3_t mins, const vec3_t maxs, vec3_t angles, vec3_t tmins, vec3_t tmaxs)
{
	vec3_t	tmp, v;
	int		i, j;
	vec3_t	forward, right, up;

	if (!angles[0] && !angles[1] && !angles[2])
	{
		VectorCopy (mins, tmins);
		VectorCopy (maxs, tmaxs);
		return;
	}

	for (i=0 ; i<3 ; i++)
	{
		tmins[i] = (vec_t)INT_MAX; // Set maximum values for world range
		tmaxs[i] = (vec_t)INT_MIN;  // Set minimal values for world range
	}

	AngleVectors (angles, forward, right, up);

	for ( i = 0; i < 8; i++ )
	{
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


		VectorScale (forward, tmp[0], v);
		VectorMA (v, -tmp[1], right, v);
		VectorMA (v, tmp[2], up, v);

		for (j=0 ; j<3 ; j++)
		{
			if (v[j] < tmins[j])
				tmins[j] = v[j];
			if (v[j] > tmaxs[j])
				tmaxs[j] = v[j];
		}
	}
}

/*
=============
R_DrawBEntitiesOnList
=============
*/
static void
R_DrawBEntitiesOnList (void)
{
	int		i, clipflags;
	vec3_t		oldorigin;
	vec3_t		mins, maxs;
	float		minmaxs[6];
	mnode_t		*topnode;

	if (!r_drawentities->value)
		return;

	VectorCopy (modelorg, oldorigin);

	for (i=0 ; i<r_newrefdef.num_entities + r_newrefdef.num_alpha_entities ; i++)
	{
		entity_t *currententity = (i < r_newrefdef.num_entities) ?
			r_newrefdef.entities[i] : r_newrefdef.alpha_entities[i - r_newrefdef.num_entities];
		const model_t *currentmodel = R_EntityModel(currententity);
		vec3_t angles;

		if (!currentmodel)
			continue;
		if (currentmodel->nummodelsurfaces == 0)
			continue;	// clip brush only
		if (currentmodel->type != mod_brush)
			continue;
		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
		R_EntityAngles(currententity, angles);
		RotatedBBox (currentmodel->mins, currentmodel->maxs, angles, mins, maxs);
		VectorAdd (mins, currententity->origin, minmaxs);
		VectorAdd (maxs, currententity->origin, (minmaxs+3));

		clipflags = R_BmodelCheckBBox (minmaxs);
		if (clipflags == BMODEL_FULLY_CLIPPED)
			continue;	// off the edge of the screen

		topnode = R_FindTopnode (minmaxs, minmaxs+3);
		if (!topnode)
			continue;	// no part in a visible leaf

		VectorCopy (currententity->origin, r_entorigin);
		VectorSubtract (r_origin, r_entorigin, modelorg);

		r_pcurrentvertbase = currentmodel->vertexes;

		// FIXME: stop transforming twice
		R_RotateBmodel(currententity);

		// calculate dynamic lighting for bmodel
		R_PushDlights (currentmodel);

		if (topnode->contents == CONTENTS_NODE)
		{
			// not a leaf; has to be clipped to the world BSP
			r_clipflags = clipflags;
			R_DrawSolidClippedSubmodelPolygons(currententity, currentmodel, topnode);
		}
		else
		{
			// falls entirely in one leaf, so we just put all the
			// edges in the edge list and let 1/z sorting handle
			// drawing order
			R_DrawSubmodelPolygons(currententity, currentmodel, clipflags, topnode);
		}

		// put back world rotation and frustum clipping
		// FIXME: R_RotateBmodel should just work off base_vxx
		VectorCopy (base_vpn, vpn);
		VectorCopy (base_vup, vup);
		VectorCopy (base_vright, vright);
		VectorCopy (oldorigin, modelorg);
		R_TransformFrustum ();
	}
}

/*
================
R_EdgeDrawing

Render the map
================
*/
static void
R_EdgeDrawing (entity_t *currententity)
{
	surf_t *world_end;

	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
		return;

	// Set function pointer pdrawfunc used later in this function
	R_BeginEdgeFrame ();
	edge_p = r_edges;
	surface_p = &surfaces[2];	// background is surface 1,
					//  surface 0 is a dummy

	if (r_dspeeds->value)
	{
		rw_time1 = VID_GetTicks();
	}

	// Build the Global Edget Table
	// Also populate the surface stack and count # surfaces to render (surf_max is the max)
	R_RenderWorld (currententity);
	world_end = surface_p;

	if (r_dspeeds->value)
	{
		rw_time2 = VID_GetTicks();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList ();

	if (r_dspeeds->value)
	{
		db_time2 = VID_GetTicks();
		se_time1 = db_time2;
	}

	// Use the Global Edge Table to maintin the Active Edge Table: Draw the world as scanlines
	// Write the Z-Buffer (but no read)
	if (pe_world)
		PE_DrawWorld (currententity, world_end, surface_p);
	else
		R_ScanEdges (currententity, surface_p);
}

//=======================================================================

static void	R_GammaCorrectAndSetPalette(const unsigned char *palette);

/*
=============
R_CalcPalette

=============
*/
static void
R_CalcPalette (void)
{
	static qboolean modified;
	byte	palette[256][4], *in, *out;
	int		i, j;
	float	alpha, one_minus_alpha;
	vec3_t	premult;
	int		v;

	alpha = r_newrefdef.blend[3];
	if (alpha <= 0)
	{
		if (modified)
		{	// set back to default
			modified = false;
			R_GammaCorrectAndSetPalette( d_8to24table );
			return;
		}
		return;
	}

	modified = true;
	if (alpha > 1)
		alpha = 1;

	premult[0] = r_newrefdef.blend[0]*alpha*255;
	premult[1] = r_newrefdef.blend[1]*alpha*255;
	premult[2] = r_newrefdef.blend[2]*alpha*255;

	one_minus_alpha = (1.0F - alpha);

	in = d_8to24table;
	out = palette[0];
	for (i=0 ; i<256 ; i++, in+=4, out+=4)
	{
		for (j=0 ; j<3 ; j++)
		{
			v = premult[j] + one_minus_alpha * in[j];
			if (v > 255)
				v = 255;
			out[j] = v;
		}
		out[3] = 255;
	}

	R_GammaCorrectAndSetPalette( ( const unsigned char * ) palette[0] );
}

//=======================================================================

static void
R_SetLightLevel (const entity_t *currententity)
{
	vec3_t		light;

	if ((r_newrefdef.rdflags & RDF_NOWORLDMODEL) || (!r_drawentities->value) || (!currententity))
	{
		r_lightlevel->value = 150.0F;
		return;
	}

	// save off light value for server to look at (BIG HACK!)
	R_LightPoint (currententity, r_newrefdef.vieworg, light);
	r_lightlevel->value = 150.0F * light[0];
}

static int
VectorCompareRound(const vec3_t v1, const vec3_t v2)
{
	if ((int)(v1[0] - v2[0]) || (int)(v1[1] - v2[1]) || (int)(v1[2] - v2[2]))
	{
		return 0;
	}

	return 1;
}

cplane_t frustum[4];


/*
================
RE_RenderFrame

================
*/
static void
R_RenderView (const refdef_t *fd)
{
	r_newrefdef = *fd;
	entity_t	ent;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		ri.Sys_Error(ERR_FATAL, "%s: NULL worldmodel", __func__);
	}

	// Need to rerender whole frame
	VID_WholeDamageBuffer();

	VectorCopy (fd->vieworg, r_refdef.vieworg);
	VectorCopy (fd->viewangles, r_refdef.viewangles);

	// compare current position with old
	if (vid_buffer_width <= 640 ||
	    !VectorCompareRound(fd->vieworg, lastvieworg) ||
	    !VectorCompare(fd->viewangles, lastviewangles))
	{
		fastmoving = true;
	}
	else
	{
		fastmoving = false;
	}

	// save position for next check
	VectorCopy (fd->vieworg, lastvieworg);
	VectorCopy (fd->viewangles, lastviewangles);

	if (r_speeds->value || r_dspeeds->value)
		r_time1 = VID_GetTicks();

	R_SetupFrame ();
	PE_BeginFrame ();

	R_SetFrustum(vup, vpn, vright, r_origin, r_newrefdef.fov_x, r_newrefdef.fov_y,
		frustum);

	// Using the current view cluster (r_viewcluster), retrieve and decompress
	// the PVS (Potentially Visible Set)
	R_MarkLeaves ();	// done here so we know if we're in water

	// For each dlight_t* passed via r_newrefdef.dlights, mark polygons affected by a light.
	R_PushDlights (r_worldmodel);

	// TODO: rearrange code same as in GL*_DrawWorld?
	/* auto cycle the world frame for texture animation */
	memset(&ent, 0, sizeof(ent));
	ent.frame = (int)(r_newrefdef.time * 2);

	// Build the Global Edge Table and render it via the Active Edge Table
	// Render the map
	R_EdgeDrawing (&ent);

	if (r_dspeeds->value)
	{
		se_time2 = VID_GetTicks();
		de_time1 = se_time2;
	}

	if (fastmoving)
	{
		// redraw all
		VID_WholeDamageZBuffer();
	}
	else
	{
		// No Z rewrite required
		VID_NoDamageZBuffer();
	}
	// Draw enemies, barrel etc...
	// Use Z-Buffer mostly in read mode only.
	R_DrawEntitiesOnList ();

	if (r_dspeeds->value)
	{
		de_time2 = VID_GetTicks();
		dp_time1 = de_time2;
	}

	// Duh !
	R_DrawParticles ();

	if (r_dspeeds->value)
		dp_time2 = VID_GetTicks();

	// Perform pixel palette blending ia the pics/colormap.pcx lower part lookup table.
	R_DrawAlphaSurfaces(&ent);
	R_DrawAlphaEntitiesOnList ();

	// Save off light value for server to look at (BIG HACK!)
	R_SetLightLevel (&ent);

	if (r_dowarp)
		D_WarpScreen ();

	if (r_dspeeds->value)
	{
		de_time2 = VID_GetTicks();
		dp_time1 = de_time2;
	}

	// Modify the palette (when taking hit or pickup item) so all colors are modified
	R_CalcPalette ();

	if (sw_aliasstats->value)
		R_PrintAliasStats ();

	if (r_speeds->value)
		R_PrintTimes ();

	if (r_dspeeds->value)
		R_PrintDSpeeds ();

	R_ReallocateMapBuffers();
}

/*
** R_InitGraphics
*/
void RE_Draw_FadeScreen(paletteRGBA_t color);

static int
RE_RenderFrame (const refdef_t *fd)
{
	paletteRGBA_t color;

	if (cl_camera_under_surface->value)
		color.c = strtoul(r_underwater_color->string, NULL, 0);
	else
		color.c = ri.Is_Screen_Flashing();

	if (color.a != 255)
	{
		R_RenderView (fd);

		if (color.a == 0)
			return 0;
	}

	RE_Draw_FadeScreen(color);
	ri.Deactivate_Screen_Flash();

	return 0;
}

/*
** R_InitGraphics
*/
static void
R_InitGraphics( int width, int height )
{
	PE_Finish();
	R_FreeZBuffer();

	// free surface cache
	if ( sc_base )
	{
		D_FlushCaches();
		free(sc_base);
		sc_base = NULL;
	}

	d_pzbuffer = Q_malloc(width * height * sizeof(zbufvalue_t));

	R_InitCaches();

	R_GammaCorrectAndSetPalette(d_8to24table);
}

/*
** RE_BeginFrame
*/
static void
RE_BeginFrame( float camera_separation )
{
	fastmoving = false;

	if ( vid_gamma->modified || vid_brightness->modified || vid_contrast->modified )
	{
		Draw_BuildGammaTable();
		R_GammaCorrectAndSetPalette(d_8to24table);

		vid_gamma->modified = false;
		vid_brightness->modified = false;
		vid_contrast->modified = false;
	}
}

/*
==================
RE_SetMode
==================
*/
static qboolean
RE_SetMode(void)
{
	const int mode = (int)ri.Cvar_Get("vid_mode", "1", CVAR_ARCHIVE)->value;

	if (!ri.Vid_GetModeInfo(&vid.width, &vid.height, mode) &&
		!ri.Vid_GetModeInfo(&vid.width, &vid.height, 1))
	{
		return false;
	}

	R_Printf(PRINT_ALL, "Setting mode %d: %dx%d\n", mode, vid.width, vid.height);

	return ri.GLimp_InitGraphics(vid.width, vid.height);
}

/*
** R_GammaCorrectAndSetPalette
*/
static void
R_GammaCorrectAndSetPalette( const unsigned char *palette )
{
	unsigned char rgb[768];
	int i;

	for (i = 0; i < 256; i++)
	{
		rgb[i * 3 + 0] = sw_state.gammatable[palette[i * 4 + 0]];
		rgb[i * 3 + 1] = sw_state.gammatable[palette[i * 4 + 1]];
		rgb[i * 3 + 2] = sw_state.gammatable[palette[i * 4 + 2]];
	}

	if (memcmp(rgb, sw_state.currentpalette, sizeof(rgb)))
	{
		memcpy(sw_state.currentpalette, rgb, sizeof(rgb));
		VIDMC1_SetPalette(rgb);
	}
}

// Heretic II's gamma, brightness and contrast curve (see Heretic2R's R_InitGammaTable).
static void
Draw_BuildGammaTable (void)
{
	const float gamma = vid_gamma->value;
	const float brightness = vid_brightness->value;
	float contrast = 1.0f - vid_contrast->value;
	int i;

	if (contrast > 0.5f)
		contrast = powf(contrast + 0.5f, 3.0f);
	else
		contrast = powf(contrast + 0.5f, 0.5f);

	sw_state.gammatable[0] = 0;

	for (i = 1; i < 256; i++)
	{
		float inf = 255.0f * powf(((float)i + 0.5f) / 255.5f, gamma) + 0.5f;
		float sign;

		if (inf < 128.0f)
		{
			inf = 128.0f - inf;
			sign = -1.0f;
		}
		else
		{
			inf -= 128.0f;
			sign = 1.0f;
		}

		inf = (brightness * 160.0f - 80.0f) + (powf(inf / 128.0f, contrast) * sign + 1.0f) * 128.0f;

		sw_state.gammatable[i] = (byte)ClampI((int)inf, 0, 255);
	}
}

/*
============
RE_SetSky
============
*/
// 3dstudio environment map names
static const char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
static const int	r_skysideimage[6] = {5, 2, 4, 1, 0, 3};
extern mtexinfo_t		r_skytexinfo[6];

static void
RE_SetSky (const char *name, float rotate, const vec3_t axis)
{
	int		i;

	VectorCopy (axis, skyaxis);

	for (i=0 ; i<6 ; i++)
	{
		char path[MAX_QPATH];
		image_t	*image;

		Com_sprintf(path, sizeof(path), "pics/skies/%s%s.m8", name, suf[r_skysideimage[i]]);
		image = R_FindImage(path, it_sky);

		if (!image)
		{
			R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, path);
			image = r_notexture_mip;
		}

		r_skytexinfo[i].image = image;
	}
}

/*
===============
RE_RegisterSkin
===============
*/
static struct image_s *
RE_RegisterSkin (const char *name, qboolean *retval)
{
	image_t *image = R_FindImage (name, it_skin);

	if (retval)
		*retval = (image != NULL);

	return image;
}

void R_Printf(int level, const char* msg, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, msg);
	vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	ri.Con_Printf(level, "%s", text);
}

static int RE_PrepareForWindow(void)
{
	return 0;
}

static int
RE_GetReferencedID(const struct model_s *model)
{
	const fmdl_t *fmdl = model->extradata;

	if (model->type == mod_alias && fmdl->referenceType > REF_NULL && fmdl->referenceType < NUM_REFERENCED)
		return fmdl->referenceType;

	return REF_NULL;
}

// Exported to client effects, which never call it (see Heretic2R's client.h).
static int
RE_FindSurface(const vec3_t start, const vec3_t end, struct Surface_s *surface)
{
	return -1;
}

static int RE_InitContext(void *win);

void RE_Draw_GetPicSize(int *w, int *h, const char *name);
void RE_Draw_Pic(int x, int y, int scale, const char *name, float alpha);
void RE_Draw_StretchPic(int x, int y, int w, int h, const char *name, float alpha, DrawStretchPicScaleMode_t mode);
void RE_Draw_Char(int x, int y, int scale, int c, paletteRGBA_t color, qboolean draw_shadow);
void RE_Draw_TileClear(int x, int y, int w, int h, const char *name);
void RE_Draw_Fill(int x, int y, int w, int h, paletteRGBA_t color);
void RE_Draw_BigFont(int x, int y, const char *text, float alpha);
int RE_BF_Strlen(const char *text);
void RE_Draw_BookPic(const char *name, float scale, float alpha);
void RE_Draw_InitCinematic(int width, int height);
void RE_Draw_CloseCinematic(void);
void RE_Draw_Cinematic(const byte *data, const paletteRGB_t *palette);
void RE_Draw_Name(const vec3_t origin, const char *name, paletteRGBA_t color);

/*
===============
GetRefAPI
===============
*/
refexport_t
GetRefAPI(const refimport_t imp)
{
	refexport_t	refexport;

	memset(&refexport, 0, sizeof(refexport_t));
	ri = imp;

	refexport.api_version = REF_API_VERSION;
	refexport.title = "Software";

	refexport.Init = RE_Init;
	refexport.Shutdown = RE_Shutdown;

	refexport.BeginRegistration = RE_BeginRegistration;
	refexport.RegisterModel = RE_RegisterModel;
	refexport.RegisterSkin = RE_RegisterSkin;
	refexport.RegisterPic = RE_Draw_FindPic;
	refexport.SetSky = RE_SetSky;
	refexport.EndRegistration = RE_EndRegistration;
	refexport.GetReferencedID = RE_GetReferencedID;

	refexport.RenderFrame = RE_RenderFrame;

	refexport.DrawGetPicSize = RE_Draw_GetPicSize;
	refexport.DrawPic = RE_Draw_Pic;
	refexport.DrawStretchPic = RE_Draw_StretchPic;
	refexport.DrawChar = RE_Draw_Char;
	refexport.DrawTileClear = RE_Draw_TileClear;
	refexport.DrawFill = RE_Draw_Fill;
	refexport.DrawFadeScreen = RE_Draw_FadeScreen;
	refexport.DrawBigFont = RE_Draw_BigFont;
	refexport.BF_Strlen = RE_BF_Strlen;
	refexport.BookDrawPic = RE_Draw_BookPic;
	refexport.DrawInitCinematic = RE_Draw_InitCinematic;
	refexport.DrawCloseCinematic = RE_Draw_CloseCinematic;
	refexport.DrawCinematic = RE_Draw_Cinematic;
	refexport.Draw_Name = RE_Draw_Name;

	refexport.BeginFrame = RE_BeginFrame;
	refexport.EndFrame = RE_EndFrame;

	refexport.FindSurface = RE_FindSurface;

	refexport.InitContext = RE_InitContext;
	refexport.ShutdownContext = RE_ShutdownContext;
	refexport.PrepareForWindow = RE_PrepareForWindow;

	return refexport;
}

int vid_buffer_height = 0;
int vid_buffer_width = 0;

static int
RE_InitContext(void *win)
{
	s_frame = win;

	if (s_frame->pitch != s_frame->width)
	{
		R_Printf(PRINT_ALL, "%s: the frame buffer must have no row padding\n", __func__);
		return false;
	}

	vid_buffer_width = s_frame->width;
	vid_buffer_height = s_frame->height;

	R_InitGraphics(vid_buffer_width, vid_buffer_height);
	SWimp_CreateRender(vid_buffer_width, vid_buffer_height);

	return true;
}

static void
RE_ShutdownContext(void)
{
	PE_Finish();
	vid_buffer = NULL;

	if (sintable)
	{
		free(sintable);
	}
	sintable = NULL;

	if (intsintable)
	{
		free(intsintable);
	}
	intsintable = NULL;

	if (blanktable)
	{
		free(blanktable);
	}
	blanktable = NULL;

	if (vid_polygon_spans)
	{
		free(vid_polygon_spans);
	}
	vid_polygon_spans = NULL;

	if (newedges)
	{
		free(newedges);
	}
	newedges = NULL;

	if (removeedges)
	{
		free(removeedges);
	}
	removeedges = NULL;

	if (triangle_spans)
	{
		free(triangle_spans);
	}
	triangle_spans = NULL;

	if (warp_rowptr)
	{
		free(warp_rowptr);
	}
	warp_rowptr = NULL;

	if (warp_column)
	{
		free(warp_column);
	}
	warp_column = NULL;

	if (edge_basespans)
	{
		free(edge_basespans);
	}
	edge_basespans = NULL;

	if (finalverts)
	{
		free(finalverts);
	}
	finalverts = NULL;

	if(blocklights)
	{
		free(blocklights);
	}
	blocklights = NULL;

	if(r_edges)
	{
		free(r_edges);
	}
	r_edges = NULL;

	if(lsurfs)
	{
		free(lsurfs);
	}
	lsurfs = NULL;

	if(r_warpbuffer)
	{
		free(r_warpbuffer);
	}
	r_warpbuffer = NULL;

}

/*
/*
be careful if you ever want to change width: 12.20 fixed
point math used in R_ScanEdges() overflows at width 2048 !!
*/
char shift_size;

static void
RE_EndFrame (void)
{
	PE_Finish();

	VIDMC1_EndFrame(s_frame);
	vid_buffer = s_frame->pixels;

	VID_NoDamageBuffer();
}

static void
SWimp_CreateRender(int width, int height)
{
	vid_buffer = s_frame->pixels;
	// Need to rewrite whole frame
	VID_WholeDamageBuffer();

	sintable = Q_malloc((width+CYCLE) * sizeof(int));
	intsintable = Q_malloc((width+CYCLE) * sizeof(int));
	blanktable = Q_malloc((width+CYCLE) * sizeof(int));

	newedges = Q_malloc(width * sizeof(edge_t *));
	removeedges = Q_malloc(width * sizeof(edge_t *));

	warp_rowptr = Q_malloc((width+AMP2*2) * sizeof(byte*));
	warp_column = Q_malloc((width+AMP2*2) * sizeof(int));

	// count of "out of items"
	r_outofsurfaces = false;
	r_outofedges = false;
	r_outofverts = false;
	r_outoftriangles = false;
	r_outoflights = false;
	r_outedgebasespans = false;
	// pointers to allocated buffers
	finalverts = NULL;
	r_edges = NULL;
	lsurfs = NULL;
	triangle_spans = NULL;
	blocklights = NULL;
	edge_basespans = NULL;
	// curently allocated items
	r_cnumsurfs = 0;
	r_numallocatededges = 0;
	r_numallocatedverts = 0;
	r_numallocatedtriangles = 0;
	r_numallocatedlights = 0;
	r_numallocatededgebasespans = 0;

	R_ReallocateMapBuffers();

	r_warpbuffer = Q_malloc(height * width * sizeof(pixel_t));

	// 2k+ resolution and 32 == shift20_t
	if ((width >= 2048) && (sizeof(shift20_t) == 4))
	{
		shift_size = 18;
	}
	else
	{
		shift_size = 20;
	}

	R_InitTurb (width);

	vid_polygon_spans = Q_malloc(sizeof(espan_t) * (height + 1));

	memset(sw_state.currentpalette, 0, sizeof(sw_state.currentpalette));

	R_GammaCorrectAndSetPalette( d_8to24table );
}


/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/

/*
==================
R_ScreenShot_f
==================
*/
static void
R_ScreenShot_f(void)
{
	PE_Finish();
	int x, y;
	byte *buffer = Q_malloc(vid_buffer_width * vid_buffer_height * 3);
	const unsigned char *palette = sw_state.currentpalette;

	if (!buffer)
	{
		R_Printf(PRINT_ALL, "R_ScreenShot: Couldn't malloc %d bytes\n", vid_buffer_width * vid_buffer_height * 3);
		return;
	}

	for (x=0; x < vid_buffer_width; x ++)
	{
		for (y=0; y < vid_buffer_height; y ++) {
			int buffer_pos = y * vid_buffer_width + x;
			buffer[buffer_pos * 3 + 0] = palette[vid_buffer[buffer_pos] * 3 + 0];
			buffer[buffer_pos * 3 + 1] = palette[vid_buffer[buffer_pos] * 3 + 1];
			buffer[buffer_pos * 3 + 2] = palette[vid_buffer[buffer_pos] * 3 + 2];
		}
	}

	ri.Vid_WriteScreenshot(vid_buffer_width, vid_buffer_height, 3, buffer);

	free(buffer);
}
