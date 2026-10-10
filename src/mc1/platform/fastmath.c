// The MRISC32 FPU has no square root instruction and newlib's sqrtf is a slow bit-by-bit loop.
// The reciprocal square root estimate refined twice by Newton-Raphson is good to about 2 ulp.

#ifdef __MRISC32__

float sqrtf(float x)
{
	if (!(x > 0.0f))
		return 0.0f;

	float y;
	const float half = 0.5f * x;

	__asm__("frsqrta\t%0, %1" : "=r"(y) : "r"(x));
	y = y * (1.5f - half * y * y);
	y = y * (1.5f - half * y * y);

	return x * y;
}

#endif
