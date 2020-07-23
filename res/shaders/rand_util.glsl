
float rand1(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec2 rand(vec2 p) {
	return vec2(rand1(p), rand1(p * 29330 + 1020));
}