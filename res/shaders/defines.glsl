
#define ALLOW_SPHERES
#define ALLOW_PLANES
#define ALLOW_TRIANGLES
#define ALLOW_CUBES

#define GRAPHICS_DEBUG

//#define DISABLE_SHADOWS
//#define DISABLE_REFLECTIONS
//TODO: #define SOFT_SHADOWS
//TODO: #define FULL_PRECISION

//TODO: Compile for different thread counts

#define THREADS 64

//THREADS_XY should be 1 << THREADS_XY_SHIFT, but due to GLSL limitations it is hardcoded

#define THREADS_XY 16
#define THREADS_XY_SHIFT 4
#define THREADS_XY_MASK (THREADS_XY - 1)

#ifdef FULL_PRECISION
	#define outputFormat rgba32f
#else
	#define outputFormat rgba16f
#endif
