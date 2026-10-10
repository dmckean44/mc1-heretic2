//
// sw_fmodel.c -- Heretic II FlexModel loading.
//
// Copyright 1998 Raven Software
//
// Based on Heretic2R's gl1_FlexModel.c. The software renderer draws the "tris" and "st coord"
// blocks (the GL renderer uses the glcmds instead), grouped by mesh node so nodes can be hidden
// or reskinned per entity.
//

#include "header/flexmodel.h"
#include "Skeletons/r_Skeletons.h"
#include "Skeletons.h"

static qboolean fmLoadHeader(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	if (version != FM_HEADER_VER)
		ri.Sys_Error(ERR_DROP, "Invalid HEADER version for block %s: %d != %d\n", FM_HEADER_NAME, FM_HEADER_VER, version);

	memcpy(&fmdl->header, buffer, sizeof(fmheader_t));

	const fmheader_t* h = &fmdl->header;

	if (h->skinwidth < 1 || h->skinwidth > SKINPAGE_WIDTH || h->skinheight < 1 || h->skinheight > SKINPAGE_HEIGHT)
		ri.Sys_Error(ERR_DROP, "Model '%s' has invalid skin size (%ix%i)", model->name, h->skinwidth, h->skinheight);

	if (h->num_xyz < 1 || h->num_xyz >= MAX_FM_VERTS)
		ri.Sys_Error(ERR_DROP, "Model '%s' has invalid number of vertices (%i)", model->name, h->num_xyz);

	if (h->num_st < 1)
		ri.Sys_Error(ERR_DROP, "Model '%s' has no st vertices", model->name);

	if (h->num_tris < 1 || h->num_tris >= MAX_FM_TRIANGLES)
		ri.Sys_Error(ERR_DROP, "Model '%s' has invalid number of triangles (%i)", model->name, h->num_tris);

	if (h->num_frames < 1 || h->num_frames >= MAX_FM_FRAMES)
		ri.Sys_Error(ERR_DROP, "Model '%s' has invalid number of frames (%i)", model->name, h->num_frames);

	if (h->num_mesh_nodes > MAX_FM_MESH_NODES)
		ri.Sys_Error(ERR_DROP, "Model '%s' has too many mesh nodes (%i)", model->name, h->num_mesh_nodes);

	VectorSet(model->mins, -32.0f, -32.0f, -32.0f);
	VectorSet(model->maxs, 32.0f, 32.0f, 32.0f);

	return true;
}

static qboolean fmLoadSkin(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	if (version != FM_SKIN_VER)
		ri.Sys_Error(ERR_DROP, "Invalid SKIN version for block %s: %d != %d\n", FM_SKIN_NAME, FM_SKIN_VER, version);

	const int skin_names_size = fmdl->header.num_skins * MAX_FRAMENAME;
	if (skin_names_size != datasize)
	{
		ri.Con_Printf(PRINT_ALL, "Skin sizes do not match: %d != %d\n", datasize, skin_names_size);
		return false;
	}

	fmdl->skin_names = Hunk_Alloc(skin_names_size);
	memcpy(fmdl->skin_names, buffer, skin_names_size);

	return true;
}

static qboolean fmLoadST(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	if (version != FM_ST_VER)
		ri.Sys_Error(ERR_DROP, "Invalid ST version for block %s: %d != %d\n", FM_ST_NAME, FM_ST_VER, version);

	const int size = fmdl->header.num_st * (int)sizeof(fmstvert_t);
	if (datasize < size)
		return false;

	fmdl->st_verts = Hunk_Alloc(size);
	memcpy(fmdl->st_verts, buffer, size);

	return true;
}

static qboolean fmLoadTris(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	if (version != FM_TRI_VER)
		ri.Sys_Error(ERR_DROP, "Invalid TRIS version for block %s: %d != %d\n", FM_TRI_NAME, FM_TRI_VER, version);

	const int size = fmdl->header.num_tris * (int)sizeof(fmtriangle_t);
	if (datasize < size)
		return false;

	fmdl->tris = Hunk_Alloc(size);
	memcpy(fmdl->tris, buffer, size);

	return true;
}

static qboolean fmLoadFrames(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	if (version != FM_FRAME_VER)
		ri.Sys_Error(ERR_DROP, "Invalid FRAMES version for block %s: %d != %d\n", FM_FRAME_NAME, FM_FRAME_VER, version);

	const int size = fmdl->header.num_frames * fmdl->header.framesize;
	if (datasize < size)
		return false;

	fmdl->frames = Hunk_Alloc(size);
	memcpy(fmdl->frames, buffer, size);

	return true;
}

static qboolean fmLoadGLCmds(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	return true;
}

static qboolean fmLoadMeshNodes(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	if (version != FM_MESH_VER)
		ri.Sys_Error(ERR_DROP, "Invalid MESH version for block %s: %d != %d\n", FM_MESH_NAME, FM_MESH_VER, version);

	const int size = fmdl->header.num_mesh_nodes * (int)sizeof(fmmeshnode_t);
	if (fmdl->header.num_mesh_nodes < 1)
		return true;

	if (datasize < size)
		return false;

	fmdl->mesh_nodes = Hunk_Alloc(size);
	memcpy(fmdl->mesh_nodes, buffer, size);

	return true;
}

