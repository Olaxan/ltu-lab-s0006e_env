#include "swrast.h"
#include "GL/glew.h"
#include "line.h"
#include "color.h"

#include <algorithm>
#include <utility>
#include <cmath>

namespace efiilj
{
	rasterizer::rasterizer(const unsigned int height, const unsigned int width,
		std::shared_ptr<camera_model> camera, const unsigned int color)
		: height_(height), width_(width), color_(color), camera_(std::move(camera))
	{
		buffer_ = new unsigned int[width * height];
		depth_ = new unsigned short[width * height];

		clear();
	}

	rasterizer::~rasterizer()
	{
		std::cout << "Deleting rasterizer..." << std::endl;
		
		delete[] buffer_;
		delete[] depth_;
	}

	void rasterizer::convert_screenspace(vertex_data& vertex) const
	{
		vertex.pos /= vertex.pos.w();
		vertex.pos.x((vertex.pos.x() + 1) * (width_ / 2));
		vertex.pos.y((vertex.pos.y() + 1) * (height_ / 2));
	}

	void rasterizer::put_pixel(const int x, const int y, const unsigned int c) const
	{
		if (x > 0 && x < width_ && y > 0 && y < height_)
			buffer_[x + width_ * y] = c;
	}

	void rasterizer::fill_line(const point_data& start, const point_data& end, const vector4& face_normal, const rasterizer_node& node, vertex_data* data) const
	{
		const int y = start.y;

		const fragment_uniforms uniform {};

		for (int x = std::min(start.x, end.x); x < std::max(start.x, end.x); x++)
		{
			vector3 bc = get_barycentric(static_cast<float>(x), static_cast<float>(y), face_normal, data);
			vertex_data fragment = interpolate_fragment(bc, data);
			
			const unsigned c = node.fragment_shader(fragment, node.texture(), uniform);
			
			put_pixel(x, y, c);
		}
	}

	void rasterizer::draw_tri(rasterizer_node& node, const matrix4& local, const unsigned index)
	{

		// Get model face vertices
		vertex* vertices[] = 
		{
			node.get_by_index(index),
			node.get_by_index(index + 1),
			node.get_by_index(index + 2)
		};

		const vector4 face_normal = get_face_normal(vertices[0]->xyzw, vertices[1]->xyzw, vertices[2]->xyzw);
		const vector4 camera_local = local * camera_->transform().position;

		if (cull_backface(vertices[0]->xyzw, face_normal, camera_local))
			return;

		// Create uniforms struct using camera view/perspective and node model transform
		const vertex_uniforms vertex_u(camera_->view_perspective(), node.transform().model());

		// Get vertex data from node vertex shader
		vertex_data data[] = 
		{
			node.vertex_shader(vertices[0], vertex_u),
			node.vertex_shader(vertices[1], vertex_u),
			node.vertex_shader(vertices[2], vertex_u)
		};

		// Convert vertex data to screen-space coordinates (?)
		convert_screenspace(data[0]);
		convert_screenspace(data[1]);
		convert_screenspace(data[2]);

		// Sort vertex data array based on vertex position
		const auto cmp = [this](const vertex_data& a, const vertex_data& b) { return a.pos.x() + static_cast<float>(width_) * a.pos.y() < b.pos.x() + static_cast<float>(width_) * b.pos.y(); };
		std::sort(data, data + 3, cmp);

		// Create line data based on sorted vertex data
		line_data l1(data[0].pos, data[2].pos);
		line_data l2(data[0].pos, data[1].pos);
		line_data l3(data[1].pos, data[2].pos);

		// Draw "upper" face segment (until middle vertex is reached)
		if (l1.dy > 0 && l2.dy > 0)
		{
			while (l1.curr_y < l2.y2)
			{
				point_data pt1 = get_point_on_line(l1);
				point_data pt2 = get_point_on_line(l2);
				fill_line(pt1, pt2, face_normal, node, data);
			}
		}

		// Draw rest of face
		if (l1.dy > 0 && l3.dy > 0)
		{
			while (l1.curr_y < l3.y2)
			{
				point_data pt1 = get_point_on_line(l1);
				point_data pt2 = get_point_on_line(l3);
				fill_line(pt1, pt2, face_normal, node, data);
			}
		}
	}

