#pragma once

namespace igx::rt {

	enum class Editors {
		CAMERA,
		CLOUDS
	};

	oicExposedEnum(
		DisplayType, u32, 
		Default, Accumulation,
		Intersection_attributes, Normals, Albedo,
		Light_buffer, Reflection_buffer, No_secondaries,
		UI_Only, Material, Object,
		Shadows_no_reflection,
		Dispatch_groups,
		Reflection_albedo,
		Reflection_normals,
		Reflection_material,
		Reflection_object,
		Reflection_intersection_attributes,
		Reflection_of_reflection,
		Clouds,
		Worley
	);

}