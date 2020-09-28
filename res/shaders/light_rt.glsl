#include "light.glsl"

#ifdef VENDOR_NV

	layout(binding=7, std430) readonly buffer ShadowOutput32 {
	    uint shadowOutput32[];
	};

#else

	layout(binding=7, std430) readonly buffer ShadowOutput64 {
	    uint64_t shadowOutput64[];
	};

#endif

bool didHit(uvec2 loc, uint lightId, uvec2 res) {

	#ifdef VENDOR_NV
	
		//const uvec2 threads = uvec2(16, 2);
		const uvec2 shift = uvec2(4, 1);			//log2(threads)
		const uvec2 mask = uvec2(15, 1);			//threads - 1

		const uvec2 inTile = loc & mask;
		const uint tileShift = inTile.x | (inTile.y << shift.x);

		return (shadowOutput32[indexToLight(loc, res, lightId, shift, mask)] & (1 << tileShift)) != 0;
	
	#else
	
		//const uvec2 threads = uvec2(16, 4);
		const uvec2 shift = uvec2(4, 2);			//log2(threads)
		const uvec2 mask = uvec2(15, 3);			//threads - 1

		const uvec2 inTile = loc & mask;
		const uint tileShift = inTile.x | (inTile.y << shift.x);

		return (shadowOutput64[indexToLight(loc, res, lightId, shift, mask)] & (1 << tileShift)) != 0;

	#endif
}