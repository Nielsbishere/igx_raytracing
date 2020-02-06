
#define ALLOW_SPHERES
#define ALLOW_PLANES
#define ALLOW_TRIANGLES

//#define DISABLE_SHADOWS
//#define DISABLE_REFLECTIONS
//TODO: #define SOFT_SHADOWS
//TODO: #define FULL_PRECISION

#define AVOID_CONFLICTS

#ifdef FULL_PRECISION
	#define outputFormat rgba32f
#else
	#define outputFormat rgba16f
#endif
