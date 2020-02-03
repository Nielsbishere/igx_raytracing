
#define ALLOW_SPHERES
#define ALLOW_PLANES
#define ALLOW_TRIANGLES

//TODO: #define SOFT_SHADOWS
//TODO: #define FULL_PRECISION

#ifdef FULL_PRECISION
	#define outputFormat rgba32f
#else
	#define outputFormat rgba16f
#endif