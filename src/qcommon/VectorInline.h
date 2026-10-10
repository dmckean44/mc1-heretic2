//
// VectorInline.h -- the small vector functions of H2Common as inline functions, for builds that
// link H2Common statically (see Vector.h).
//

#pragma once

static inline float H2_DotProduct(const vec3_t v1, const vec3_t v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

static inline void H2_VectorSubtract(const vec3_t veca, const vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

static inline void H2_VectorAdd(const vec3_t veca, const vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

static inline void H2_VectorCopy(const vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

static inline void H2_VectorClear(vec3_t v)
{
	v[0] = 0.0f;
	v[1] = 0.0f;
	v[2] = 0.0f;
}

static inline void H2_VectorSet(vec3_t v, const float x, const float y, const float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

static inline void H2_VectorScale(const vec3_t in, const float scale, vec3_t out)
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

static inline void H2_VectorMA(const vec3_t veca, const float scale, const vec3_t vecb, vec3_t out)
{
	out[0] = veca[0] + scale * vecb[0];
	out[1] = veca[1] + scale * vecb[1];
	out[2] = veca[2] + scale * vecb[2];
}

static inline void H2_CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

static inline float H2_VectorLengthSquared(const vec3_t v)
{
	return H2_DotProduct(v, v);
}

static inline float H2_VectorLength(const vec3_t v)
{
	return sqrtf(H2_DotProduct(v, v));
}

static inline void H2_VectorInverse(vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

static inline void H2_VectorNegate(const vec3_t in, vec3_t out)
{
	out[0] = -in[0];
	out[1] = -in[1];
	out[2] = -in[2];
}

static inline void H2_VectorScaleByVector(const vec3_t in, const vec3_t scale, vec3_t out)
{
	out[0] = in[0] * scale[0];
	out[1] = in[1] * scale[1];
	out[2] = in[2] * scale[2];
}

static inline float H2_VectorSeparationSquared(const vec3_t va, const vec3_t vb)
{
	vec3_t work;

	H2_VectorSubtract(va, vb, work);
	return H2_DotProduct(work, work);
}

static inline float H2_VectorSeparation(const vec3_t v1, const vec3_t v2)
{
	return sqrtf(H2_VectorSeparationSquared(v1, v2));
}

static inline void H2_Vec3SubtractAssign(const vec3_t value, vec3_t subFrom)
{
	subFrom[0] -= value[0];
	subFrom[1] -= value[1];
	subFrom[2] -= value[2];
}

static inline void H2_Vec3AddAssign(const vec3_t value, vec3_t addTo)
{
	addTo[0] += value[0];
	addTo[1] += value[1];
	addTo[2] += value[2];
}

static inline void H2_Vec3MultAssign(const vec3_t value, vec3_t multBy)
{
	multBy[0] *= value[0];
	multBy[1] *= value[1];
	multBy[2] *= value[2];
}

static inline void H2_Vec3ScaleAssign(const float value, vec3_t scaleBy)
{
	scaleBy[0] *= value;
	scaleBy[1] *= value;
	scaleBy[2] *= value;
}

#define DotProduct(v1, v2)					H2_DotProduct(v1, v2)
#define VectorSubtract(a, b, out)			H2_VectorSubtract(a, b, out)
#define VectorAdd(a, b, out)				H2_VectorAdd(a, b, out)
#define VectorCopy(in, out)					H2_VectorCopy(in, out)
#define VectorClear(v)						H2_VectorClear(v)
#define VectorSet(v, x, y, z)				H2_VectorSet(v, x, y, z)
#define VectorScale(in, scale, out)			H2_VectorScale(in, scale, out)
#define VectorMA(a, scale, b, out)			H2_VectorMA(a, scale, b, out)
#define CrossProduct(v1, v2, cross)			H2_CrossProduct(v1, v2, cross)
#define VectorLengthSquared(v)				H2_VectorLengthSquared(v)
#define VectorLength(v)						H2_VectorLength(v)
#define VectorInverse(v)					H2_VectorInverse(v)
#define VectorNegate(in, out)				H2_VectorNegate(in, out)
#define VectorScaleByVector(in, scale, out)	H2_VectorScaleByVector(in, scale, out)
#define VectorSeparationSquared(va, vb)		H2_VectorSeparationSquared(va, vb)
#define VectorSeparation(v1, v2)			H2_VectorSeparation(v1, v2)
#define Vec3SubtractAssign(value, v)		H2_Vec3SubtractAssign(value, v)
#define Vec3AddAssign(value, v)				H2_Vec3AddAssign(value, v)
#define Vec3MultAssign(value, v)			H2_Vec3MultAssign(value, v)
#define Vec3ScaleAssign(value, v)			H2_Vec3ScaleAssign(value, v)
