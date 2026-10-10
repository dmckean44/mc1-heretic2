//
// flexmodel.h -- Heretic II FlexModels for the software renderer.
//
// Copyright 1998 Raven Software
//
// The layout of fmdl_t follows Heretic2R's GL renderer, so its skeleton code can be shared. The
// software renderer adds MD2-style triangles and texture coordinates, grouped by mesh node.
//

#pragma once

#include "local.h"
#include "FlexModel.h"
#include "Reference.h"

#define MAX_FM_TRIANGLES	2048
#define MAX_FM_VERTS		2048
#define MAX_FM_FRAMES		2048
#define MAX_FM_MESH_NODES	16

#define SKINPAGE_WIDTH		640
#define SKINPAGE_HEIGHT		480

#define FM_SKIN_NAME		"skin"
#define FM_SKIN_VER			1
#define FM_ST_NAME			"st coord"
#define FM_ST_VER			1
#define FM_TRI_NAME			"tris"
#define FM_TRI_VER			1
#define FM_FRAME_NAME		"frames"
#define FM_FRAME_VER		1
#define FM_SHORT_FRAME_NAME	"short frames"
#define FM_SHORT_FRAME_VER	1
#define FM_NORMAL_NAME		"normals"
#define FM_NORMAL_VER		1
#define FM_COMP_NAME		"comp data"
#define FM_COMP_VER			1
#define FM_GLCMDS_NAME		"glcmds"
#define FM_GLCMDS_VER		1
#define FM_MESH_NAME		"mesh nodes"
#define FM_MESH_VER			3
#define FM_SKELETON_NAME	"skeleton"
#define FM_SKELETON_VER		1
#define FM_REFERENCES_NAME	"references"
#define FM_REFERENCES_VER	1

typedef struct
{
	short s;
	short t;
} fmstvert_t;

typedef struct
{
	short index_xyz[3];
	short index_st[3];
} fmtriangle_t;

typedef struct
{
	byte tris[MAX_FM_TRIANGLES >> 3];
	byte verts[MAX_FM_VERTS >> 3];
	short start_glcmds;
	short num_glcmds;
} fmmeshnode_t;

typedef struct
{
	byte v[3];
	byte lightnormalindex;
} fmtrivertx_t;

typedef struct
{
	float scale[3];
	float translate[3];
	char name[16];
	fmtrivertx_t verts[1]; // Variable sized.
} fmaliasframe_t;

typedef struct
{
	float bmin[3];
	float bmax[3];
} fmgroup_t;

typedef struct
{
	int first;	// Into fmdl_t::node_tris.
	int count;
} fmnode_tris_t;

typedef struct fmdl_s
{
	fmheader_t header;
	fmstvert_t* st_verts;
	fmtriangle_t* tris;
	fmaliasframe_t* frames;
	int* glcmds;
	char* skin_names;
	fmmeshnode_t* mesh_nodes;

	int ngroups;
	fmgroup_t* compdata;
	int* frame_to_group;
	char* framenames;
	byte* lightnormalindex;

	int skeletalType;
	int rootCluster;
	struct ModelSkeleton_s* skeletons;

	int referenceType;
	Placement_t* refsForFrame;

	// Triangles of each mesh node (all triangles in node 0 when the model has no nodes).
	int num_nodes;
	fmnode_tris_t node_tris_range[MAX_FM_MESH_NODES];
	short* node_tris;
} fmdl_t;

extern void Mod_LoadFlexModel(model_t* mod, void* buffer, int length);
extern void Mod_RegisterFlexModel(model_t* mod);

extern void R_LerpVert(const vec3_t new_point, const vec3_t old_point, vec3_t interpolated_point, const float move[3], const float frontv[3], const float backv[3]);
