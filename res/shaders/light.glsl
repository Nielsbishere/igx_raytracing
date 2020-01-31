
const uint LightType_Directional = 0;
const uint LightType_Point = 1;
//const uint LightType_Spot = 2;
//const uint LightType_Area = 3;

struct Light {

	vec3 pos;
	float rad;

	vec3 dir;
	uint type;

	vec3 col;
	float angle;

};