static qboolean fmSkipBlock(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	return false;
}

static qboolean fmLoadSkeleton(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	if (version != FM_SKELETON_VER)
	{
		ri.Con_Printf(PRINT_ALL, "Invalid SKELETON version for block %s: %d != %d\n", FM_SKELETON_NAME, FM_SKELETON_VER, version);
		return false;
	}

	const int* in_i = buffer;

	fmdl->skeletalType = *in_i;
	fmdl->rootCluster = CreateSkeleton(fmdl->skeletalType);

	const int num_clusters = *(++in_i);

	int num_verts = 0;
	for (int cluster = num_clusters - 1; cluster > -1; cluster--)
	{
		num_verts += *(++in_i);

		const int cluster_index = fmdl->rootCluster + cluster;
		SkeletalClusters[cluster_index].numVerticies = num_verts;
		SkeletalClusters[cluster_index].verticies = Hunk_Alloc(num_verts * (int)sizeof(int));
	}

	int start_vert_index = 0;
	for (int cluster = num_clusters - 1; cluster > -1; cluster--)
	{
		for (int v = start_vert_index; v < SkeletalClusters[fmdl->rootCluster + cluster].numVerticies; v++)
		{
			const int vert_index = *(++in_i);
			for (int c = 0; c <= cluster; c++)
				SkeletalClusters[fmdl->rootCluster + c].verticies[v] = vert_index;
		}

		start_vert_index = SkeletalClusters[fmdl->rootCluster + cluster].numVerticies;
	}

	const qboolean have_skeleton = *(++in_i);

	if (have_skeleton)
	{
		const float* in_f = (const float*)in_i;

		fmdl->skeletons = Hunk_Alloc(fmdl->header.num_frames * (int)sizeof(ModelSkeleton_t));

		for (int i = 0; i < fmdl->header.num_frames; i++)
		{
			CreateSkeletonAsHunk(fmdl->skeletalType, fmdl->skeletons + i);

			for (int c = 0; c < num_clusters; c++)
			{
				M_SkeletalJoint_t* joint = &fmdl->skeletons[i].rootJoint[c];

				joint->model.origin[0] = *(++in_f);
				joint->model.origin[1] = *(++in_f);
				joint->model.origin[2] = *(++in_f);

				joint->model.direction[0] = *(++in_f);
				joint->model.direction[1] = *(++in_f);
				joint->model.direction[2] = *(++in_f);

				joint->model.up[0] = *(++in_f);
				joint->model.up[1] = *(++in_f);
				joint->model.up[2] = *(++in_f);

				VectorCopy(joint->model.origin, joint->parent.origin);
				VectorCopy(joint->model.direction, joint->parent.direction);
				VectorCopy(joint->model.up, joint->parent.up);
			}
		}
	}
	else
	{
		fmdl->header.num_xyz -= num_clusters * 3;
	}

	return true;
}

static qboolean fmLoadReferences(fmdl_t* fmdl, model_t* model, const int version, const int datasize, const void* buffer)
{
	typedef struct
	{
		int referenceType;
		qboolean haveRefs;
		Placement_t* refsForFrame;
	} dmreferences_t;

	if (version != FM_REFERENCES_VER)
	{
		ri.Con_Printf(PRINT_ALL, "Invalid REFERENCES version for block %s: %d != %d\n", FM_REFERENCES_NAME, FM_REFERENCES_VER, version);
		return false;
	}

	const dmreferences_t* in = buffer;
	fmdl->referenceType = in->referenceType;

	if (!in->haveRefs)
	{
		fmdl->header.num_xyz -= numReferences[fmdl->referenceType] * 3;
		return true;
	}

	const int size = fmdl->header.num_frames * numReferences[fmdl->referenceType] * (int)sizeof(Placement_t);
	fmdl->refsForFrame = Hunk_Alloc(size);
	memcpy(fmdl->refsForFrame, &in->refsForFrame, size);

	return true;
}

typedef struct
{
	char ident[FMDL_BLOCK_IDENT_SIZE];
	qboolean (*load)(fmdl_t* fmdl, model_t* model, int version, int datasize, const void* buffer);
} fmdl_loader_t;

static const fmdl_loader_t fmblocks[] =
{
	{ FM_HEADER_NAME,		fmLoadHeader },
	{ FM_SKIN_NAME,			fmLoadSkin },
	{ FM_ST_NAME,			fmLoadST },
	{ FM_TRI_NAME,			fmLoadTris },
	{ FM_FRAME_NAME,		fmLoadFrames },
	{ FM_GLCMDS_NAME,		fmLoadGLCmds },
	{ FM_MESH_NAME,			fmLoadMeshNodes },
	{ FM_SHORT_FRAME_NAME,	fmSkipBlock },
	{ FM_NORMAL_NAME,		fmSkipBlock },
	{ FM_COMP_NAME,			fmSkipBlock },
	{ FM_SKELETON_NAME,		fmLoadSkeleton },
	{ FM_REFERENCES_NAME,	fmLoadReferences },
	{ "",					NULL },
};

