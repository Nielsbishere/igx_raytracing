#include "rt/structs.hpp"

namespace igx::rt {

	Mat3x3f32 CPUCamera::getRot() const {

		const f32
			a = roll,
			b = yaw,
			g = pitch,
			ca = std::cos(a),
			cb = std::cos(b),
			cg = std::cos(g),
			sa = std::sin(a),
			sb = std::sin(b),
			sg = std::sin(g),
			sbsg = sb * sg,
			sbcg = sb * cg;

		return Mat3x3f32{
			Vec3f32{ ca * cb, ca * sbsg - sa * cg, ca * sbcg + sa * sg },
			Vec3f32{ sa * cb, sa * sbsg + ca * cg, sa * sbcg - ca * sg },
			Vec3f32{ -sb, cb * sg, cb * cg }
		};
	}

	Mat4x4f32 CPUCamera::getView(f32 eyeOffset) const {

		Mat3x3f32 rot = getRot();

		Mat4x4f32 res;

		res.x = rot.xAxis;
		res.y = rot.yAxis;
		res.z = rot.zAxis;

		//Convert to mm and divide by 2 since it's from left to right eye

		res.pos = eye + rot.xAxis * (ipd * 5e-4f * eyeOffset);

		return res;
	}

}