	vector3 rasterizer::get_barycentric(const float x, const float y, const vector4& face_normal, vertex_data* data)
	{
		const vector2 v0 = vector2(data[1].pos.x(), data[1].pos.y()) - vector2(data[0].pos.x(), data[0].pos.y());
		const vector2 v1 = vector2(data[2].pos.x(), data[2].pos.y()) - vector2(data[0].pos.x(), data[0].pos.y());
		const vector2 v2 = vector2(x, y) - vector2(data[0].pos.x(), data[0].pos.y());
		const float d00 = vector2::dot(v0, v0);
		const float d01 = vector2::dot(v0, v1);
		const float d11 = vector2::dot(v1, v1);
		const float d20 = vector2::dot(v2, v0);
		const float d21 = vector2::dot(v2, v1);
		const float denom = d00 * d11 - d01 * d01;
		const float v = (d11 * d20 - d01 * d21) / denom;
		const float w = (d00 * d21 - d01 * d20) / denom;
		const float u = 1.0f - v - w;

		return vector3(v, w, y);
	}

	vector3 rasterizer::get_barycentric(const vector4& point, const vector4& face_normal, vertex_data* data)
	{
		return get_barycentric(point.x(), point.y(), face_normal, data);
	}

	vector3 rasterizer::get_barycentric(const point_data& point, const vector4& face_normal, vertex_data* data)
	{
		return get_barycentric(static_cast<float>(point.x), static_cast<float>(point.y), face_normal, data);
	}

	vertex_data rasterizer::interpolate_fragment(const vector3& barycentric, vertex_data* data)
	{
		const vector4 position = data[0].pos * barycentric.x() + data[1].pos * barycentric.y() + data[2].pos * barycentric.z();
		const vector4 fragment = data[0].fragment * barycentric.x() + data[1].fragment * barycentric.y() + data[2].fragment * barycentric.z();
		const vector4 normal = data[0].normal * barycentric.x() + data[1].normal * barycentric.y() + data[2].normal * barycentric.z();
		const vector4 color = data[0].color * barycentric.x() + data[1].color * barycentric.y() + data[2].color * barycentric.z();
		const vector2 uv = data[0].uv * barycentric.x() + data[1].uv * barycentric.y();

		return vertex_data { position, fragment, normal, color, uv };
	}

	vector4 rasterizer::get_face_normal(const vector4& a, const vector4& b, const vector4& c)
	{
		const vector4 u = b - a;
		const vector4 v = c - a;
		return vector4::cross(u, v);
	}

	float rasterizer::get_winding_order(const vector4& a, const vector4& b, const vector4& c)
	{
		const matrix2 test (
			b.x() - a.x(),
			c.x() - a.x(),
			b.y() - a.y(),
			c.y() - a.y()
		);

		return test.determinant();
	}

	point_data rasterizer::get_point_on_line(line_data& line)
	{
		point_data point { line.curr_x, line.curr_y };
		
		if (line.horizontal)
		{
			while (true) //line.curr_x != line.x2
			{
				if (line.fraction >= 0)
				{
					line.curr_y += line.step_y;
					line.fraction -= line.dx;
					break;
				}
				line.curr_x += line.step_x;
				line.fraction += line.dy;
				point.x = line.curr_x;
			}
		}
		else
		{
			line.curr_y += line.step_y;
			point.y = line.curr_y;
			
			if (line.fraction >= 0)
			{
				line.curr_x += line.step_x;
				line.fraction -= line.dy;
			}
			
			line.fraction += line.dx;
		}

		return point;
	}

	bool rasterizer::cull_backface(const vector4& p0, const vector4& face_normal, const vector4& camera_local) const
	{
		const vector4 cam_to_tri = p0 - camera_local;
		
		return vector4::dot(cam_to_tri, face_normal) >= 0;
	}

	void rasterizer::bresenham_line(line_data& line, const unsigned c) const
	{
		
		// put initial pixel
		put_pixel(line.curr_x, line.curr_y, c);

		if (line.horizontal)
		{	
			while (line.curr_x != line.x2) 
			{
				line.curr_x += line.step_x;
				if (line.fraction >= 0) 
				{
					line.curr_y += line.step_y;
					line.fraction -= line.dx;
				}
				line.fraction += line.dy;
				put_pixel(line.curr_x, line.curr_y, c);
			}
		}
		else
		{	
			while (line.curr_y != line.y2) 
			{
				if (line.fraction >= 0) 
				{
					line.curr_x += line.step_x;
					line.fraction -= line.dy;
				}
				line.curr_y += line.step_y;
				line.fraction += line.dx;
				put_pixel(line.curr_x, line.curr_y, c);
			}
		}

		line.reset();
	}

	void rasterizer::clear() const
	{
		std::fill(buffer_, buffer_ + width_ * height_, color_);
	}

	void rasterizer::render()
	{
		clear();

		for (const auto& node_ptr : nodes_)
		{
			matrix4 local = node_ptr->transform().model_inv();
			
			for (unsigned int i = 0; i < node_ptr->index_count(); i += 3)
			{
				draw_tri(*node_ptr, local, i);
			}
		}
	}
}
