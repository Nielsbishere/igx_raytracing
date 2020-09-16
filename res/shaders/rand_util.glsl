#ifndef RAND_UTIL
#define RAND_UTIL

//Defining a seed on the GPU sent from the CPU

struct Seed {
	float randomX, randomY;
	float cpuOffsetX, cpuOffsetY;
	uint sampleCount, sampleOffset;
};

const float goldenRatio = 0.61803398875f;

//Beter random funcs from wisp (https://github.com/TeamWisp/WispRenderer)

uint initRand(uint v0, uint v1) {

	uint s0 = 0;

	for (uint n = 0; n < 16; n++) {
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

//Get 'random' value [0, 1]
float nextRand(inout uint s) {
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

//Uniform uint (yoinked from http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html)
//But modified to work for ints

uint uVdC(uint seed) {
     seed = (seed << 16u) | (seed >> 16u);
     seed = ((seed & 0x55555555u) << 1u) | ((seed & 0xAAAAAAAAu) >> 1);
     seed = ((seed & 0x33333333u) << 2u) | ((seed & 0xCCCCCCCCu) >> 2);
     seed = ((seed & 0x0F0F0F0Fu) << 4u) | ((seed & 0xF0F0F0F0u) >> 4);
     seed = ((seed & 0x00FF00FFu) << 8u) | ((seed & 0xFF00FF00u) >> 8);
     return seed;
 }

 float fVdC(uint seed) {
	return float(uVdC(seed)) * 2.3283064365386963e-10;	//div by 2^32, gives us a range [0, 1>
 }

//Generic random funcs

float rand1(vec2 co){
    return fract(sin(dot(co, vec2(12.9898,78.233))) * 43758.5453);
}

float rand1(vec3 co){
    return fract(sin(dot(co, vec3(12.9898,78.233,471.28))) * 43758.5453);
}

float rand1_3(vec3 co){
    return rand1(co.xy * vec2(co.z, 1));
}

vec2 rand(vec2 p) {
	return vec2(rand1(p), rand1(p * 1103515245 + 12345));
}

vec3 rand(vec3 p) {
	return vec3(
        rand1(p), 
        rand1(p + 69),
        rand1(p + 420)
    );
}

//	Simplex 4D Noise 
//	by Ian McEwan, Ashima Arts
//
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
float permute(float x){return floor(mod(((x*34.0)+1.0)*x, 289.0));}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
float taylorInvSqrt(float r){return 1.79284291400159 - 0.85373472095314 * r;}

vec4 grad4(float j, vec4 ip){
  const vec4 ones = vec4(1.0, 1.0, 1.0, -1.0);
  vec4 p,s;

  p.xyz = floor( fract (vec3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
  p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
  s = vec4(lessThan(p, vec4(0.0)));
  p.xyz = p.xyz + (s.xyz*2.0 - 1.0) * s.www; 

  return p;
}

float snoise(vec4 v){
  const vec2  C = vec2( 0.138196601125010504,  // (5 - sqrt(5))/20  G4
                        0.309016994374947451); // (sqrt(5) - 1)/4   F4
// First corner
  vec4 i  = floor(v + dot(v, C.yyyy) );
  vec4 x0 = v -   i + dot(i, C.xxxx);

// Other corners

// Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
  vec4 i0;

  vec3 isX = step( x0.yzw, x0.xxx );
  vec3 isYZ = step( x0.zww, x0.yyz );
//  i0.x = dot( isX, vec3( 1.0 ) );
  i0.x = isX.x + isX.y + isX.z;
  i0.yzw = 1.0 - isX;

//  i0.y += dot( isYZ.xy, vec2( 1.0 ) );
  i0.y += isYZ.x + isYZ.y;
  i0.zw += 1.0 - isYZ.xy;

  i0.z += isYZ.z;
  i0.w += 1.0 - isYZ.z;

  // i0 now contains the unique values 0,1,2,3 in each channel
  vec4 i3 = clamp( i0, 0.0, 1.0 );
  vec4 i2 = clamp( i0-1.0, 0.0, 1.0 );
  vec4 i1 = clamp( i0-2.0, 0.0, 1.0 );

  //  x0 = x0 - 0.0 + 0.0 * C 
  vec4 x1 = x0 - i1 + 1.0 * C.xxxx;
  vec4 x2 = x0 - i2 + 2.0 * C.xxxx;
  vec4 x3 = x0 - i3 + 3.0 * C.xxxx;
  vec4 x4 = x0 - 1.0 + 4.0 * C.xxxx;

// Permutations
  i = mod(i, 289.0); 
  float j0 = permute( permute( permute( permute(i.w) + i.z) + i.y) + i.x);
  vec4 j1 = permute( permute( permute( permute (
             i.w + vec4(i1.w, i2.w, i3.w, 1.0 ))
           + i.z + vec4(i1.z, i2.z, i3.z, 1.0 ))
           + i.y + vec4(i1.y, i2.y, i3.y, 1.0 ))
           + i.x + vec4(i1.x, i2.x, i3.x, 1.0 ));
// Gradients
// ( 7*7*6 points uniformly over a cube, mapped onto a 4-octahedron.)
// 7*7*6 = 294, which is close to the ring size 17*17 = 289.

  vec4 ip = vec4(1.0/294.0, 1.0/49.0, 1.0/7.0, 0.0) ;

  vec4 p0 = grad4(j0,   ip);
  vec4 p1 = grad4(j1.x, ip);
  vec4 p2 = grad4(j1.y, ip);
  vec4 p3 = grad4(j1.z, ip);
  vec4 p4 = grad4(j1.w, ip);

// Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  p4 *= taylorInvSqrt(dot(p4,p4));

// Mix contributions from the five corners
  vec3 m0 = max(0.6 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
  vec2 m1 = max(0.6 - vec2(dot(x3,x3), dot(x4,x4)            ), 0.0);
  m0 = m0 * m0;
  m1 = m1 * m1;
  return 49.0 * ( dot(m0*m0, vec3( dot( p0, x0 ), dot( p1, x1 ), dot( p2, x2 )))
               + dot(m1*m1, vec2( dot( p3, x3 ), dot( p4, x4 ) ) ) ) ;

}

//Worley noise

float worley(uvec3 res, uvec3 pointsRes, vec3 offset, vec3 floc) {

	//Calculate constants

	const vec3 invRes = 1 / vec3(res);
	const vec3 invPointRes = 1 / vec3(pointsRes);

	const vec3 pointPerPixel = pointsRes * invRes;

	const vec3 currentPoint = floor(floc * pointPerPixel);
	const vec3 pixelPerPoint = res * invPointRes;

	float squareDist = 1;

	//Get the current point and the points in the neighboring cells
	//Get the minimum distance between them

	for(int k = -1; k <= 1; ++k)
		for(int j = -1; j <= 1; ++j)
			for(int i = -1; i <= 1; ++i) {

				//Grab current point in neighbor cell

				vec3 local = vec3(i, j, k);
				vec3 global = currentPoint + local;

				//Ensure out of bounds access is threated as wrap
				//But we don't have a texture, so manual 

				global += vec3(lessThan(global, vec3(0))) * pointsRes;
				global -= vec3(greaterThanEqual(global, pointsRes)) * pointsRes;

				//Grab random offset in cell

				vec3 sampledOffset = rand(offset + global);

				global += sampledOffset;

				//Calculate the square distance and check if it's less

				vec3 localPoint = (global * pixelPerPoint - floc) * pointPerPixel;
				float localSquareDist = dot(localPoint, localPoint);

				squareDist = min(squareDist, localSquareDist);
			}

	return sqrt(squareDist);
}


#endif