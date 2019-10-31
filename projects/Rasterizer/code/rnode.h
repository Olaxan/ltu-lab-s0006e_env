#pragma once

#include "vertex.h"
#include "transform.h"

#include <vector>
#include <functional>


namespace efiilj
{
	class rasterizer_node
	{
	private:
		std::vector<vertex> vertices_;
		std::vector<unsigned> indices_;
		std::function<void(vertex*)> vertex_shader_;
		std::function<unsigned(vector2, vector4, unsigned*)> fragment_shader_; //UV, Normal, Texture
		std::shared_ptr<transform_model> transform_;

	public:
		rasterizer_node(std::vector<vertex> vertices, std::vector<unsigned> indices, std::shared_ptr<transform_model> transform);

		void set_vertex_shader(std::function<void(vertex*)> shader) { vertex_shader_ = std::move(shader); }
		void set_fragment_shader(std::function<unsigned(vector2, vector4, unsigned*)> shader) { fragment_shader_ = std::move(shader); }

		unsigned int vertex_count() const { return vertices_.size(); }
		unsigned int index_count() const { return indices_.size(); }

		vertex& get_by_index(const unsigned index) { return vertices_[indices_[index]]; }

		transform_model& transform() const { return *this->transform_; }
		void transform(std::shared_ptr<transform_model>& transform) { this->transform_ = std::move(transform); }
	};
}
