#pragma once

#include "matrix4.h"
#include "transform.h"

#include <memory>

namespace efiilj
{
	/**
	 * \brief Class to represent a camera in 3D space.
	 */
	class camera_model
	{
	private:

		vector3 up_axis_;
		matrix4 perspective_;
		std::shared_ptr<transform_model> transform_;

	public:

		/**
		 * \brief Creates a new camera instance.
		 * \param fov Camera field of view in radians
		 * \param aspect Camera width/height aspect ration
		 * \param near Near clipping plane
		 * \param far Far clipping plane
		 * \param trans_ptr Pointer to the transform object moving this camera
		 * \param up The vector representing up in this world space
		 */
		camera_model(float fov, float aspect, float near, float far, std::shared_ptr<transform_model>& trans_ptr, const vector3& up);

		camera_model(camera_model& copy)
			= default;

		camera_model(camera_model&& move)
			= default;

		const transform_model& transform() const { return *this->transform_; }
		void transform(std::shared_ptr<transform_model>& trans) { transform_ = std::move(trans); }

		const vector3& up() const { return this->up_axis_; }
		void up(const vector3& xyz) { up_axis_ = xyz; }

		/**
		 * \brief Builds a view/perspective matrix with the current values and returns it.
		 * \return A 4-dimensional matrix for view/perspective projection of a vertex in 3D space
		 */
		matrix4 view_perspective() const;

		~camera_model()
		= default;
	};
}