// Sorts the triangles by mesh node. A triangle that belongs to several nodes is drawn with the first.
static void fmGroupTrianglesByNode(fmdl_t* fmdl)
{
	const int num_tris = fmdl->header.num_tris;

	fmdl->node_tris = Hunk_Alloc(num_tris * (int)sizeof(short));

	if (fmdl->mesh_nodes == NULL)
	{
		fmdl->num_nodes = 1;
		fmdl->node_tris_range[0].first = 0;
		fmdl->node_tris_range[0].count = num_tris;

		for (int i = 0; i < num_tris; i++)
			fmdl->node_tris[i] = (short)i;

		return;
	}

	byte assigned[MAX_FM_TRIANGLES] = { 0 };
	int count = 0;

	fmdl->num_nodes = fmdl->header.num_mesh_nodes;

	for (int n = 0; n < fmdl->num_nodes; n++)
	{
		const byte* mask = fmdl->mesh_nodes[n].tris;

		fmdl->node_tris_range[n].first = count;

		for (int i = 0; i < num_tris; i++)
		{
			if ((mask[i >> 3] & (1 << (i & 7))) && !assigned[i])
			{
				assigned[i] = true;
				fmdl->node_tris[count++] = (short)i;
			}
		}

		fmdl->node_tris_range[n].count = count - fmdl->node_tris_range[n].first;
	}
}

// The skin and texture coordinates are in header skin units; scale the coordinates when the skin image differs.
static void fmScaleTexCoords(fmdl_t* fmdl, const model_t* mod)
{
	const image_t* skin = mod->skins[0];

	if (skin == NULL || skin == r_notexture_mip || (skin->width == fmdl->header.skinwidth && skin->height == fmdl->header.skinheight))
		return;

	for (int i = 0; i < fmdl->header.num_st; i++)
	{
		fmdl->st_verts[i].s = (short)(fmdl->st_verts[i].s * skin->width / fmdl->header.skinwidth);
		fmdl->st_verts[i].t = (short)(fmdl->st_verts[i].t * skin->height / fmdl->header.skinheight);
	}

	fmdl->header.skinwidth = skin->width;
	fmdl->header.skinheight = skin->height;
}

void Mod_LoadFlexModel(model_t* mod, void* buffer, int length)
{
	mod->type = mod_alias;
	// Skeletons expand to one joint set per frame, so leave plenty of room; Hunk_End() trims it.
	mod->extradata = Hunk_Begin(length * 2 + 0x100000);

	fmdl_t* fmdl = Hunk_Alloc(sizeof(fmdl_t));
	fmdl->skeletalType = SKEL_NULL;
	fmdl->referenceType = REF_NULL;

	byte* in = buffer;

	while (length > 0)
	{
		const fmdl_blockheader_t* header = (fmdl_blockheader_t*)in;
		in += sizeof(fmdl_blockheader_t);

		const fmdl_loader_t* loader;
		for (loader = &fmblocks[0]; loader->ident[0] != 0; loader++)
		{
			if (Q_stricmp(loader->ident, header->ident) == 0)
			{
				if (!loader->load(fmdl, mod, header->version, header->size, in))
					ri.Com_Error(ERR_DROP, "Mod_LoadFlexModel: failed to load block %s in %s\n", header->ident, mod->name);

				break;
			}
		}

		if (loader->ident[0] == 0)
			ri.Con_Printf(PRINT_ALL, "Mod_LoadFlexModel: unknown block %s\n", header->ident);

		in += header->size;
		length -= header->size + (int)sizeof(fmdl_blockheader_t);
	}

	if (fmdl->frames == NULL || fmdl->tris == NULL || fmdl->st_verts == NULL)
		ri.Com_Error(ERR_DROP, "Mod_LoadFlexModel: %s has no frames, triangles or texture coordinates\n", mod->name);

	fmGroupTrianglesByNode(fmdl);

	mod->numframes = fmdl->header.num_frames;
	Mod_RegisterFlexModel(mod);
	fmScaleTexCoords(fmdl, mod);
}

void Mod_RegisterFlexModel(model_t* mod)
{
	const fmdl_t* fmdl = mod->extradata;
	const char* skin_name = fmdl->skin_names;

	for (int i = 0; i < fmdl->header.num_skins && i < MAX_FRAMES; i++, skin_name += MAX_FRAMENAME)
		mod->skins[i] = R_FindImage(skin_name, it_skin);
}

void Mod_FreeFlexModel(model_t* mod)
{
	const fmdl_t* fmdl = mod->extradata;

	if (fmdl == NULL || fmdl->skeletalType == SKEL_NULL)
		return;

	for (int i = 0; i < numJointsInSkeleton[fmdl->skeletalType]; i++)
		SkeletalClusters[fmdl->rootCluster + i].inUse = false;
}

void R_LerpVert(const vec3_t new_point, const vec3_t old_point, vec3_t interpolated_point, const float move[3], const float frontv[3], const float backv[3])
{
	for (int i = 0; i < 3; i++)
		interpolated_point[i] = new_point[i] * frontv[i] + old_point[i] * backv[i] + move[i];
}
