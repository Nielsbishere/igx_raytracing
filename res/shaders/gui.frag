#version 450

//This shader allows Nuklear to be subpixel rendered with gamma correction
//Implemented concepts from puredev software's sub-pixel gamma correct font rendering

in layout(location=0) vec2 uv;
in layout(location=1) vec4 col;

out layout(location=0, index=0) vec4 color;
out layout(location=0, index=1) vec4 blend;

layout(binding=0, std140) uniform GUIInfo {
	ivec4 res_pos;
	vec2 whiteTexel;
};

struct Monitor {
	ivec4 begin_orientation_refreshRate;
	ivec4 end_gamma_contrast;				//TODO: Use this gamma properly
	vec4 sampleR_sampleG;
	vec4 sampleB_sizeInMeters;
};

layout(binding=0, std140) buffer Monitors {
	Monitor monitors[];
};

layout(binding=0) uniform sampler2D atlas;

float gammaCorrect(float r){ 
//	return max(1.055 * pow(r, 0.416666667) - 0.055, 0);		TODO:
	return r;
}

float doSample(vec2 subpix, vec2 subpixRes) {
	return gammaCorrect(texture(atlas, subpix / subpixRes).r);
}

bool pointInMonitor(ivec2 pos, Monitor m) {
	return 
		pos.x >= m.begin_orientation_refreshRate.x &&
		pos.y >= m.begin_orientation_refreshRate.y &&
		pos.x < m.end_gamma_contrast.x &&
		pos.y < m.end_gamma_contrast.y;
}


void main() {

	color = col;

	uint monitorId = 0;

	for (uint i = 1; i < monitors.length(); ++i) {
		if(pointInMonitor(ivec2(uv * res_pos.xy) + res_pos.zw, monitors[i])) {
			monitorId = i;
			break;
		}
	}

	Monitor m = monitors[monitorId];

	uint rotation = m.begin_orientation_refreshRate.z;

	vec2 subpixRes = res_pos.xy * (rotation == 90 || rotation == 270 ? vec2(1, 3) : vec2(3, 1));
	vec2 subpixPos = vec2(ivec2(uv * subpixRes));

	blend = mix(

		//Sample subpixel

		vec4(
			doSample(subpixPos + m.sampleR_sampleG.xy, subpixRes) * color.a,
			doSample(subpixPos + m.sampleR_sampleG.zw, subpixRes) * color.a,
			doSample(subpixPos + m.sampleB_sizeInMeters.xy, subpixRes) * color.a,
			color.a
		),

		//Correct for when white needs to be sampled from the atlas

		vec4(
			gammaCorrect(texture(atlas, uv).r).rrr,
			color.a
		),

		float(uv == whiteTexel)
	);
}