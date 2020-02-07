
#define ALLOW_SPHERES
#define ALLOW_PLANES
#define ALLOW_TRIANGLES
#define ALLOW_CUBES

//#define DISABLE_SHADOWS
//#define DISABLE_REFLECTIONS
//TODO: #define SOFT_SHADOWS
//TODO: #define FULL_PRECISION

//TODO: Compile for different thread counts

#define THREADS 64
#define THREADS_X 8
#define THREADS_Y 8

#define AVOID_CONFLICTS

#ifdef FULL_PRECISION
	#define outputFormat rgba32f
#else
	#define outputFormat rgba16f
#endif
