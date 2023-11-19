#include <bit>
#include <new>
#include <map>
#include <set>
#include <tuple>
#include <mutex>
#include <deque>
#include <cmath>
#include <queue>
#include <string>
#include <thread>
#include <memory>
#include <random>
#include <vector>
#include <atomic>
#include <cassert>
#include <iomanip>
#include <utility>
#include <numeric>
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <variant>
#include <optional>
#include <sstream>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <typeindex>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>

#include <glfw.hpp>
#include <dt_timer.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <implot/implot.h>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>

using namespace std::literals::string_literals;

namespace {
	struct glew_guard_t {
		glew_guard_t() {
			if (glewInit() != GLEW_OK) {
				throw std::runtime_error("Failed to initialize glew.");
			}
		}
	};

	void imgui_texture(ImTextureID id, const ImVec2& size) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems) {
			return;
		}

		ImVec2 content_size = ImGui::GetContentRegionAvail();
		float ratio = std::min(content_size.x / size.x, content_size.y / size.y);
		float width = size.x * ratio;
		float height = size.y * ratio;

		window->DC.CursorPos.y += height;
		ImGui::Image(id, ImVec2{width, -height});
	}

	template<class T>
	ImTextureID to_tex_id(T id) {
		return (ImTextureID)(std::uintptr_t)id;
	}

	struct texture_t {
		texture_t() = default;
		~texture_t() {
			if (valid()) {
				glDeleteTextures(1, &id);
			}
		}

		texture_t(texture_t&& another) noexcept {
			*this = std::move(another);
		}

		texture_t& operator = (texture_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
				std::swap(width, another.width);
				std::swap(height, another.height);
			} return *this;
		}

		void bind_unit(GLuint unit) const {
			glBindTextureUnit(unit, id);
		}

		bool valid() const {
			return id != 0;
		}

		GLuint id{};
		int width{};
		int height{};
	};

	texture_t gen_test_texture(int width, int height) {
		assert(width > 0);
		assert(height > 0);

		constexpr int channels = 2;

		std::vector<float> tex_data(width * height * channels);
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				int base = i * width * channels + j * channels;
				tex_data[base    ] = (float)j / width;
				tex_data[base + 1] = (float)i / height;
			}
		}

		texture_t tex{};
		tex.width = width;
		tex.height = height;
		glCreateTextures(GL_TEXTURE_2D, 1, &tex.id);
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureStorage2D(tex.id, 1, GL_RG32F, width, height);
		glTextureSubImage2D(tex.id, 0, 0, 0, width, height, GL_RG, GL_FLOAT, tex_data.data());
		return tex;
	}

	enum texture_internal_format_t : GLenum {
		Rgba32f = GL_RGBA32F,
		Rgba16f = GL_RGBA16F,
		Rgba8 = GL_RGBA8,
	};

	texture_t gen_empty_texture(int width, int height, texture_internal_format_t format) {
		assert(width > 0);
		assert(height > 0);

		texture_t tex{};
		tex.width = width;
		tex.height = height;
		glCreateTextures(GL_TEXTURE_2D, 1, &tex.id);
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureStorage2D(tex.id, 1, format, width, height);
		return tex;
	}

	enum depth_texture_type_t : GLenum {
		Depth16 = GL_DEPTH_COMPONENT16,
		Depth24 = GL_DEPTH_COMPONENT24,
		Depth32 = GL_DEPTH_COMPONENT32,
		Depth32f = GL_DEPTH_COMPONENT32F,
	};

	texture_t gen_depth_texture(int width, int height, depth_texture_type_t type) {
		assert(width > 0);
		assert(height > 0);

		texture_t tex{};
		tex.width = width;
		tex.height = height;
		glCreateTextures(GL_TEXTURE_2D, 1, &tex.id);
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(tex.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureStorage2D(tex.id, 1, type, width, height);
		return tex;
	}

	enum vertex_attrib_index_t : GLenum {
		Vertex0 = 0,
		Vertex = Vertex0,
		Normal0 = 1,
		Normal = Normal0,
	};

	struct vertex_buffer_t {
		GLintptr offset{};
		GLintptr stride{};
	};

	struct vertex_attrib_t {
		vertex_attrib_index_t attrib_index{};
		GLint size{};
		GLenum type{};
		GLboolean normalized{};
		GLuint relative_offset{};
	};

	// coordinate system is right-handed
	// OxOz - ground plane, Oy is up-axis
	// coords must be in [-1;1] range
	struct mesh_t {
		mesh_t() = default;
		mesh_t(mesh_t&& another) noexcept {
			*this = std::move(another);
		}

		~mesh_t() {
			if (valid()) {
				glDeleteBuffers(1, &id);
			}
		}

		mesh_t& operator = (mesh_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
				std::swap(vertex_buffer, another.vertex_buffer);
				std::swap(vertex_attrib, another.vertex_attrib);
				std::swap(mode, another.mode);
				std::swap(faces, another.faces);
				std::swap(vertices, another.vertices);
			} return *this;
		}

		bool valid() const {
			return id != 0;
		}

		GLuint id{}; // for now only one buffer per mesh 
		vertex_buffer_t vertex_buffer; // for now only one vertex buffer per buffer
		vertex_attrib_t vertex_attrib; // for now only one vertex attrib per vertex buffer
		GLenum mode{};
		int faces{};
		int vertices{};
	};

	// sphere: radius(r), inclination(theta, from Oy), azimuth(phi, from Ox)
	// (1, pi/2, 0) -> (1, 0, 0)
	// (1, pi/2, pi/2) -> (0, 0, 1)
	glm::vec3 sphere_to_cartesian(const glm::vec3& coords) {
		float r = coords.x;
		float cos_the = std::cos(coords.y), sin_the = std::sin(coords.y);
		float cos_phi = std::cos(coords.z), sin_phi = std::sin(coords.z);
		return glm::vec3(r * sin_the * cos_phi, r * cos_the, r * sin_the * sin_phi);
	}

	mesh_t gen_sphere_mesh(int subdiv = 0) {
		assert(subdiv >= 0);

		const float pi = 3.14159265359;
		const float angle = 0.463647609; //std::atan(0.5);
		const float north_the = pi / 2.0 - angle;
		const float south_the = pi / 2.0 + angle;

		constexpr int ico_vertices = 12;
		constexpr int total_faces = 20;
		constexpr int total_vertices = 3 * total_faces;

		glm::vec3 icosahedron_verts[ico_vertices] = {
			sphere_to_cartesian(glm::vec3(1.0, 0.0, 0.0)), // north pole
			sphere_to_cartesian(glm::vec3(1.0, north_the, 0.0 * 2.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, north_the, 1.0 * 2.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, north_the, 2.0 * 2.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, north_the, 3.0 * 2.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, north_the, 4.0 * 2.0 * pi / 5.0)),

			sphere_to_cartesian(glm::vec3(1.0, south_the, 0.0 * 2.0 * pi / 5.0 + 1.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 1.0 * 2.0 * pi / 5.0 + 1.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 2.0 * 2.0 * pi / 5.0 + 1.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 3.0 * 2.0 * pi / 5.0 + 1.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 4.0 * 2.0 * pi / 5.0 + 1.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, pi, 0.0)), // south pole
		};

		struct face_t {
			glm::vec3 v0, v1, v2;
		};

		face_t icosahedron_faces[total_vertices] = {
			{icosahedron_verts[0], icosahedron_verts[2], icosahedron_verts[1]},
			{icosahedron_verts[0], icosahedron_verts[3], icosahedron_verts[2]},
			{icosahedron_verts[0], icosahedron_verts[4], icosahedron_verts[3]},
			{icosahedron_verts[0], icosahedron_verts[5], icosahedron_verts[4]},
			{icosahedron_verts[0], icosahedron_verts[1], icosahedron_verts[5]},

			{icosahedron_verts[1], icosahedron_verts[2], icosahedron_verts[6]},
			{icosahedron_verts[6], icosahedron_verts[2], icosahedron_verts[7]},
			{icosahedron_verts[7], icosahedron_verts[2], icosahedron_verts[3]},
			{icosahedron_verts[3], icosahedron_verts[8], icosahedron_verts[7]},
			{icosahedron_verts[8], icosahedron_verts[3], icosahedron_verts[4]},
			{icosahedron_verts[4], icosahedron_verts[9], icosahedron_verts[8]},
			{icosahedron_verts[4], icosahedron_verts[5], icosahedron_verts[9]},
			{icosahedron_verts[5], icosahedron_verts[10], icosahedron_verts[9]},
			{icosahedron_verts[10], icosahedron_verts[5], icosahedron_verts[1]},
			{icosahedron_verts[1], icosahedron_verts[6], icosahedron_verts[10]},

			{icosahedron_verts[11], icosahedron_verts[6], icosahedron_verts[7]},
			{icosahedron_verts[11], icosahedron_verts[7], icosahedron_verts[8]},
			{icosahedron_verts[11], icosahedron_verts[8], icosahedron_verts[9]},
			{icosahedron_verts[11], icosahedron_verts[9], icosahedron_verts[10]},
			{icosahedron_verts[11], icosahedron_verts[10], icosahedron_verts[6]},
		};

		std::vector<face_t> subdivided;
		subdivided.reserve(total_vertices << (subdiv << 1)); // a << (b << 1) = a * 2 ^ (b * 2) = a * 4 ^ b
		for (auto& face : icosahedron_faces) {
			subdivided.push_back(face);
		}
		
		for (int i = 0; i < subdiv; i++) {
			int current_subdiv = subdivided.size();
			for (int j = 0; j < current_subdiv; j++) {
				auto [v0, v1, v2] = subdivided[j];
				glm::vec3 v01 = glm::normalize((v0 + v1) / 2.0f);
				glm::vec3 v12 = glm::normalize((v1 + v2) / 2.0f);
				glm::vec3 v20 = glm::normalize((v2 + v0) / 2.0f);
				subdivided[j] = {v01, v12, v20};
				subdivided.push_back({v0, v01, v20});
				subdivided.push_back({v1, v12, v01});
				subdivided.push_back({v2, v20, v12});
			}
		}

		mesh_t mesh{};
		glCreateBuffers(1, &mesh.id);
		glNamedBufferStorage(mesh.id, sizeof(face_t) * subdivided.size(), subdivided.data(), 0);
		mesh.vertex_buffer.offset = 0;
		mesh.vertex_buffer.stride = 3 * sizeof(float);
		mesh.vertex_attrib.attrib_index = Vertex0;
		mesh.vertex_attrib.normalized = GL_FALSE;
		mesh.vertex_attrib.relative_offset = 0;
		mesh.vertex_attrib.size = 3;
		mesh.vertex_attrib.type = GL_FLOAT;
		mesh.faces = subdivided.size();
		mesh.vertices = subdivided.size() * 3;
		mesh.mode = GL_TRIANGLES;
		return mesh;
	}

	#define ARRAY_SIZE(arr) (sizeof(arr) / (sizeof(arr[0])))

	mesh_t gen_pyramid_mesh() {
		struct face_t {
			glm::vec3 v0, v1, v2;
		};

		glm::vec3 verts[4] = {
			glm::vec3{0.0f, 0.0f, 0.0f},
			glm::vec3{1.0f, 0.0f, 0.0f},
			glm::vec3{0.0f, 1.0f, 0.0f},
			glm::vec3{0.0f, 0.0f, 1.0f},
		};
		face_t faces[] = {
			{verts[0], verts[1], verts[2]},
			{verts[0], verts[2], verts[3]},
			{verts[0], verts[3], verts[1]},
			{verts[1], verts[3], verts[2]},
		};

		mesh_t mesh{};
		glCreateBuffers(1, &mesh.id);
		glNamedBufferStorage(mesh.id, sizeof(faces), faces, 0);
		mesh.vertex_buffer.offset = 0;
		mesh.vertex_buffer.stride = 3 * sizeof(float);
		mesh.vertex_attrib.attrib_index = Vertex0;
		mesh.vertex_attrib.normalized = GL_FALSE;
		mesh.vertex_attrib.relative_offset = 0;
		mesh.vertex_attrib.size = 3;
		mesh.vertex_attrib.type = GL_FLOAT;
		mesh.faces = ARRAY_SIZE(faces);
		mesh.vertices = ARRAY_SIZE(faces) * 3;
		mesh.mode = GL_TRIANGLES;
		return mesh;
	}

	enum vertex_array_mode_t : GLenum {
		Triangles = GL_TRIANGLES,
	};

	struct vertex_array_t {
		static vertex_array_t create() {
			vertex_array_t vao{};
			glCreateVertexArrays(1, &vao.id);
			return vao;
		}

		vertex_array_t() = default;
		vertex_array_t(vertex_array_t&& another) noexcept {
			*this = std::move(another);
		}

		~vertex_array_t() {
			if (valid()) {
				glDeleteVertexArrays(1, &id);
			}
		}

		vertex_array_t& operator = (vertex_array_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
				std::swap(mode, another.mode);
				std::swap(count, another.count);
			} return *this;
		}

		void bind() const {
			glBindVertexArray(id);
		}

		bool valid() const {
			return id != 0;
		}

		GLuint id{};
		GLenum mode{};
		int count{};
	};

	vertex_array_t gen_vertex_array_from_mesh(const mesh_t& mesh) {
		vertex_array_t vao{};

		int bind_index = 0;
		glCreateVertexArrays(1, &vao.id);
		glVertexArrayVertexBuffer(vao.id, bind_index, mesh.id, mesh.vertex_buffer.offset, mesh.vertex_buffer.stride);
		glEnableVertexArrayAttrib(vao.id, mesh.vertex_attrib.attrib_index);
		glVertexArrayAttribFormat(vao.id,
								mesh.vertex_attrib.attrib_index,
								mesh.vertex_attrib.size,
								mesh.vertex_attrib.type,
								mesh.vertex_attrib.normalized,
								mesh.vertex_attrib.relative_offset);
		glVertexArrayAttribBinding(vao.id, mesh.vertex_attrib.attrib_index, bind_index);
		vao.mode = mesh.mode;
		vao.count = mesh.vertices;
		return vao;
	}

	enum framebuffer_attachment_type_t : GLenum {
		Color0 = GL_COLOR_ATTACHMENT0,
		Depth = GL_DEPTH_ATTACHMENT,
	};

	struct framebuffer_attachment_t {
		GLenum type{};
		GLuint texture{};
		GLint level{};
	};

	struct framebuffer_t {
		static framebuffer_t create() {
			framebuffer_t frame{};
			glCreateFramebuffers(1, &frame.id);
			return frame;
		}

		framebuffer_t() = default;
		framebuffer_t(framebuffer_t&& another) noexcept {
			*this = std::move(another);
		}

		~framebuffer_t() {
			if (valid()) {
				glDeleteFramebuffers(1, &id);	
			}
		}

		framebuffer_t& operator = (framebuffer_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
			}
			return *this;
		}

		void attach(const framebuffer_attachment_t& attachment) {
			glNamedFramebufferTexture(id, attachment.type, attachment.texture, attachment.level);
		}

		void detach(const framebuffer_attachment_t& attachment) {
			glNamedFramebufferTexture(id, attachment.type, 0, attachment.level);
		}

		bool get_complete_status(GLenum target) {
			return glCheckNamedFramebufferStatus(id, target) == GL_FRAMEBUFFER_COMPLETE;
		}

		void set_draw_buffers(GLsizei count, const GLenum *buffers) {
			glNamedFramebufferDrawBuffers(id, count, buffers);
		}

		void bind() const {
			glBindFramebuffer(GL_FRAMEBUFFER, id);
		}

		bool valid() const {
			return id != 0;
		}

		GLuint id{};
	};

	struct shader_t {
		static shader_t create(const std::string& source, GLenum type) {
			const GLchar* src = source.data();
			GLint size = source.size();

			shader_t shader{};
			shader.id = glCreateShader(type);
			shader.type = type;
			glShaderSource(shader.id, 1, &src, &size);
			glCompileShader(shader.id);
			return shader;
		}

		shader_t() = default;
		shader_t(shader_t&& another) noexcept {
			*this = std::move(another);
		}

		~shader_t() {
			if (valid()) {
				glDeleteShader(id);
			}
		}

		shader_t& operator = (shader_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
			}
			return *this;
		}

		bool valid() const {
			return id != 0;
		}

		bool compiled() const {
			GLint status;
			glGetShaderiv(id, GL_COMPILE_STATUS, &status);
			return status == GL_TRUE;
		}

		std::string get_info_log() const {
			int log_length = {};
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_length);

			GLsizei length{};
			std::string info_log(log_length, '\0');
			glGetShaderInfoLog(id, log_length, &length, info_log.data());
			info_log.resize(length); // cut unneccessary null-terminator
			assert(length + 1 == log_length);
			return info_log;
		}

		GLuint id{};
		GLenum type{};
	};

	struct uniform_props_t {
		static constexpr int total_props = 4;
		static constexpr const GLenum all_props[] = {GL_BLOCK_INDEX, GL_TYPE, GL_NAME_LENGTH, GL_LOCATION};

		static uniform_props_t from_array(const std::string& name, const GLint props[total_props]) {
			return uniform_props_t{
				.name = name, .block_index = props[0], .type = props[1], .name_length = props[2], .location = props[3],
			};
		}

		static GLint get_array_block_index(const GLint props[total_props]) {
			return props[0];
		}

		std::string name{};
		GLint block_index{};
		GLint type{};
		GLint name_length{};
		GLint location{};
	};

	const char* uniform_type_to_str(GLint type) {
		switch (type) {
			case GL_FLOAT: return "float";
			case GL_INT: return "int";
			case GL_FLOAT_VEC3: return "vec3";
			case GL_FLOAT_VEC4: return "vec4";
			case GL_FLOAT_MAT3: return "mat3";
			case GL_FLOAT_MAT4: return "mat4";
			default: return "unknown"; 
		}
	}

	std::ostream& operator << (std::ostream& os, const uniform_props_t& props) {
		os << "name: " << props.name
			<< " block_index: " << props.block_index
			<< " type: " << uniform_type_to_str(props.type)
			<< " name_length: " << props.name_length
			<< " location: " << props.location << std::endl;
		return os;
	}

	struct shader_program_t {
		using uniform_props_map_t = std::unordered_map<std::string, uniform_props_t>;

		static uniform_props_map_t get_uniform_props_map(GLuint id) {
			uniform_props_map_t uniforms;

			GLint active_uniforms{}, uniform_max_name_length{};
			glGetProgramInterfaceiv(id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &active_uniforms);
			glGetProgramInterfaceiv(id, GL_UNIFORM, GL_MAX_NAME_LENGTH, &uniform_max_name_length);

			std::string name;
			for (GLuint i = 0; i < active_uniforms; i++) {
				GLint props[uniform_props_t::total_props];
				glGetProgramResourceiv(id, GL_UNIFORM, i,
							uniform_props_t::total_props, uniform_props_t::all_props, uniform_props_t::total_props, nullptr, props);
				if (uniform_props_t::get_array_block_index(props) != -1) {
					continue;
				}

				GLsizei name_length;
				name.resize(uniform_max_name_length, '\0');
				glGetProgramResourceName(id, GL_UNIFORM, i, uniform_max_name_length, &name_length, name.data());
				name.resize(name_length); // cut unneccessary null-terminator

				uniforms[name] = uniform_props_t::from_array(name, props);
			}
			
			return uniforms;
		}

		template<class ... shader_t>
		static shader_program_t create(shader_t&& ... shader) {
			shader_program_t program;{} 
			program.id = glCreateProgram();
			(glAttachShader(program.id, shader.id), ...);
			glLinkProgram(program.id);
			(glDetachShader(program.id, shader.id), ...);

			if (program.linked()) {
				program.uniforms = get_uniform_props_map(program.id);
			}
			
			return program;
		}

		shader_program_t() = default;
		shader_program_t(shader_program_t&& another) noexcept {
			*this = std::move(another);
		}

		~shader_program_t() {
			if (valid()) {
				glDeleteProgram(id);
			}
		}

		shader_program_t& operator = (shader_program_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
				std::swap(uniforms, another.uniforms);
			}
			return *this;
		}

		void use() const {
			glUseProgram(id);
		}

		bool set_float(const char* name, GLfloat value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT);
				glProgramUniform1f(id, it->second.location, value);
				return true;
			}
			return false;
		}

		bool set_int(const char* name, GLint value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_INT);
				glProgramUniform1i(id, it->second.location, value);
				return true;
			}
			return false;
		}

		bool set_mat3(const char* name, const glm::mat3& value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT_MAT3);
				glProgramUniformMatrix3fv(id, it->second.location, 1, GL_FALSE, glm::value_ptr(value));
				return true;
			}
			return false;
		}

		bool set_mat4(const char* name, const glm::mat4& value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT_MAT4);
				glProgramUniformMatrix4fv(id, it->second.location, 1, GL_FALSE, glm::value_ptr(value));
				return true;
			}
			return false;
		}

		bool set_vec3(const char* name, const glm::vec3& value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT_VEC3);
				glProgramUniform3fv(id, it->second.location, 1, glm::value_ptr(value));
				return true;
			}
			return false;
		}

		bool set_vec4(const char* name, const glm::vec4& value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT_VEC4);
				glProgramUniform4fv(id, it->second.location, 1, glm::value_ptr(value));
				return true;
			}
			return false;
		}

		bool valid() const {
			return id != 0;
		}

		bool linked() const {
			GLint status;
			glGetProgramiv(id, GL_LINK_STATUS, &status);
			return status == GL_TRUE;
		}

		std::string get_info_log() const {
			int log_length = {};
			glGetProgramiv(id, GL_INFO_LOG_LENGTH, &log_length);

			GLsizei length = {};
			std::string info_log(log_length, '\0');
			glGetProgramInfoLog(id, log_length, &length, info_log.data());
			info_log.resize(length); // cut unneccessary null-terminator
			assert(length + 1 == log_length);
			return info_log;
		}

		GLuint id{};
		uniform_props_map_t uniforms;
	};

	const inline std::string basic_vert_source =
R"(
#version 460 core

layout(location = 0) in vec3 attr_pos;

uniform mat4 u_m;
uniform mat4 u_v;
uniform mat4 u_p;

out vec3 world_pos;

void main() {
	vec4 pos = u_m * vec4(attr_pos, 1.0);
	world_pos = pos.xyz;
	gl_Position = u_p * u_v * pos;
}
)";

	const inline std::string basic_frag_source =
R"(
#version 460 core

layout(location = 0) out vec4 color;

in vec3 world_pos;

uniform vec3 u_object_color;
uniform float u_shininess;
uniform float u_specular_strength;

uniform vec3 u_eye_pos;

uniform vec3 u_ambient_color;
uniform vec3 u_light_color;
uniform vec3 u_light_pos;

void main() {
	vec3 v0 = dFdxFine(world_pos);
	vec3 v1 = dFdyFine(world_pos);
	vec3 normal = normalize(cross(v0, v1));

	vec3 object_color = u_object_color;

	vec3 ambient = u_ambient_color;

	vec3 light_ray = normalize(u_light_pos - world_pos);
	float diffuse_coef = max(dot(light_ray, normal), 0.0);
	vec3 diffuse = diffuse_coef * u_light_color;

	vec3 look_ray = u_eye_pos - world_pos;
	float dist = length(look_ray);
	look_ray /= dist;

	vec3 spec_vec = normalize((look_ray + light_ray) * 0.5);
	float specular_coef = u_specular_strength * pow(clamp(dot(normal, spec_vec), 0.0, 1.0), u_shininess);
	vec3 specular = specular_coef * u_light_color;

	float att_coef = 1.0 / (0.1 + dist * dist);
	color = vec4((ambient + 64.0 * (diffuse + specular) * att_coef) * object_color, 1.0);
}
)";

	const inline std::string basic_vert_instanced_source =
R"(
#version 460 core

layout(location = 0) in vec3 attr_pos;

layout(std430, binding = 0) readonly buffer instanced_m {
    mat4 m[];
};

uniform mat4 u_v;
uniform mat4 u_p;

out vec3 world_pos;
flat out int object_id;

void main() {
	u_v;
	vec4 pos = m[gl_InstanceID] * vec4(attr_pos, 1.0);
	world_pos = pos.xyz;
	object_id = gl_InstanceID;
	gl_Position = u_p * pos;
}
)";

	const inline std::string basic_frag_instanced_source =
R"(
#version 460 core

layout(location = 0) out vec4 color;

in vec3 world_pos;
flat in int object_id;

struct material {
	vec3 object_color;
	float shininess;
	float specular_strength;
};

layout(std430, binding = 1) readonly buffer instanced_mtl {
    material mtl[];
};

uniform vec3 u_eye_pos;

uniform vec3 u_ambient_color;
uniform vec3 u_light_color;
uniform vec3 u_light_pos;

void main() {
	vec3 object_color = mtl[object_id].object_color;
	vec3 dummy = u_eye_pos + u_ambient_color + u_light_color + u_light_color;
	vec3 d = world_pos - u_light_pos;
	float att = 1.0f / length(d);
	color = vec4(object_color * u_light_color * att, 1.0);
}
)";

	std::tuple<shader_program_t, std::string> gen_basic_shader_program() {
		std::ostringstream out;

		shader_t vert = shader_t::create(basic_vert_source, GL_VERTEX_SHADER);
		if (!vert.compiled()) {
			out << "*** Failed to compile vert shader:\n" << vert.get_info_log() << "\n";
		}

		shader_t frag = shader_t::create(basic_frag_source, GL_FRAGMENT_SHADER);
		if (!frag.compiled()) {
			out << "*** Failed to compile frag shader:\n" << frag.get_info_log() << "\n";
		}

		shader_program_t program = shader_program_t::create(vert, frag);
		if (!program.linked()) {
			out << "*** Failed to link program:\n" << program.get_info_log() << "\n";
			program = shader_program_t{};
		}
		return std::make_tuple(std::move(program), out.str());
	}

	std::tuple<shader_program_t, std::string> gen_basic_instanced_shader_program() {
		std::ostringstream out;

		shader_t vert = shader_t::create(basic_vert_instanced_source, GL_VERTEX_SHADER);
		if (!vert.compiled()) {
			out << "*** Failed to compile vert shader:\n" << vert.get_info_log() << "\n";
		}

		shader_t frag = shader_t::create(basic_frag_instanced_source, GL_FRAGMENT_SHADER);
		if (!frag.compiled()) {
			out << "*** Failed to compile frag shader:\n" << frag.get_info_log() << "\n";
		}

		shader_program_t program = shader_program_t::create(vert, frag);
		if (!program.linked()) {
			out << "*** Failed to link program:\n" << program.get_info_log() << "\n";
			program = shader_program_t{};
		}
		return std::make_tuple(std::move(program), out.str());
	}

	// very simple write-only buffer that is mapped by default
	struct gpu_buffer_t {
		gpu_buffer_t(std::uint64_t _size) {
			size = (_size + 63) & ~(std::uint64_t)63;
			size *= 2;

			glCreateBuffers(1, &buffer_id);
			glNamedBufferStorage(buffer_id, size, nullptr, GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT /*| GL_MAP_COHERENT_BIT*/);
			mapped_pointer = glMapNamedBufferRange(buffer_id, 0, size, GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT /*| GL_MAP_COHERENT_BIT*/);
		}

		~gpu_buffer_t() {
			glUnmapNamedBuffer(buffer_id);
			glDeleteBuffers(1, &buffer_id);
		}

		void* get_front() const {
			return mapped_pointer;
		}

		void* get_back() const {
			return (char*)mapped_pointer + size / 2;
		}

		GLuint buffer_id{};
		std::uint64_t size{};
		void* mapped_pointer{};
	};


	namespace impl {
		template<class __handle_t, auto __null_handle>
		struct handle_pool_t {
		public:
			using handle_t = __handle_t;

			static constexpr handle_t null_handle = __null_handle;

		private:
			void extend() {
				std::size_t old_size = handles.size();
				std::size_t new_size = 2 * old_size + 1;
				handles.resize(new_size);
				for (std::size_t i = old_size + 1; i < new_size; i++) {
					handles[i - 1] = i;
				}
				handles[new_size - 1] = head;
				head = old_size;
			}

		public:
			[[nodiscard]] handle_t acquire() {
				if (head == null_handle) {
					extend();
				}
				handle_t handle = head;
				head = handles[handle];
				handles[handle] = handle;
				return handle;
			}

			void release(handle_t handle) {
				assert(is_used(handle));
				handles[handle] = head;
				head = handle;
			}

			bool is_used(handle_t handle) const {
				if (handle != null_handle && handle < handles.size()) {
					return handles[handle] == handle;
				}
				return false;
			}

			void clear() {
				head = null_handle;
				handles.clear();
			}

			std::size_t get_use_count() const {
				return handles.size();
			}

		private:
			std::vector<handle_t> handles;
			handle_t head{null_handle};
		};
	}
	
	using handle_t = unsigned;
	inline constexpr handle_t null_handle = ~0;

	using handle_pool_t = impl::handle_pool_t<handle_t, null_handle>;

	template<class tag_t>
	struct type_id_t {
	private:
		inline static std::size_t counter = 0;

	public:
		template<class type_t>
		static std::size_t get() {
			static const std::size_t id = counter++;
			return id;
		}

		static std::size_t count() {
			return counter;
		}
	};

	template<class iter_t>
	struct iter_helper_t {
		iter_t begin() {
			return iter_begin;
		}

		iter_t end() {
			return iter_end;
		}

		iter_t iter_begin, iter_end;
	};

	template<class if_t>
	class if_storage_t {
		template<class object_t>
		static std::size_t object_id() { return type_id_t<if_t>::template get<object_t>(); }

		template<class object_t>
		if_storage_t(std::shared_ptr<if_t> _if_ptr, object_t* _true_ptr)
			: if_ptr{std::move(_if_ptr)}, true_ptr{_true_ptr}, id{object_id<object_t>()} {}

	public:
		template<class object_t, class ... args_t>
		static if_storage_t create(args_t&& ... args) {
			auto entry_ptr = std::make_shared<object_t>(std::forward<args_t>(args)...);
			return if_storage_t{entry_ptr, entry_ptr.get()};
		}

		template<class object_t>
		static if_storage_t create(object_t* object) {
			return if_storage_t{std::shared_ptr<if_t>(object), object};
		}

		template<class object_t>
		static if_storage_t create(std::shared_ptr<object_t> object_ptr) {
			return if_storage_t{object_ptr, object_ptr.get()};
		}

		if_storage_t(if_storage_t&& another) noexcept {
			*this = std::move(another);
		}

		if_storage_t& operator = (if_storage_t&& another) noexcept {
			std::swap(if_ptr, another.if_ptr);
			std::swap(true_ptr, another.true_ptr);
			std::swap(id, another.id);
			return *this;
		}

		template<class object_t>
		auto* get() const {
			assert(id == object_id<object_t>());
			return (object_t*)true_ptr;
		}

		if_t* get_if() const {
			return if_ptr.get();
		}

	private:
		std::shared_ptr<if_t> if_ptr{}; // to simplify things up
		void* true_ptr{}; // true pointer to entry to avoid dynamic_cast (can be directly cast to object using static_cast)
		std::size_t id{};
	};


	template<class resource_t>
	using resource_ptr_t = std::shared_ptr<resource_t>;

	template<class resource_t>
	using resource_ref_t = std::weak_ptr<resource_t>;

	class resource_registry_t {
		struct resource_tag_t;

		template<class resource_t>
		static std::size_t resource_id() { return type_id_t<resource_tag_t>::template get<resource_t>(); }

		class resource_entry_if_t {
		public:
			virtual ~resource_entry_if_t() {}
		};

		using resource_storage_t = if_storage_t<resource_entry_if_t>;

		template<class resource_t>
		class resource_entry_t : public resource_entry_if_t {
		public:
			void add(const std::string& name, resource_ptr_t<resource_t> resource) {
				entry[name] = resource;
			}

			void remove(const std::string& name) {
				entry.erase(name);
			}

			resource_ptr_t<resource_t> get(const std::string& name) {
				if (auto it = entry.find(name); it != entry.end()) {
					return it->second;
				} return {};
			}

			resource_ref_t<resource_t> get_ref(const std::string& name) {
				return get(name);
			}

			void clear() {
				entry.clear();
			}

		private:
			std::unordered_map<std::string, resource_ptr_t<resource_t>> entry;
		};


		template<class resource_t>
		auto& acquire_entry() {
			using entry_t = resource_entry_t<resource_t>;

			std::size_t res_id = resource_id<resource_t>();
			if (auto it = entries.find(resource_id<resource_t>()); it != entries.end()) {
				return *it->second.template get<entry_t>();
			}

			auto [it, inserted] = entries.insert({resource_id<resource_t>(), resource_storage_t::create<entry_t>()});
			return *it->second.template get<entry_t>();
		}

	public:
		template<class resource_t>
		void add(const std::string& name, resource_ptr_t<resource_t> resource) {
			acquire_entry<resource_t>().add(name, resource);
		}

		template<class resource_t>
		void remove(const std::string& name) {
			acquire_entry<resource_t>().remove(name);
		}

		template<class resource_t>
		auto get(const std::string& name) {
			return acquire_entry<resource_t>().get(name);
		}

		template<class resource_t>
		auto get_ref(const std::string& name) {
			return acquire_entry<resource_t>().get_ref(name);
		}

		template<class resource_t>
		void clear() {
			acquire_entry<resource_t>().clear();
		}

		void clear() {
			entries.clear();
		}

	private:
		std::unordered_map<std::size_t, resource_storage_t> entries;
	};


	class component_registry_t {
		struct component_tag_t;

		template<class component_t>
		static std::size_t component_id() { return type_id_t<component_tag_t>::template get<component_t>(); }

		class component_entry_if_t {
		public:
			virtual ~component_entry_if_t() {}
			virtual void remove(handle_t handle) = 0;
			virtual void clear() = 0;
		};

		using component_storage_t = if_storage_t<component_entry_if_t>;

		template<class component_t>
		class component_entry_t : public component_entry_if_t {
		public:
			template<class ... args_t>
			component_t* emplace(handle_t handle, args_t&& ... args) {
				if constexpr(std::is_aggregate_v<component_t>) {
					if (auto [it, inserted] = components.emplace(std::piecewise_construct,
						std::forward_as_tuple(handle), std::forward_as_tuple(std::forward<args_t>(args)...)); inserted) {
						return &it->second;
					}
					return nullptr;
				} else {
					if (auto [it, inserted] = components.insert({handle, component_t{std::forward<args_t>(args)...}}); inserted) {
						return &it->second;
					}
					return nullptr;
				}
			}

			component_t* add(handle_t handle, const component_t& component) {
				if (auto [it, inserted] = components.insert({handle, component}); inserted) {
					return &it->second;
				}
				return nullptr;
			}

			component_t* add(handle_t handle, component_t&& component) {
				if (auto [it, inserted] = components.insert({handle, std::move(component)}); inserted) {
					return &it->second;
				}
				return nullptr;
			}

			virtual void remove(handle_t handle) override {
				components.erase(handle);
			}

			component_t* get(handle_t handle) {
				if (auto it = components.find(handle); it != components.end()) {
					return &it->second;
				}
				return nullptr;
			}

			component_t* peek() {
				if (!components.empty()) {
					return &components.begin()->second;
				}
				return nullptr;
			}

			std::size_t count() const {
				return components.size();
			}

			auto begin() {
				return components.begin();
			}

			auto end() {
				return components.end();
			}

			virtual void clear() override {
				auto first = begin(), last = end();
				while (first != last) {
					auto curr = first++;
					components.erase(curr);
				}
			}

		private:
			std::unordered_map<handle_t, component_t> components;
		};

		template<class component_t>
		auto& acquire_entry() {
			using entry_t = component_entry_t<component_t>;
			
			if (auto it = entries.find(component_id<component_t>()); it != entries.end()) {
				return *it->second.template get<entry_t>();
			}

			auto [it, inserted] = entries.insert({component_id<component_t>(), component_storage_t::create<entry_t>()});
			return *it->second.template get<entry_t>();
		}

	public:
		void release(handle_t handle) {
			for (auto& [id, entry] : entries) {
				entry.get_if()->remove(handle);
			}
		}

		template<class component_t, class ... args_t>
		component_t* emplace(handle_t handle, args_t&& ... args) {
			return acquire_entry<component_t>().emplace(handle, std::forward<args_t>(args)...);
		}

		template<class component_t>
		component_t* add(handle_t handle, const component_t& component) {
			return acquire_entry<component_t>().add(handle, component);
		}

		template<class component_t>
		component_t* add(handle_t handle, component_t&& component) {
			return acquire_entry<component_t>().add(handle, std::move(component));
		}

		template<class component_t>
		void remove(handle_t handle) {
			acquire_entry<component_t>().remove(handle);
		}

		template<class component_t>
		component_t* get(handle_t handle) {
			return acquire_entry<component_t>().get(handle);
		}

		template<class component_t>
		component_t* peek() {
			return acquire_entry<component_t>().peek();
		}

		template<class component_t>
		std::size_t count() const {
			return acquire_entry<component_t>().count();
		}

		template<class component_t>
		auto begin_iter() {
			return acquire_entry<component_t>().begin();
		}

		template<class component_t>
		auto end_iter() {
			return acquire_entry<component_t>().end();
		}

		template<class component_t>
		auto iterate() {
			return iter_helper_t{begin_iter<component_t>(), end_iter<component_t>()};
		}

		// void func(handle_t, component_t&)
		// it is safe to erase component while iterating
		template<class component_t, class func_t>
		void for_each(func_t&& func) {
			auto [first, last] = iterate<component_t>();
			while (first != last) {
				auto curr = first++;
				func(curr->first, curr->second);
			}
		}

		template<class component_t>
		void clear() {
			acquire_entry<component_t>().clear();
		}

		void clear() {
			for (auto& [id, entry] : entries) {
				entry.get_if()->clear();
			}
		}

	private:
		std::unordered_map<std::size_t, component_storage_t> entries;
	};

	// nothing else for now, no-copy and no-move by default
	class engine_ctx_t;

	class system_if_t {
	public:
		system_if_t(engine_ctx_t* _ctx) : ctx{_ctx} {}
		system_if_t(const system_if_t&) = delete;
		system_if_t(system_if_t&&) noexcept = delete;

		virtual ~system_if_t() {}

		system_if_t& operator = (const system_if_t&) = delete;
		system_if_t& operator = (system_if_t&&) noexcept = delete;

		inline engine_ctx_t* get_ctx() const {
			return ctx;
		}

	private:
		engine_ctx_t* ctx{};
	};

	class system_registry_t {
		using system_storage_t = if_storage_t<system_if_t>;

	public:
		template<class system_t>
		bool add(const std::string& name, std::shared_ptr<system_t> sys) {
			auto [it, inserted] = systems.insert({name, system_storage_t::create(std::move(sys))});
			return inserted;
		}

		bool remove(const std::string& name) {
			return systems.erase(name);
		}

		template<class system_t>
		system_t* get(const std::string& name) {
			if (auto it = systems.find(name); it != systems.end()) {
				return it->second.get<system_t>();
			}
			return nullptr;
		}

		void clear() {
			systems.clear();
		}

	private:
		std::unordered_map<std::string, system_storage_t> systems;
	};

	// systems are allowed to own a resource, so they can call get_resource() (well, this can also be not good, but for now they are allowed)
	// entities are not allowed to own any resource so they MUST call get_resource_ref()
	// TODO : how to process null_handle ? (most probably, ignore). now null_handle is considered invalid
	// TODO : releasing a handle during destruction causes crash (TODO : check, probably an old comment)
	// TODO : get_all_components<component...> - returns tuple of pointers to the appropriate components if an entity has all components set
	// TODO : get_some_components<component...> -  returns tuple of pointers to the appropriate components, some pointers can be null if component does not exist
	class engine_ctx_t {
	public:
		~engine_ctx_t() {
			clear();
		}


		template<class resource_t>
		void add_resource(const std::string& name, resource_ptr_t<resource_t> resource) {
			resource_registry.add<resource_t>(name, std::move(resource));
		}

		template<class resource_t>
		void remove_resource(const std::string& name) {
			resource_registry.remove<resource_t>(name);
		}

		template<class resource_t>
		auto get_resource(const std::string& name) {
			return resource_registry.get<resource_t>(name);
		}

		template<class resource_t>
		auto get_resource_ref(const std::string& name) {
			return resource_registry.get_ref<resource_t>(name);
		}

		template<class resource_t>
		void clear_resources() {
			resource_registry.clear<resource_t>();
		}

		void clear_resources() {
			resource_registry.clear();
		}


		bool is_alive(handle_t handle) const {
			return handles.is_used(handle) && refcount[handle] > 0;
		}

		bool is_alive_weak(handle_t handle) const {
			return handles.is_used(handle) && weakrefcount[handle] > 0;
		}

		[[nodiscard]] handle_t acquire() {
			handle_t handle = handles.acquire();
			if (auto size = handles.get_use_count(); refcount.size() < size) {
				refcount.resize(size);
				weakrefcount.resize(size);
			}
			refcount[handle] = 1;
			weakrefcount[handle] = 1;
			return handle;
		}

		[[nodiscard]] handle_t incref(handle_t handle) {
			if (!is_alive(handle)) {
				std::cerr << "trying to incref invalid handle " << handle << std::endl;
				return null_handle;
			}
			refcount[handle]++;
			return handle;
		}

		[[nodiscard]] handle_t incweakref(handle_t handle) {
			if (!is_alive_weak(handle)) {
				std::cerr << "trying to weakincref invalid handle " << handle << std::endl;
				return null_handle;
			}
			weakrefcount[handle]++;
			return handle;
		}

		void release(handle_t handle) {
			if (!is_alive(handle)) {
				std::cerr << "trying to release invalid handle " << handle << std::endl;
				return;
			}
			refcount[handle]--;
			if (refcount[handle] == 0) {
				component_registry.release(handle);
				
				weakrefcount[handle]--;
				if (weakrefcount[handle] == 0) {
					handles.release(handle);
				}
			}
		}

		void release_weak(handle_t handle) {
			if (!is_alive_weak(handle)) {
				std::cerr << "trying to release invalid handle " << handle << std::endl;
				return;
			}
			// TODO : we should not decrement if there are any strong refs
			weakrefcount[handle]--;
			if (weakrefcount[handle] == 0) {
				handles.release(handle);
			}
		}

		template<class component_t, class ... args_t>
		component_t* add_component(handle_t handle, args_t&& ... args) {
			if (!is_alive(handle)) {
				std::cerr << "trying to add component to invalid handle " << handle << std::endl;
				return nullptr;
			}
			return component_registry.emplace<component_t>(handle, std::forward<args_t>(args)...);
		}

		template<class component_t>
		component_t* add_component(handle_t handle, const component_t& component) {
			if (!is_alive(handle)) {
				std::cerr << "trying to add component to invalid handle " << handle << std::endl;
				return nullptr;
			}
			return component_registry.add<component_t>(handle, component);
		}

		template<class component_t>
		component_t* add_component(handle_t handle, component_t&& component) {
			if (!is_alive(handle)) {
				std::cerr << "trying to add component to invalid handle " << handle << std::endl;
				return nullptr;
			}
			return component_registry.add<component_t>(handle, std::move(component));
		}

		template<class component_t>
		void remove_component(handle_t handle) {
			if (!is_alive(handle)) {
				std::cerr << "trying to remove component using invalid handle " << handle << std::endl;
				return;
			}
			component_registry.remove<component_t>(handle);
		}

		template<class component_t>
		component_t* get_component(handle_t handle) {
			if (!is_alive(handle)) {
				std::cerr << "trying to get component using invalid handle " << handle << std::endl;
				return nullptr;
			}
			return component_registry.get<component_t>(handle);
		}

		template<class component_t>
		component_t* peek_component() {
			return component_registry.peek<component_t>();
		}

		template<class component_t>
		std::size_t count_components() {
			return component_registry.count<component_t>();
		}

		template<class component_t>
		auto iterate_components() {
			return component_registry.iterate<component_t>();
		}

		// void func(handle_t, component_t&)
		// it is dafe to erase component while iterating
		template<class component_t, class func_t>
		void for_each_component(func_t&& func) {
			component_registry.for_each<component_t>(std::forward<func_t>(func));
		}

		template<class component_t>
		void clear_components() {
			component_registry.clear<component_t>();
		}

		void clear_components() {
			component_registry.clear();
		}


		template<class system_t>
		bool add_system(const std::string& name, std::shared_ptr<system_t> sys) {
			if (!system_registry.add(name, std::move(sys))) {
				std::cerr << "failed to add system " << std::quoted(name) << std::endl;
				return false;
			}
			return true;
		}

		bool remove_system(const std::string& name) {
			if (!system_registry.remove(name)) {
				std::cerr << "failed to remove system " << std::quoted(name) << std::endl;
				return false;
			}
			return true;
		}

		template<class system_t>
		system_t* get_system(const std::string& name) {
			if (system_t* sys = system_registry.get<system_t>(name)) {
				return sys;
			}
			std::cerr << "failed to get system " << std::quoted(name) << std::endl;
			return nullptr;
		}

		void clear_systems() {
			system_registry.clear();
		}


		void clear() {
			clear_components();
			clear_resources();
			clear_systems();
			handles.clear();
			refcount.clear();
			weakrefcount.clear();
		}

	private:
		handle_pool_t handles;
		std::vector<int> refcount;
		std::vector<int> weakrefcount;
		system_registry_t system_registry;
		component_registry_t component_registry;
		resource_registry_t resource_registry;
	};

	// it is just an utility, wrapper of handle_t
	class entity_t {
	public:
		entity_t() = default;
		entity_t(engine_ctx_t* _ctx) : ctx{_ctx}, handle{_ctx->acquire()} {}
		entity_t(engine_ctx_t* _ctx, handle_t _handle) : ctx{_ctx}, handle{_handle} {}

		entity_t(entity_t&&) noexcept = default;
		entity_t(const entity_t&) = default;

		~entity_t() = default;

		entity_t& operator = (entity_t&&) noexcept = default;
		entity_t& operator = (const entity_t&) = default;

		template<class component_t, class ... args_t>
		component_t* add_component(args_t&& ... args) {
			return ctx->add_component<component_t>(handle, std::forward<args_t>(args)...);
		}

		template<class component_t>
		component_t* add_component(const component_t& component) {
			return ctx->add_component<component_t>(handle, component);
		}

		template<class component_t>
		component_t* add_component(component_t&& component) {
			return ctx->add_component<component_t>(handle, std::move(component));
		}

		template<class component_t>
		void remove_component() {
			ctx->remove_component<component_t>(handle);
		}

		template<class component_t>
		component_t* get_component() {
			return ctx->get_component<component_t>(handle);
		}

		bool is_alive() const {
			return ctx->is_alive(handle);
		}

		bool is_alive_weak() const {
			return ctx->is_alive_weak(handle);
		}

		[[nodiscard]] entity_t incref() const {
			return entity_t{ctx, ctx->incref(handle)};
		}

		[[nodiscard]] entity_t incweakref() const {
			return entity_t{ctx, ctx->incweakref(handle)};
		}

		void release() {
			ctx->release(handle);
			reset();
		}

		void release_weak() {
			ctx->release_weak(handle);
			reset();
		}

		engine_ctx_t* get_ctx() const {
			return ctx;
		}

		handle_t get_handle() const {
			return handle;
		}

		void reset() {
			ctx = nullptr;
			handle = null_handle;
		}

		bool empty() const {
			return ctx == nullptr;
		}

	private:
		engine_ctx_t* ctx{};
		handle_t handle{null_handle};
	};

	class shared_entity_t {
	public:
		shared_entity_t() = default;
		shared_entity_t(entity_t _entity) : entity{_entity} {}
		shared_entity_t(const shared_entity_t& another) : entity{another.entity.incref()} {}
		shared_entity_t(shared_entity_t&& another) noexcept {
			*this = std::move(another);
		}

		~shared_entity_t() {
			if (!empty()) {
				reset();
			}
		}

		shared_entity_t& operator = (entity_t _entity) noexcept {
			reset();
			entity = std::move(_entity);
			return *this;
		}

		shared_entity_t& operator = (const shared_entity_t& another) {
			if (this != &another) {
				reset();
				entity = another.entity.incref();
			}
			return *this;
		}

		shared_entity_t& operator = (shared_entity_t&& another) noexcept {
			if (this != &another) {
				std::swap(entity, another.entity);
			}
			return *this;
		}

		entity_t& get() {
			return entity;
		}

		void reset() {
			assert(!empty());
			entity.release();
		}

		bool empty() const {
			return entity.empty();
		}

	private:
		entity_t entity{};
	};

	class weak_entity_t {
	public:
		weak_entity_t() = default;
		weak_entity_t(entity_t _entity) : entity{_entity} {}
		weak_entity_t(const weak_entity_t& another) : entity{another.entity.incweakref()} {}
		weak_entity_t(weak_entity_t&& another) noexcept {
			*this = std::move(another);
		}

		~weak_entity_t() {
			if (!empty()) {
				reset();
			}
		}

		weak_entity_t& operator = (entity_t _entity) noexcept {
			reset();
			entity = std::move(_entity);
			return *this;
		}

		weak_entity_t& operator = (const weak_entity_t& another) {
			if (this != &another) {
				reset();
				entity = another.entity.incweakref();
			}
			return *this;
		}

		weak_entity_t& operator = (weak_entity_t&& another) noexcept {
			if (this != &another) {
				std::swap(entity, another.entity);
			}
			return *this;
		}

		entity_t& get() {
			return entity;
		}

		void reset() {
			assert(!empty());
			entity.release_weak();
		}

		bool empty() const {
			return entity.empty();
		}

	private:
		entity_t entity{};
	};


	template<class data_t>
	struct mt_queue_t {
		template<class _data_t>
		void push(_data_t&& data) {
			std::unique_lock lock_guard{lock};
			queue.push(std::forward<_data_t>(data));
			added.notify_one();
		}

		data_t pop() {
			std::unique_lock lock_guard{lock};
			if (queue.empty()) {
				added.wait(lock_guard, [&] (){
					return !queue.empty();
				});
			}
			data_t data = std::move(queue.front());
			queue.pop();
			return data;
		}

		std::mutex lock;
		std::condition_variable added;
		std::queue<data_t> queue;
	};

	struct job_if_t {
		job_if_t() = default;
		virtual ~job_if_t() = default;

		job_if_t(job_if_t&&) noexcept = delete;
		job_if_t& operator= (job_if_t&&) noexcept = delete;

		job_if_t(const job_if_t&) = delete;
		job_if_t& operator= (const job_if_t&) = delete;

		virtual void execute() = 0;

		void set_ready() {
			std::unique_lock lock_guard{lock};
			ready_status = true;
			ready.notify_all();
		}

		void wait() {
			std::unique_lock lock_guard{lock};
			ready.wait(lock_guard, [&] (){
				return ready_status;
			});
			ready_status = false;
		}

		std::mutex lock;
		std::condition_variable ready;
		bool ready_status{};
	};

	struct dummy_job_t : public job_if_t {
		void execute() override
		{}
	};

	template<class func_t>
	struct func_job_t : public job_if_t {
		template<class _func_t>		
		func_job_t(_func_t&& _func)
			: func{std::forward<_func_t>(_func)}
		{}

		void execute() override {
			func();
		}

		func_t func;
	};

	template<class func_t>
	func_job_t(func_t&& func) -> func_job_t<func_t>;


	struct thread_pool_t {
		static constexpr int thread_count_fallback = 8;

		thread_pool_t(int thread_count = 0) {
			if (thread_count <= 0) {
				thread_count = thread_count_fallback;
			}
			for (int i = 0; i < thread_count; i++) {
				workers.push_back(std::thread([&]() {
					thread_pool_worker_func();
				}));
			}
		}

		~thread_pool_t() {
			terminating.store(true, std::memory_order_relaxed); // queue will sync variable 'terminating' due to acquire-release in the underlying mutex

			std::vector<dummy_job_t> dummy_jobs(workers.size());
			for (auto& job : dummy_jobs) {
				push_job(&job);
			}

			std::unique_lock lock_guard{lock};
			worker_terminated.wait(lock_guard, [&] (){
				return workers_terminated == workers.size();
			});
			lock_guard.unlock();

			for (auto& worker : workers) {
				worker.join();
			}
		}

		void thread_pool_worker_func() {
			while (!terminating.load(std::memory_order_relaxed)) { // will be synced in a destructor
				job_if_t* job = job_queue.pop();
				job->execute();
				job->set_ready();
			}

			std::unique_lock lock_guard{lock};
			workers_terminated++;
			worker_terminated.notify_one();
		}

		void push_job(job_if_t* job) {
			job_queue.push(job);
		}

		std::vector<std::thread> workers;
		mt_queue_t<job_if_t*> job_queue;

		std::mutex lock;
		std::condition_variable worker_terminated;
		int workers_terminated{};
		std::atomic<bool> terminating{};
	};


	using tick_t = float;
	using timer_component_t = dt_timer_t<tick_t>;

	// TODO : implement
	class timer_system_t : public system_if_t {
	public:
		timer_system_t(engine_ctx_t* ctx) : system_if_t(ctx) {}

	private:
		void update_timer(timer_component_t& timer, tick_t dt) {
			while (dt > tick_t(0)) {
				if (auto action = timer.update(dt)) {
					action();
				}
			}
		}

	public:
		void update(tick_t dt) {
			for (auto& [handle, timer] : get_ctx()->iterate_components<timer_component_t>()) {
				//update_timer(timer, dt);
			}
		}
	};


	struct transform_t {
		glm::mat4 to_mat4() const {
			auto mat_scale = glm::scale(base, scale);
			auto mat_rotation = glm::mat4_cast(rotation);
			auto mat_translation = glm::translate(glm::mat4(1.0f), translation);
			return mat_translation * mat_rotation * mat_scale;
		}

		glm::mat4 base{};
		glm::vec3 scale{};
		glm::quat rotation{};
		glm::vec3 translation{};
	};


	using sync_callback_t = std::function<void(entity_t entity)>;

	struct sync_component_t {
		void operator() (entity_t entity) {
			if (callback) {
				callback(entity);
			}
		}

		sync_callback_t callback{};
	};

	class sync_component_system_t : public system_if_t {
	public:
		sync_component_system_t(engine_ctx_t* ctx) : system_if_t(ctx) {}

		void update() {
			auto* ctx = get_ctx();
			for (auto& [handle, sync] : ctx->iterate_components<sync_component_t>()) {
				sync(entity_t{ctx, handle});
			}
		}
	};


	struct camera_t {
		glm::mat4 get_view() const {
			return glm::translate(glm::lookAt(eye, center, up), -eye);
		}

		glm::mat4 get_proj(float aspect_ratio) const {
			return glm::perspective(glm::radians(45.0f), aspect_ratio, near, far);
		}

		float fov{};
		float near{};
		float far{};
		glm::vec3 eye{};
		glm::vec3 center{};
		glm::vec3 up{};
	};

	struct omnidir_light_t {
		glm::vec3 ambient{};
		glm::vec3 color{};
		glm::vec3 pos{};
	};

	struct viewport_t {
		float get_aspect_ratio() const {
			return (float)width / height;
		}

		int x{}, y{}, width{}, height{};
	};


	struct imgui_pass_t {
		resource_ref_t<framebuffer_t> framebuffer;
		viewport_t viewport{};
		glm::vec4 clear_color{};
		float clear_depth{};
	};

	class imgui_system_t : public system_if_t {
	public:
		using gui_callback_t = std::function<bool()>;

		imgui_system_t(engine_ctx_t* ctx, GLFWwindow* window, const std::string& version) : system_if_t(ctx) {
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGui::StyleColorsDark();
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init(version.c_str());
			ImPlot::CreateContext();

			register_callback([&] () {
				ImGui::ShowDemoWindow();
				return true;
				});
		}

		~imgui_system_t() {
			ImPlot::DestroyContext();
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}

		void do_gui() {
			int read_pos = 0;
			int write_pos = 0;
			int count = callbacks.size();
			while (read_pos < count) {
				if (callbacks[read_pos]()) {
					if (read_pos != write_pos) {
						callbacks[write_pos] = std::move(callbacks[read_pos]);
					}
					++write_pos;
				}
				++read_pos;
			}
			callbacks.resize(write_pos);
		}

	public:
		void update() {
			// TODO : save/restore utility
			struct save_restore_render_state_t {
				save_restore_render_state_t() {
					glGetIntegerv(GL_VIEWPORT, viewport);
					glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
					glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear_depth);
					glGetBooleanv(GL_DEPTH_TEST, &depth_test);
					glGetBooleanv(GL_CULL_FACE, &cull_face);
				}

				~save_restore_render_state_t() {
					glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
					glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
					glClearDepth(clear_depth);

					if (depth_test) {
						glEnable(GL_DEPTH_TEST);
					} else {
						glDisable(GL_DEPTH_TEST);
					}

					if (cull_face) {
						glEnable(GL_CULL_FACE);
					} else {
						glDisable(GL_CULL_FACE);
					}
				}

				GLfloat clear_color[4] = {};
				GLfloat clear_depth{};
				GLint viewport[4]{};
				GLboolean depth_test{};
				GLboolean cull_face{};
			} state;

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			do_gui();
			ImGui::Render();

			// actual rendering
			for (auto& [handle, pass] : get_ctx()->iterate_components<imgui_pass_t>()) {
				if (auto framebuffer = pass.framebuffer.lock()) {
					framebuffer->bind();
				} else {
					std::cerr << "framebuffer expired" << std::endl;
					continue;
				}

				auto& viewport = pass.viewport;
				glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
				auto& col = pass.clear_color;
				glClearColor(col.r, col.g, col.b, col.a);
				glClearDepth(pass.clear_depth);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			}
		}

		void register_callback(gui_callback_t callback) {
			callbacks.push_back(std::move(callback));
		}

	private:
		std::vector<gui_callback_t> callbacks{};
	};


	class basic_resources_system_t : public system_if_t {
	public:
		basic_resources_system_t(engine_ctx_t* ctx, int tex_width, int tex_height) : system_if_t(ctx) {
			auto basic_program_ptr = ([&] () {
				auto [program, info_log] = gen_basic_shader_program();
				if (!program.valid()) {
					std::cerr << info_log << std::endl;
					return std::make_shared<shader_program_t>();
				}
				return std::make_shared<shader_program_t>(std::move(program));
			})();

			auto basic_instanced_program_ptr = ([&] () {
				auto [program, info_log] = gen_basic_instanced_shader_program();
				if (!program.valid()) {
					std::cerr << info_log << std::endl;
					return std::make_shared<shader_program_t>();
				}
				return std::make_shared<shader_program_t>(std::move(program));
			})();

			auto sphere_mesh_ptr = std::make_shared<mesh_t>(gen_sphere_mesh(3));
			auto sphere_vao_ptr = std::make_shared<vertex_array_t>(gen_vertex_array_from_mesh(*sphere_mesh_ptr));
			auto pyramid_mesh_ptr = std::make_shared<mesh_t>(gen_pyramid_mesh());
			auto pyramid_vao_ptr = std::make_shared<vertex_array_t>(gen_vertex_array_from_mesh(*pyramid_mesh_ptr));
			auto color = std::make_shared<texture_t>(gen_empty_texture(tex_width, tex_height, Rgba32f));
			auto depth = std::make_shared<texture_t>(gen_depth_texture(tex_width, tex_height, Depth32f));

			ctx->add_resource<shader_program_t>("basic_program", std::move(basic_program_ptr));
			ctx->add_resource<shader_program_t>("basic_instanced_program", std::move(basic_instanced_program_ptr));
			ctx->add_resource<mesh_t>("sphere", std::move(sphere_mesh_ptr));
			ctx->add_resource<vertex_array_t>("sphere", std::move(sphere_vao_ptr));
			ctx->add_resource<mesh_t>("pyramid", std::move(pyramid_mesh_ptr));
			ctx->add_resource<vertex_array_t>("pyramid", std::move(pyramid_vao_ptr));
			ctx->add_resource<texture_t>("color", std::move(color));
			ctx->add_resource<texture_t>("depth", std::move(depth));
		}

		~basic_resources_system_t() {
			// completely unneccessary
			auto* ctx = get_ctx();
			ctx->remove_resource<texture_t>("depth");
			ctx->remove_resource<texture_t>("color");
			ctx->remove_resource<vertex_array_t>("sphere");
			ctx->remove_resource<mesh_t>("sphere");
			ctx->remove_resource<shader_program_t>("basic_program");
			ctx->remove_resource<shader_program_t>("basic_instanced_program");
		} 
	};


	struct basic_material_t {
		glm::vec3 color{};
		float specular_strength{};
		float shininess{};
		resource_ref_t<vertex_array_t> vao;
	};

	struct basic_pass_t {
		resource_ref_t<framebuffer_t> framebuffer;
		viewport_t viewport{};
		glm::vec4 clear_color{};
		float clear_depth{};
		weak_entity_t camera{};
		weak_entity_t light{};
	};

	#define std430_vec3 alignas(sizeof(glm::vec4)) glm::vec3
	#define std430_vec4 alignas(sizeof(glm::vec4)) glm::vec4
	#define std430_float alignas(sizeof(float)) float

	class basic_renderer_system_t : public system_if_t {
	public:
		struct std430_basic_material_t {
			std430_vec3 color{};
			std430_float shininess{};
			std430_float specular_strength{};
		};

		static constexpr int max_instances = 100000;

		basic_renderer_system_t(engine_ctx_t* ctx) : system_if_t(ctx) {
			program = get_ctx()->get_resource<shader_program_t>("basic_program");
			instanced_program = get_ctx()->get_resource<shader_program_t>("basic_instanced_program");

			sphere = get_ctx()->get_resource<vertex_array_t>("sphere");

			m_size = max_instances * sizeof(glm::mat4);
			object_mtl_size = max_instances * sizeof(std430_basic_material_t);

			instance_buffer = std::make_unique<gpu_buffer_t>(m_size + object_mtl_size + 2 * 256);

			m_offset = (((std::uintptr_t)instance_buffer->mapped_pointer + 255ull) & ~255ull) - (std::uintptr_t)instance_buffer->mapped_pointer;
			object_mtl_offset = (m_offset + m_size + 255ull) & ~255ull;

			m = (glm::mat4*)((char*)instance_buffer->mapped_pointer + m_offset);
			object_mtl = (std430_basic_material_t*)((char*)instance_buffer->mapped_pointer + object_mtl_offset);

			auto* imgui = get_ctx()->get_system<imgui_system_t>("imgui");
			imgui->register_callback([&] () {
				ImGui::SetNextWindowSize(ImVec2{256, 256}, ImGuiCond_Once);
				if (ImGui::Begin("basic renderer")) {
					ImGui::Checkbox("submit data", &submit_data);
				}
				ImGui::End();
				return true;
			});
		}

	private:
		void render_pass_instanced(basic_pass_t* pass, camera_t* camera, omnidir_light_t* light) {
			// TODO : create utility to save/restore context
			struct save_restore_render_state_t {
				save_restore_render_state_t() {
					glGetIntegerv(GL_VIEWPORT, viewport);
					glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
					glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear_depth);
					glGetBooleanv(GL_DEPTH_TEST, &depth_test);
					glGetBooleanv(GL_CULL_FACE, &cull_face);
				}

				~save_restore_render_state_t() {
					glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
					glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
					glClearDepth(clear_depth);

					if (depth_test) {
						glEnable(GL_DEPTH_TEST);
					} else {
						glDisable(GL_DEPTH_TEST);
					}

					if (cull_face) {
						glEnable(GL_CULL_FACE);
					} else {
						glDisable(GL_CULL_FACE);
					}
				}

				GLfloat clear_color[4] = {};
				GLfloat clear_depth{};
				GLint viewport[4]{};
				GLboolean depth_test{};
				GLboolean cull_face{};
			} state;

			if (auto framebuffer = pass->framebuffer.lock()) {
				framebuffer->bind();
			} else {
				std::cerr << "framebuffer expired" << std::endl;
				return;
			}

			auto* ctx = get_ctx();

			int object_count = 0;
			if (submit_data) {
				for (auto& [handle, material] : ctx->iterate_components<basic_material_t>()) {
					auto* transform = ctx->get_component<transform_t>(handle);
					if (!transform) {
						std::cerr << "missing transform component. handle: " << handle << std::endl;
						continue;
					}

					m[object_count] = transform->to_mat4();

					auto& mtl = object_mtl[object_count];
					mtl.color = material.color;
					mtl.shininess = material.shininess;
					mtl.specular_strength = material.specular_strength;

					object_count++;
				}
				last_submitted = object_count;
			} else {
				object_count = last_submitted;
			}

			auto& viewport = pass->viewport;
			glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
			auto& col = pass->clear_color;
			glClearColor(col.r, col.g, col.b, col.a);
			glClearDepth(pass->clear_depth);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);

			instanced_program->use();
			//instanced_program->set_mat4("u_v", camera->get_view());
			instanced_program->set_mat4("u_p", camera->get_proj(viewport.get_aspect_ratio()) * camera->get_view());
			instanced_program->set_vec3("u_eye_pos", camera->eye);
			instanced_program->set_vec3("u_ambient_color", light->ambient);
			instanced_program->set_vec3("u_light_color", light->color);
			instanced_program->set_vec3("u_light_pos", light->pos);

			sphere->bind();
			/*glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, instance_buffer->buffer_id, m_offset, m_size);
			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, instance_buffer->buffer_id, object_mtl_offset, object_mtl_size);
			glDrawArraysInstanced(sphere->mode, 0, sphere->count, object_count);*/

			int split = 8;
			int part = object_count / split;
			for (int start = 0; start < object_count; start += part) {
				int inst_count = std::min(start + part, object_count) - start;

				glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, instance_buffer->buffer_id, m_offset + sizeof(glm::mat4) * start, sizeof(glm::mat4) * inst_count);
				glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, instance_buffer->buffer_id, object_mtl_offset + sizeof(std430_basic_material_t) * start, sizeof(std430_basic_material_t) * inst_count);
				glDrawArraysInstanced(sphere->mode, 0, sphere->count, inst_count);
			}
		}

	public:
		void update() {
			for (auto& [handle, pass] : get_ctx()->iterate_components<basic_pass_t>()) {
				auto* camera = pass.camera.get().get_component<camera_t>();
				if (!camera) {
					std::cerr << "missing camera component. handle: " << handle << " camera: " << pass.camera.get().get_handle() << std::endl;
					continue;
				}

				auto* light = pass.light.get().get_component<omnidir_light_t>();
				if (!light) {
					std::cerr << "missing light component. handle: " << handle << " light: " << pass.light.get().get_handle() << std::endl;
					continue;
				}

				render_pass_instanced(&pass, camera, light);
			}
		}

	private:
		resource_ptr_t<shader_program_t> program;
		resource_ptr_t<shader_program_t> instanced_program;
		resource_ptr_t<vertex_array_t> sphere;

		std::unique_ptr<gpu_buffer_t> instance_buffer;
		std::uint64_t m_offset{};
		std::uint64_t m_size{};
		glm::mat4* m{};
		std::uint64_t object_mtl_offset{};
		std::uint64_t object_mtl_size{};
		std430_basic_material_t* object_mtl{};

		bool submit_data = true;
		int last_submitted{};
	};


	class window_system_t : public system_if_t {
	public:
		window_system_t(engine_ctx_t* ctx, int window_width, int window_height) : system_if_t(ctx) {
			glfw_guard = std::make_unique<glfw::guard_t>();
			window = std::make_unique<glfw::window_t>(
				glfw::window_params_t::create_basic_opengl("yin-yang", window_width, window_height, 4, 6)
			);
			window->make_ctx_current();
			glew_guard = std::make_unique<glew_guard_t>();

			/*if (GLEW_ARB_gpu_shader_int64) {
				if (glewGetExtension("GL_ARB_gpu_shader_int64")) {
					std::cout << "we got 'GL_ARB_gpu_shader_int64'!\n";
				} else {
					std::cerr << "We failed to get 'GL_ARB_gpu_shader_int64'";
				}
			} else {
				std::cerr << "shit...\n";
			}*/
		}

		glfw::window_t& get_window() {
			return *window;
		}

	private:
		std::unique_ptr<glfw::guard_t> glfw_guard;
		std::unique_ptr<glfw::window_t> window;
		std::unique_ptr<glew_guard_t> glew_guard;
	};


	// TODO : octotree
	// TODO : multithreaded update
	struct aabb_t {
		glm::vec3 min{};
		glm::vec3 max{};
	};

	struct sphere_t {
		glm::vec3 center{};
		float radius{};
	};

	bool test_intersection_aabb_aphere(const aabb_t& aabb, const sphere_t& sphere) {
		glm::vec3 dr = sphere.center - glm::clamp(sphere.center, aabb.min, aabb.max);
		return dr.x * dr.x + dr.y * dr.y + dr.z * dr.z <= sphere.radius * sphere.radius;
	}

	bool test_intersection_aabb_aabb(const aabb_t& aabb1, const aabb_t& aabb2) {
		return glm::all(glm::lessThanEqual(aabb1.min, aabb2.max) && glm::lessThanEqual(aabb2.min, aabb1.max));
	}

	bool test_intersection_aabb_point(const aabb_t& aabb, const glm::vec3& point) {
		return glm::all(glm::lessThanEqual(aabb.min, point) && glm::lessThanEqual(point, aabb.max));
	}


	// indices from range [-2^31 + 1, to 2^31 - 2]
	constexpr int sparse_cell_base = 1 << 30;

	using sparse_cell_t = glm::ivec3;

	struct sparse_cell_hasher_t {
		static std::uint32_t h32(std::uint32_t h) {
			h ^= (h >> 16);
			h *= 0x85ebca6b;
			h ^= (h >> 13);
			h *= 0xc2b2ae35;
			h ^= (h >> 16);
			return h;
		}

		std::uint32_t operator() (const sparse_cell_t& cell) const {
			std::uint32_t x = h32(cell.x);
			std::uint32_t y = h32(cell.y);
			std::uint32_t z = h32(cell.z);
			return (x * 0xc2b2ae35 + y) * 0x85ebca6b + z;
		}
	};

	struct sparse_cell_equals_t {
		bool operator() (const sparse_cell_t& cell1, const sparse_cell_t& cell2) const {
			return cell1 == cell2;
		}
	};

	// cell_scale = 1.0f / cell_size
	sparse_cell_t get_sparse_cell(const glm::vec3& point, float cell_scale, float cell_min, float cell_max) {
		return glm::clamp(glm::floor(point * cell_scale), glm::vec3{cell_min}, glm::vec3{cell_max});
	}

	template<class data_t>
	using sparse_cell_storage_t = std::unordered_map<sparse_cell_t, data_t, sparse_cell_hasher_t>;

	using sparse_cell_set_t = std::unordered_set<sparse_cell_t, sparse_cell_hasher_t>;

	struct sparse_query_result_t {
		bool valid() const {
			return head != -1;
		}

		sparse_cell_t cell{};
		int head{};
		int count{};
	};

	static constexpr sparse_query_result_t bad_sparse_query = sparse_query_result_t{sparse_cell_t{}, -1, 0};

	template<class __data_t>
	struct sparse_grid_t {
		using data_t = __data_t;

		struct data_entry_t {
			int next = -1; // index of the next entry in data_storage
			data_t data{};
		};

		struct storage_entry_t {
			int head = -1; // first index in data_entries
			int count = 0; // count of data_entries
		};

		using cell_storage_t = sparse_cell_storage_t<storage_entry_t>;
		using cell_storage_iterator_t = typename cell_storage_t::iterator;

		using data_storage_t = std::vector<data_entry_t>;
		using data_storage_iterator_t = typename data_storage_t::iterator;

		sparse_grid_t(float _cell_size, float _cell_min, float _cell_max)
			: cell_scale{1.0f / _cell_size}
			, cell_min{_cell_min}
			, cell_max{_cell_max}
		{}

		template<class _data_t>
		void add(const glm::vec3& point, _data_t&& data) {
			auto [it, _] = cell_storage.insert({get_sparse_cell(point, cell_scale, cell_min, cell_max), storage_entry_t{-1, 0}});
			auto& storage_entry = it->second;
			data_storage.emplace_back(storage_entry.head, std::forward<_data_t>(data));
			storage_entry.head = (int)data_storage.size() - 1;
			storage_entry.count++;
		}

		void clear() {
			cell_storage.clear();
			data_storage.clear();
		}

		sparse_query_result_t find_cell(const sparse_cell_t& cell) const {
			if (auto it = cell_storage.find(cell); it != cell_storage.end()) {
				return sparse_query_result_t{it->first, it->second.head, it->second.count};
			}
			return bad_sparse_query;
		}

		// search checking all cells
		void query_linear(const sparse_cell_t& min, const sparse_cell_t& max, std::vector<sparse_query_result_t>& result) const {
			for (auto& [cell, entry] : cell_storage) {
				if (min.x <= cell.x && cell.x <= max.x &&
					min.y <= cell.y && cell.y <= max.y &&
					min.z <= cell.z && cell.z <= max.z) {
					result.push_back(sparse_query_result_t{cell, entry.head, entry.count});
				}
			}
		}

		void query_linear(const glm::vec3& min, const glm::vec3& max, std::vector<sparse_query_result_t>& result) const {
			auto cell0 = get_sparse_cell(min, cell_scale, cell_min, cell_max);
			auto cell1 = get_sparse_cell(max, cell_scale, cell_min, cell_max);
			query_linear(cell0, cell1, result);
		}

		// search from subspace
		void query(const sparse_cell_t& min, const sparse_cell_t& max, std::vector<sparse_query_result_t>& result) const {
			auto count = max - min + sparse_cell_t{1};
			for (int i = 0; i < count.x; i++) {
				for (int j = 0; j < count.y; j++) {
					for (int k = 0; k < count.z; k++) {
						auto cell = min + sparse_cell_t{i, j, k};
						if (auto query_result = find_cell(cell); query_result.valid()) {
							result.push_back(query_result);
						}
					}
				}
			}
		}

		void query(const glm::vec3& min, const glm::vec3& max, std::vector<sparse_query_result_t>& result) const {
			auto cell0 = get_sparse_cell(min, cell_scale, cell_min, cell_max);
			auto cell1 = get_sparse_cell(max, cell_scale, cell_min, cell_max);
			query(cell0, cell1, result);
		}

		const cell_storage_t& get_cell_storage() const {
			return cell_storage;
		}

		data_entry_t& get_data_entry(int index) {
			return data_storage[index];
		}

		const data_entry_t& get_data_entry(int index) const {
			return data_storage[index];
		}

		cell_storage_t cell_storage;
		data_storage_t data_storage;
		float cell_scale{};
		float cell_min{};
		float cell_max{};
	};

	template<class _sparse_grid_t>
	struct sparse_data_iterator_t {
		template<class __sparse_grid_t>
		sparse_data_iterator_t(__sparse_grid_t& _grid, int _head)
			: grid{&_grid}
			, head{_head}
		{}

		bool valid() const {
			return head != -1;
		}

		auto& get() {
			return grid->get_data_entry(head);
		}

		void next() {
			head = get().next;
		}

		_sparse_grid_t* grid;
		int head;
	};

	template<class __sparse_grid_t>
	sparse_data_iterator_t(__sparse_grid_t&, int) -> sparse_data_iterator_t<__sparse_grid_t>;


	int nextlog2(int n) {
		int power = 0;
		int next = 1;
		while (next < n) {
			next <<= 1;
			power++;
		}
		return power;
	}

	int nextpow2(int n) {
		int next = 1;
		while (next < n) {
			next <<= 1;
		}
		return next;
	}

	int log2size(int n) {
		return 1 << n;
	}

	template<class>
	struct callback_t;

	template<class ret_t, class ... args_t>
	struct callback_t<ret_t(args_t...)>{
		template<class ctx_t>
		using ctx_func_t = ret_t(ctx_t*, args_t...);

		callback_t() = default;

		template<class ctx_t>
		callback_t(ctx_t& _ctx, ctx_func_t<ctx_t>* _func)
			: ctx{&_ctx}
			, func{(ctx_func_t<void>*)_func}
		{}

		template<class ctx_t>
		callback_t(ctx_t* _ctx, ctx_func_t<ctx_t>* _func)
			: ctx{_ctx}
			, func{(ctx_func_t<void>*)_func}
		{}

		ret_t operator() (args_t ... args) const {
			return func(ctx, std::forward<args_t>(args)...);
		}

		void* ctx{};
		ctx_func_t<void>* func{};
	};

	using lofi_hasher_t = callback_t<std::uint32_t(int)>;
	using lofi_equals_t = callback_t<bool(int, int)>;

	struct lofi_hashtable_t {
		static constexpr int inflation_coef = 2;

		struct bucket_t {
			void reset() {
				head.store(-1, std::memory_order_relaxed);
				count.store(0, std::memory_order_relaxed);
			}

			std::atomic<int> head{}; // default : -1
			std::atomic<int> count{}; // default : 0
		};

		struct list_entry_t {
			int next{};
		};

		lofi_hashtable_t(const lofi_hashtable_t&) = delete;
		lofi_hashtable_t& operator= (const lofi_hashtable_t&) = delete;
		lofi_hashtable_t(lofi_hashtable_t&&) noexcept = delete;
		lofi_hashtable_t& operator= (lofi_hashtable_t&&) noexcept = delete;

		lofi_hashtable_t(int _max_size, int dummy, lofi_hasher_t _hasher, lofi_equals_t _equals)
			: hasher{_hasher}
			, equals{_equals}
			, max_capacity{nextpow2(_max_size) * inflation_coef}
		{
			buckets = std::make_unique<bucket_t[]>(max_capacity);
			list_entries = std::make_unique<list_entry_t[]>(max_capacity / inflation_coef);
		}

		// master
		void reset_size(int new_object_count, int dummy) {
			object_count = std::min(max_capacity / inflation_coef, new_object_count);
			capacity_m1 = std::min(max_capacity, nextpow2(object_count) * inflation_coef) - 1;
			capacity_log2 = std::countr_zero<unsigned>(capacity_m1 + 1);
		}

		// worker
		// used by worker to reset the range of buckets
		bucket_t* get_buckets_data(int start = 0) {
			return buckets.get() + start;
		}

		// worker
		// returns either index of a bucket that was just used or -1 if it was already used
		struct put_result_t {
			int bucket_index{};
			int scans{};
		};

		put_result_t put(int item) {
			int bucket_index = hash_to_index(hasher(item));
			int scans = 1;
			while (true) {
				auto& bucket = buckets[bucket_index];

				int head_old = bucket.head.load(std::memory_order_relaxed);
				if (head_old == -1 && bucket.head.compare_exchange_strong(head_old, item, std::memory_order_relaxed)) {
					bucket.count.fetch_add(1, std::memory_order_relaxed);
					list_entries[item].next = item;
					return {bucket_index, scans};
				}
				if (equals(head_old, item)) { // totaly legit to use possibly invalid head here
					bucket.count.fetch_add(1, std::memory_order_relaxed);
					list_entries[item].next = bucket.head.exchange(item, std::memory_order_relaxed); // head can be invalid
					return {-1, scans};
				}

				bucket_index = (bucket_index + 1) & capacity_m1;
				scans++;
			}
		}


		// worker & master
		const bucket_t& get_bucket(int index) const {
			return buckets[index];
		}

		const bucket_t& get(int item) const {
			return buckets[hash_to_index(hasher(item))];
		}

		int hash_to_index(std::uint32_t hash) const {
			return ((hash >> capacity_log2) + hash) & capacity_m1; // add upper bits to lower
		}

		int get_buckets_count() const {
			return capacity_m1 + 1;
		}

		int get_object_count() const {
			return object_count;
		}


		int capacity_m1{};
		int capacity_log2{};

		lofi_hasher_t hasher;
		lofi_equals_t equals;

		std::unique_ptr<bucket_t[]> buckets; // resized to capacity (increased capacity to decrease contention)
		std::unique_ptr<list_entry_t[]> list_entries; // size = object_count
	
		const int max_capacity;
		int object_count{};
	};

	struct lofi_hashtable1_t {
		static constexpr int scans_force_insert = 32;
		static constexpr int scans_force_insert_m1 = scans_force_insert - 1;
		
		static constexpr int inflation_coef = 4;

		static int hash_to_offset(std::uint32_t hash) {
			return (hash * 0x85ebca6b + (hash >> 17)) & scans_force_insert_m1;
		}

		struct bucket_t {
			void reset() {
				head.store(-1, std::memory_order_relaxed);
				count.store(0, std::memory_order_relaxed);
			}

			std::atomic<int> head{}; // default : -1
			std::atomic<int> count{}; // default : 0
		};

		struct list_entry_t {
			int next{};
		};

		lofi_hashtable1_t(const lofi_hashtable1_t&) = delete;
		lofi_hashtable1_t& operator= (const lofi_hashtable1_t&) = delete;
		lofi_hashtable1_t(lofi_hashtable1_t&&) noexcept = delete;
		lofi_hashtable1_t& operator= (lofi_hashtable1_t&&) noexcept = delete;

		lofi_hashtable1_t(int _max_object_count, int _max_bucket_count, lofi_hasher_t _hasher, lofi_equals_t _equals)
			: max_object_count{_max_object_count}
			, max_bucket_count{inflation_coef * nextpow2(_max_bucket_count)}
			, hasher{_hasher}
			, equals{_equals}
		{
			buckets = std::make_unique<bucket_t[]>(max_bucket_count);
			list_entries = std::make_unique<list_entry_t[]>(max_object_count);
		}

		// master
		void reset_size(int new_object_count, int new_bucket_count) {
			object_count = std::min(max_object_count, new_object_count);
			bucket_count_m1 = std::min(max_bucket_count, inflation_coef * nextpow2(new_bucket_count)) - 1;
			bucket_count_log2 = std::countr_zero<unsigned>(bucket_count_m1 + 1);
		}

		// worker
		// used by worker to reset the range of buckets
		bucket_t* get_buckets_data(int start = 0) {
			return buckets.get() + start;
		}

		// worker
		// returns either index of a bucket that was just used or -1 if it was already used
		struct put_result_t {
			int bucket_index{};
			int scans{};
		};

		put_result_t put(int item) {
			std::uint32_t hash = hasher(item);

			// main insertion attempts
			int bucket_index = hash_to_index(hash);
			int scans = 1;
			while (scans <= scans_force_insert) {
				auto& bucket = buckets[bucket_index];

				int head_old = bucket.head.load(std::memory_order_relaxed);
				if (head_old == -1 && bucket.head.compare_exchange_strong(head_old, item, std::memory_order_relaxed)) {
					bucket.count.fetch_add(1, std::memory_order_relaxed);
					list_entries[item].next = item;
					return {bucket_index, scans};
				}
				if (equals(head_old, item)) { // totaly legit to use possibly invalid head here
					bucket.count.fetch_add(1, std::memory_order_relaxed);
					list_entries[item].next = bucket.head.exchange(item, std::memory_order_relaxed); // head can be invalid
					return {-1, scans};
				}

				bucket_index = (bucket_index + 1) & bucket_count_m1;
				scans++;
			}

			// forcing insertion, allows collision
			auto& bucket = buckets[(hash_to_index(hash) + hash_to_offset(hash)) & bucket_count_m1];
			bucket.count.fetch_add(1, std::memory_order_relaxed);
			list_entries[item].next = bucket.head.exchange(item, std::memory_order_relaxed);
			return {-1, scans_force_insert + 1};
		}


		// worker & master
		const bucket_t& get_bucket(int index) const {
			return buckets[index];
		}

		const bucket_t& get(int item) const {
			return buckets[hash_to_index(hasher(item))];
		}

		int hash_to_index(std::uint32_t hash) const {
			return ((hash >> bucket_count_log2) + hash) & bucket_count_m1; // add upper bits to lower
		}

		int get_buckets_count() const {
			return bucket_count_m1 + 1;
		}

		int get_object_count() const {
			return object_count;
		}

		int object_count{};
		int bucket_count_m1{};
		int bucket_count_log2{};

		lofi_hasher_t hasher;
		lofi_equals_t equals;

		std::unique_ptr<bucket_t[]> buckets; // resized to capacity (increased capacity to decrease contention)
		std::unique_ptr<list_entry_t[]> list_entries; // size = object_count

		const int max_object_count;
		const int max_bucket_count;
	};

	template<class data_t>
	struct lofi_stack_t {
		lofi_stack_t(int _max_size)
			: max_size{_max_size} {
			data = std::make_unique<data_t[]>(max_size);
		}

		// master
		void reset() {
			size.store(0, std::memory_order_relaxed);
		}

		// worker
		data_t* push(int count) {
			int start = size.fetch_add(count, std::memory_order_relaxed);
			assert(start + count <= max_size);
			return data.get() + start;
		}

		// worker
		data_t* get_data(int start) {
			return data.get() + start;
		}

		int get_size() const {
			return size.load(std::memory_order_relaxed);
		}

		const int max_size;
		std::unique_ptr<data_t[]> data;
		std::atomic<int> size{};
	};


	// point attractor
	// a = GM / |r - r0|^2 * (r - r0) / |r - r0|
	struct attractor_t {
		glm::vec3 pos{};
		float gm{};
		float min_dist{};
		float max_dist{};
		float drag_min_coef{};
		float drag_max_coef{};
		float drag_min_dist{};
		float drag_max_dist{};
	};

	// directional force
	struct force_t {
		glm::vec3 dir{};
		float mag{};
	};

	struct physics_t {
		bool valid() const {
			return !(glm::any(glm::isnan(pos)) || glm::any(glm::isnan(vel)));
		}

		glm::vec3 pos{};
		glm::vec3 vel{};
		glm::vec3 force{};
		float mass{};
		float radius{};
		bool no_update{};
	};

	struct physics_system_info_t {
		float eps{};
		float overlap_coef{};
		float overlap_vel_coef{};
		float overlap_thresh{};
		float overlap_spring_coef{};
		int overlap_resolution_iters{};
		float movement_limit{};
		float velocity_limit{};
		float touch_thresh{};
		float touch_coef_workaround{};
		float impact_cor{};
		float impact_v_loss{};
		float o2o{};
		int dt_split{};
	};

	class physics_system_t : public system_if_t {
	public:
		static constexpr float no_collision = 2.0f;

		physics_system_t(engine_ctx_t* ctx, const physics_system_info_t& info)
			: system_if_t(ctx)
			, eps{info.eps}
			, overlap_coef{info.overlap_coef}
			, overlap_vel_coef{info.overlap_vel_coef}
			, overlap_thresh{info.overlap_thresh}
			, overlap_spring_coef{info.overlap_spring_coef}
			, overlap_resolution_iters{std::max(info.overlap_resolution_iters, 1)}
			, movement_limit{info.movement_limit}
			, velocity_limit{info.velocity_limit}
			, touch_thresh{info.touch_thresh}
			, touch_coef_workaround{info.touch_coef_workaround}
			, impact_cor{info.impact_cor}
			, impact_v_loss{info.impact_v_loss}
			, o2o{info.o2o}
			, dt_split{info.dt_split}
			, grid{0.5f, -1000000.0, 1000000.0}
		{
			auto* imgui_system = ctx->get_system<imgui_system_t>("imgui");

			imgui_system->register_callback([&] (){
				if (ImGui::Begin("physics")) {
					ImGui::SetNextItemWidth(-1.0f);
					if (ImGui::SliderFloat("##o2o", &o2o, 0.0f, 500.0f, "o2o %.2f")) {
						set_o2o(o2o);
					}
					ImGui::Checkbox("enable", &enabled);
				}
				ImGui::End();
				return true;
			});
		}

	private:
		void cache_components() {
			cached_components.clear();
			index_to_handle.clear();
			for (auto& [handle, component] : get_ctx()->iterate_components<physics_t>()) {
				index_to_handle.push_back(handle);
				cached_components.push_back(&component);
			}
			total_objects = index_to_handle.size();
		}

		void initialize_grid() {
			grid.clear();

			int count = cached_components.size();
			for (int i = 0; i < count; i++) {
				auto* component = cached_components[i];
				grid.add(component->pos, i);
			}
		}

		glm::vec3 limit_vec(const glm::vec3& vec, float max_len) const {
			if (float len = glm::length(vec); len > max_len && len > eps) {
				return vec * (max_len / len);
			}
			return vec;
		}

		glm::vec3 limit_movement(const glm::vec3& r0, const glm::vec3& r1, float max_len) const {
			return r0 + limit_vec(r1 - r0, max_len);
		}

		struct body_state_t {
			glm::vec3 pos{};
			glm::vec3 vel{};
			glm::vec3 force{};
			float mass{};
		};

		glm::vec3 compute_force(const body_state_t& state) {
			glm::vec3 acc{};

			for (auto& [handle, attractor] : get_ctx()->iterate_components<attractor_t>()) {
				glm::vec3 dr = attractor.pos - state.pos;
				float dr_mag2 = glm::dot(dr, dr);
				float dr_mag = std::sqrt(dr_mag2);
				if (attractor.min_dist <= dr_mag && dr_mag <= attractor.max_dist) {
					acc += attractor.gm / dr_mag2 * dr / dr_mag;
				}

				if (attractor.drag_min_dist <= dr_mag && dr_mag <= attractor.drag_max_dist) {
					float c0 = attractor.drag_min_coef;
					float c1 = attractor.drag_max_coef;
					float d0 = attractor.drag_min_dist;
					float d1 = attractor.drag_max_dist;
					float coef = (1.0f - c1 + c0) * glm::smoothstep(d1, d0, dr_mag) + c0;
					acc -= coef * state.vel;
				}
			}

			for (auto& [handle, force] : get_ctx()->iterate_components<force_t>()) {
				acc += force.dir * force.mag;
			}

			return acc;
		}

		// dumb as shit
		glm::vec3 compute_obj2obj_force(int i) {
			auto* physics_i = cached_components[i];

			query_result.clear();
			grid.query(physics_i->pos - physics_i->radius, physics_i->pos + physics_i->radius, query_result);

			glm::vec3 acc{};
			for (auto [cell, head, count] : query_result) {
				for (sparse_data_iterator_t it{grid, head}; it.valid(); it.next()) {
					int j = it.get().data;
					if (i == j) {
						continue;
					}

					auto* physics_j = cached_components[j];
					glm::vec3 dr = physics_i->pos - physics_j->pos;

					float r = glm::length(dr);
					if (r <= eps) {
						continue;
					}

					float r0 = physics_i->radius + physics_j->radius;
					acc += o2o * std::clamp(r0 - r, 0.0f, r0) * (dr / r);
				}
			}
			return acc;
		}

		// simplification, compute acceleration beforehand
		// this method will update state considering it acceleration as const
		body_state_t integrate_motion(const body_state_t& state, float dt) {
			body_state_t new_state = state;
			glm::vec3 acc = state.force / state.mass;
			glm::vec3 vel_tmp = state.vel + 0.5f * acc * dt;
			new_state.pos = state.pos + vel_tmp * dt;
			new_state.vel = vel_tmp + 0.5f * acc * dt;
			return new_state;
		}

		void get_integrator_updates(float dt) {
			integrator_updates.clear();
			for (int i = 0; i < total_objects; i++) {
				auto& physics = *cached_components[i];

				auto old_state = body_state_t{physics.pos, physics.vel, physics.force, physics.mass};
				if (physics.no_update) {
					integrator_updates.push_back(old_state);
					continue;
				}

				old_state.force += compute_force(old_state);
				old_state.force += compute_obj2obj_force(i);

				auto new_state = integrate_motion(old_state, dt);
				new_state.pos = limit_movement(old_state.pos, new_state.pos, movement_limit);
				new_state.vel = limit_vec(new_state.vel, velocity_limit);
				integrator_updates.push_back(new_state);
			}
		}

		void update_physics() {
			if (!enabled) {
				return;
			}

			for (int i = 0; i < total_objects; i++) {
				auto& physics = *cached_components[i];
				auto& updated = integrator_updates[i];
				physics.pos = updated.pos;
				physics.vel = updated.vel;
				physics.force = glm::vec3(0.0f);

				// TODO : move somewhere else
				if (!physics.valid()) {
					physics.pos = glm::vec3(0.0f);
					physics.vel = glm::vec3(0.0f);
				}

				if (glm::length(physics.pos) > 200.0f) {
					glm::vec3 dir = glm::normalize(physics.pos);
					physics.pos -= 50.0f * dir;
					physics.vel = -std::abs(glm::dot(physics.vel, dir)) * dir;
				}
			}
		}

	public:
		void update(float dt) {
			cache_components();
			initialize_grid();
			for (int i = 0; i < dt_split; i++) {
				get_integrator_updates(dt / dt_split);
				update_physics();
			}
			frame++;
		}

		float get_o2o() const { return o2o; }
		void set_o2o(float value) { o2o = value; }

	private:
		float eps{};
		float overlap_coef{};
		float overlap_vel_coef{};
		float overlap_thresh{};
		float overlap_spring_coef{};
		int overlap_resolution_iters{};
		float movement_limit{};
		float velocity_limit{};
		float touch_thresh{};
		float touch_coef_workaround{};
		float impact_cor{};
		float impact_v_loss{};
		float o2o{};
		int dt_split{};

		bool enabled{true};

		int frame{};
		std::size_t total_objects{};
		std::vector<physics_t*> cached_components;
		std::vector<handle_t> index_to_handle;
		std::vector<body_state_t> integrator_updates;
		sparse_grid_t<int> grid;
		std::vector<sparse_query_result_t> query_result;
	};


	using seed_t = std::uint64_t;

	seed_t shuffle(seed_t value) {
		return std::rotl(value, 17) * 0x123456789ABCDEF0 + std::rotr(value, 17);
	}

	class basic_int_gen_t {
	public:
		basic_int_gen_t(seed_t seed) : base_gen(seed) {}

		int gen() {
			return base_gen();
		}

	private:
		std::minstd_rand base_gen;
	};

	class int_gen_t {
	public:
		int_gen_t(seed_t seed, int a, int b) : base_gen(seed), distr(a, b) {}

		int gen() {
			return distr(base_gen);
		}

	private:
		std::minstd_rand base_gen;
		std::uniform_int_distribution<> distr;
	};

	class float_gen_t {
	public:
		float_gen_t(seed_t seed, float a, float b) : base_gen(shuffle(seed)), distr(a, b) {}

		float gen() {
			return distr(base_gen);
		}

	private:
		std::minstd_rand base_gen;
		std::uniform_real_distribution<> distr;
	};	

	class rgb_color_gen_t {
	public:
		rgb_color_gen_t(seed_t seed)
			: r_gen(shuffle(seed    ), 0.0f, 1.0f)
			, g_gen(shuffle(seed + 1), 0.0f, 1.0f)
			, b_gen(shuffle(seed + 2), 0.0f, 1.0f
			) {}

		glm::vec3 gen() {
			float r = r_gen.gen(), g = g_gen.gen(), b = b_gen.gen();
			return glm::vec3(r, g, b);
		}

	private:
		float_gen_t r_gen, g_gen, b_gen;
	};

	class hsl_color_gen_t {
	public:
		hsl_color_gen_t(seed_t seed) : h_gen(shuffle(seed), 0.0f, 360.0f) {}

		glm::vec3 gen() {
			float h = h_gen.gen();
			return glm::vec3(h, 1.0f, 1.0f);
		}

	private:
		float_gen_t h_gen;
	};

	// TODO : test
	glm::vec3 hsl_to_rgb(const glm::vec3& hsl) {
		float h = hsl.x, s = hsl.y, l = hsl.z;

		float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
		float ht = h / 60.0f;
		float ht_q, ht_r;
		ht_r = std::modf(ht, &ht_q);
		float x = c * (1.0f - std::abs(ht_r - 1.0f));

		glm::vec3 rgb{};
		switch(int(ht)) {
			case 0: rgb = glm::vec3(c, x, 0); break;
			case 1: rgb = glm::vec3(x, c, 0); break;
			case 2: rgb = glm::vec3(0, c, x); break;
			case 3: rgb = glm::vec3(0, x, c); break;
			case 4: rgb = glm::vec3(x, 0, c); break;
			case 5: rgb = glm::vec3(c, 0, x); break;
		}

		return rgb + glm::vec3(l - c * 0.5f);
	}

	class hsl_to_rgb_color_gen_t : public hsl_color_gen_t {
	public:
		using base_t = hsl_color_gen_t;

		hsl_to_rgb_color_gen_t(seed_t seed) : base_t(seed) {}

		glm::vec3 gen() {
			return hsl_to_rgb(base_t::gen());
		}
	};

	using hsv_color_gen_t = hsl_color_gen_t;

	glm::vec3 hsv_to_rgb(const glm::vec3& hsv) {
		float h = hsv.x, s = hsv.y, v = hsv.z;

		float c = v * s;
		float ht = h / 60.0f;
		float ht_q, ht_r;
		ht_r = std::modf(ht, &ht_q);
		float x = c * (1.0f - std::abs(ht_r - 1.0f));

		glm::vec3 rgb{};
		switch(int(ht)) {
			case 0: rgb = glm::vec3(c, x, 0); break;
			case 1: rgb = glm::vec3(x, c, 0); break;
			case 2: rgb = glm::vec3(0, c, x); break;
			case 3: rgb = glm::vec3(0, x, c); break;
			case 4: rgb = glm::vec3(x, 0, c); break;
			case 5: rgb = glm::vec3(c, 0, x); break;
		}
		
		return rgb + glm::vec3(v - c);
	}

	class hsv_to_rgb_color_gen_t : public hsv_color_gen_t {
	public:
		using base_t = hsv_color_gen_t;

		hsv_to_rgb_color_gen_t(seed_t seed) : base_t(seed) {}

		glm::vec3 gen() {
			return hsv_to_rgb(base_t::gen());
		}
	};


	void sync_attractor_components(entity_t entity) {
		auto* attractor = entity.get_component<attractor_t>();
		if (!attractor) {
			std::cerr << "missing attractor component. handle " << entity.get_handle() << std::endl;
			return;
		}

		auto* transform = entity.get_component<transform_t>();
		if (!transform) {
			std::cerr << "missing transform component. handle " << entity.get_handle() << std::endl;
			return;
		}

		auto* physics = entity.get_component<physics_t>();

		transform->translation = attractor->pos;
		if (physics) {
			physics->pos = attractor->pos;
			physics->vel = glm::vec3{0.0f};
		}
	}

	void sync_transform_physics(entity_t entity) {
		auto* transform = entity.get_component<transform_t>();
		if (!transform) {
			std::cerr << "missing transform component. handle " << entity.get_handle() << std::endl;
			return;
		}

		auto* physics = entity.get_component<physics_t>();
		if (!physics) {
			std::cerr << "missing physics component. handle " << entity.get_handle() << std::endl;
			return;
		}

		transform->translation = physics->pos;
	}

	struct level_system_info_t {
		int balls_count{};
	};

	class level_system_t : public system_if_t {
		void scatter_balls(int start = -1, int end = -1) {
			start = start != -1 ? start : 0;
			end = end != -1 ? end : ball_handles.size();

			auto* ctx = get_ctx();

			float_gen_t coord_gen(42, -30.0f, +30.0f);
			float_gen_t vel_gen(42, -1.0f, +1.0f);
			for (int i = start; i < end; i++) {
				auto handle = ball_handles[i];

				float rx = coord_gen.gen(), ry = coord_gen.gen(), rz = coord_gen.gen();
				float vx = vel_gen.gen(), vy = vel_gen.gen(), vz = vel_gen.gen();

				entity_t ball{ctx, handle};
				auto* physics = ball.get_component<physics_t>();
				physics->pos = glm::vec3{rx, ry, rz};
				physics->vel = glm::vec3{vx, vy, vz};

			}
		}

		void spawn_balls(int count) {
			float r = 0.2f;

			transform_t transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(r),
				.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
				.translation = glm::vec3(0.0f) 
			};

			physics_t physics = {
				.pos = glm::vec3(0.0f, 0.0f, 0.0f),
				.vel = glm::vec3(0.0f, 0.0f, 0.0f),
				.mass = 1.0f,
				.radius = r,
			};

			basic_material_t material = {
				.color = glm::vec3(1.0f, 1.0f, 1.0f),
				.specular_strength = 1.0f,
				.shininess = 64.0f,
			};

			hsv_to_rgb_color_gen_t color_gen(42);

			for (int i = 0; i < count; i++) {
				material.color = color_gen.gen();

				entity_t object = entity_t(get_ctx());
				object.add_component(physics);
				object.add_component(transform);
				object.add_component(material);
				object.add_component(sync_component_t{sync_transform_physics});

				ball_handles.push_back(object.incweakref().get_handle());
			}
		}

		void create_balls() {
			spawn_balls(balls_count);
			scatter_balls(0, balls_count);
		}

		void create_attractors() {
			transform_t transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(0.5f),
				.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
				.translation = glm::vec3(0.0f, 0.0f, 0.0f),
			};

			physics_t physics = {
				.pos = glm::vec3(0.0f, 0.0f, 0.0f),
				.vel = glm::vec3(0.0f, 0.0f, 0.0f),
				.mass = 1.0f,
				.radius = 5.0f,
				.no_update = true,
			};

			attractor_t attractor = {
				.pos = glm::vec3(0.0f),
				.gm = 500.0f,
				.min_dist = 5.0f,
				.max_dist = 200.0f,
				.drag_min_coef = 0.0f,
				.drag_max_coef = 0.0f,
				.drag_min_dist = 2.0f,
				.drag_max_dist = 7.0f,
			};

			basic_material_t material = {
				.color = glm::vec3(1.0f, 0.0f, 0.0f),
				.specular_strength = 1.0f,
				.shininess = 64.0f,
				.vao = get_ctx()->get_resource_ref<vertex_array_t>("sphere")
			};

			entity_t object = entity_t(get_ctx());
			object.add_component(transform);
			object.add_component(attractor);
			object.add_component(material);
			object.add_component(physics);
			object.add_component(sync_component_t{sync_attractor_components});

			attractor_handle = object.incweakref().get_handle();
		}

		void create_light() {
			omnidir_light_t light = {
				.ambient = glm::vec3(0.2f),
				.color = glm::vec3(10.0f),
				.pos = glm::vec3(10.0f),
			};

			entity_t object(get_ctx());
			object.add_component(light);

			light_handle = object.incweakref().get_handle();
		}

		void create_viewer() {
			camera_t camera = {
				.fov = glm::radians(60.0f),
				.near = 1.0f,
				.far = 200.0f,
				.eye = glm::vec3(0.0f, 0.0, 13.0),
				.center = glm::vec3(0.0f, 0.0f, 0.0f),
				.up = glm::vec3(0.0f, 1.0f, 0.0f)
			};

			entity_t object(get_ctx());
			object.add_component(camera);

			viewer_handle = object.incweakref().get_handle();
		} 

		void create_basic_pass() {
			auto* ctx = get_ctx();

			auto color = ctx->get_resource<texture_t>("color");
			auto depth = ctx->get_resource<texture_t>("depth");
			auto framebuffer = std::make_shared<framebuffer_t>(framebuffer_t::create());
			framebuffer->attach(framebuffer_attachment_t{Color0, color->id});
			framebuffer->attach(framebuffer_attachment_t{Depth, depth->id});

			ctx->add_resource<framebuffer_t>("frame", framebuffer);

			entity_t viewer(ctx, viewer_handle);
			entity_t light(ctx, light_handle);
			entity_t basic_pass(ctx);
			basic_pass.add_component<basic_pass_t>(
				framebuffer, viewport_t{0, 0, color->width, color->height},
				glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
				viewer.incweakref(), light.incweakref()
			);

			basic_pass_handle = basic_pass.incref().get_handle();
		}

		void create_imgui_pass() {
			auto* ctx = get_ctx();

			auto& window = ctx->get_system<window_system_t>("window")->get_window();
			auto [width, height] = window.get_framebuffer_size();

			auto framebuffer = std::make_shared<framebuffer_t>();
			ctx->add_resource<framebuffer_t>("default_frame", framebuffer);

			entity_t imgui_pass(ctx);
			imgui_pass.add_component(imgui_pass_t{
				.framebuffer = framebuffer,
				.viewport = {0, 0, width, height},
				.clear_color = glm::vec4(1.0f, 0.5f, 0.25f, 1.0f),
				.clear_depth = 1.0f,
			});

			imgui_pass_handle = imgui_pass.incref().get_handle();
		}

		void create_passes() {
			create_basic_pass();
			create_imgui_pass();
		}

		bool framebuffer_gui() {
			if (ImGui::Begin("level: framebuffer")) {
				imgui_texture(to_tex_id(framebuffer_texture->id), ImVec2{(float)framebuffer_texture->width, (float)framebuffer_texture->height});
			}
			ImGui::End();
			return true;
		}

		bool level_control_gui() {
			if (ImGui::Begin("level: controls")) {
				ImGui::Text("balls: %d", (int)ball_handles.size());
				if (ImGui::Button("scatter")) {
					scatter_balls();
				}
				ImGui::SetNextItemWidth(100.0f);
				ImGui::InputInt("##balls_to_spawn", &balls_count_gui);
				ImGui::SameLine();
				if (ImGui::Button("spawn balls")) {
					int start = ball_handles.size();
					spawn_balls(balls_count_gui);
					scatter_balls(start);
				}
			}
			ImGui::End();
			return true;
		}

		void create_shitty_gui() {
			auto* ctx = get_ctx();
			framebuffer_texture = ctx->get_resource<texture_t>("color");

			if (auto* imgui_system = ctx->get_system<imgui_system_t>("imgui")) {
				imgui_system->register_callback([&](){
					return framebuffer_gui();
				});
				imgui_system->register_callback([&](){
					return level_control_gui();
				});
			}
		}

	public:
		level_system_t(engine_ctx_t* ctx, const level_system_info_t& info)
			: system_if_t(ctx)
			, balls_count{info.balls_count} {
			create_balls();
			create_attractors();
			create_light();
			create_viewer();
			create_passes();
			create_shitty_gui();
		}

		~level_system_t() {
			auto* ctx = get_ctx();
			for (auto& ball : ball_handles) {
				ctx->release_weak(ball);
			}
			ctx->release_weak(attractor_handle);
			ctx->release_weak(light_handle);
			ctx->release_weak(viewer_handle);

			ctx->release(basic_pass_handle);
			ctx->release(imgui_pass_handle);

			ctx->remove_resource<framebuffer_t>("frame");
			ctx->remove_resource<framebuffer_t>("default_frame");
		}

		void update() {
			float t = glfw::get_time() * 0.4f;
			entity_t viewer{get_ctx(), viewer_handle};
			auto* camera = viewer.get_component<camera_t>();
			camera->eye = glm::vec3(30.0f * std::cos(t), 10.0f * std::sin(t * 1.0f), 30.0f * std::sin(t));
		}

	private:
		std::vector<handle_t> ball_handles;
		handle_t attractor_handle{};
		handle_t light_handle{};
		handle_t viewer_handle{};
		handle_t basic_pass_handle{};
		handle_t imgui_pass_handle{};
		int balls_count{};
		int balls_count_gui{};

		resource_ptr_t<texture_t> framebuffer_texture{};
	};

	class mainloop_if_t {
	public:
		virtual ~mainloop_if_t() {}
		virtual void execute() = 0;
	};

	// TODO : I don't think I should store all this systems here, here is nice and sharp engine_ctx_t
	class mainloop_t : public mainloop_if_t {
	public:
		mainloop_t(engine_ctx_t* _ctx) : ctx{_ctx} {
			constexpr int window_width = 1600;
			constexpr int window_height = 800;
			constexpr int tex_width = 1280;
			constexpr int tex_height = 720;

			physics_system_info_t physics_system_info{
				.eps = 1e-6f,
				.overlap_coef = 1.0f,
				.overlap_resolution_iters = 1,
				.movement_limit = 300.0f,
				.velocity_limit = 300.0f,
				.impact_cor = 0.8f,
				.impact_v_loss = 0.99f,
				.dt_split = 4,
			};

			level_system_info_t level_info{
				.balls_count = 10000,
			};

			window_system = std::make_shared<window_system_t>(ctx, window_width, window_height);
			ctx->add_system("window", window_system);

			imgui_system = std::make_shared<imgui_system_t>(ctx, window_system->get_window().get_handle(), "#version 460 core");
			ctx->add_system("imgui", imgui_system);

			basic_resources_system = std::make_shared<basic_resources_system_t>(ctx, tex_width, tex_height);
			ctx->add_system("basic_resources", basic_resources_system);

			basic_renderer_system = std::make_shared<basic_renderer_system_t>(ctx);
			ctx->add_system("basic_renderer", basic_renderer_system);

			physics_system = std::make_shared<physics_system_t>(ctx, physics_system_info);
			ctx->add_system("physics", physics_system);

			timer_system = std::make_shared<timer_system_t>(ctx);
			ctx->add_system("timer", timer_system);

			level_system = std::make_shared<level_system_t>(ctx, level_info);
			ctx->add_system("level", level_system);

			sync_component_system = std::make_shared<sync_component_system_t>(ctx);
			ctx->add_system("sync_component", sync_component_system);
		}

		~mainloop_t() {
			// TODO : now there is no way to provide clean destruction of resources
			// but we can create system that will clear all dependent resources from engine_ctx
			// it can be basic_resources_system here, for example
			// many resources (like textures) depend on corresponding systems so this dependency management is something to be considered
			// for now this is fine, we got a bunch of workarounds so it must work
			ctx->remove_system("sync_component");
			sync_component_system.reset();

			ctx->remove_system("level");
			level_system.reset();

			ctx->remove_system("timer");
			timer_system.reset();

			ctx->remove_system("physics");
			physics_system.reset();

			ctx->remove_system("basic_renderer");
			basic_renderer_system.reset();

			ctx->remove_system("basic_resources");
			basic_resources_system.reset();

			ctx->remove_system("imgui");
			imgui_system.reset();

			ctx->remove_system("window");
			window_system.reset();
		}

		virtual void execute() override {
			auto& window = window_system->get_window();
			auto& physics = *physics_system;
			auto& basic_renderer = *basic_renderer_system;
			auto& imgui = *imgui_system;
			auto& sync = *sync_component_system;
			auto& timer = *timer_system;
			auto& level = *level_system;

			float dt = 0.008f;

			glfwSwapInterval(0);
			while (!window.should_close()) {
				window.swap_buffers();

				glfw::poll_events();

				timer.update(dt);
				physics.update(dt);
				basic_renderer.update();
				imgui.update();
				level.update();
				sync.update();
			}
		}

	private:
		engine_ctx_t* ctx{};
		std::shared_ptr<window_system_t> window_system;
		std::shared_ptr<basic_resources_system_t> basic_resources_system;
		std::shared_ptr<basic_renderer_system_t> basic_renderer_system;
		std::shared_ptr<imgui_system_t> imgui_system;
		std::shared_ptr<physics_system_t> physics_system;
		std::shared_ptr<timer_system_t> timer_system;
		std::shared_ptr<level_system_t> level_system;
		std::shared_ptr<sync_component_system_t> sync_component_system;
	};

	// test class
	class engine_t {
	public:
		engine_t() {
			ctx = std::make_unique<engine_ctx_t>();
			mainloop = std::make_unique<mainloop_t>(ctx.get());
		}

		void execute() {
			mainloop->execute();
		}

	private:
		std::unique_ptr<engine_ctx_t> ctx;
		std::unique_ptr<mainloop_if_t> mainloop;
	};

	// TODO : component iterate method
	// TODO : replace hashtable with vector like in entt
	// TODO : simplify hash function
	// TODO : test lofi_hashtable
	// TODO : thread pool
	// TODO : job
	// TODO : mutltithreaded physics update
	// TODO : multithreaded render data submission
	// TODO : script_system
	// TODO : simple timer system
	// TODO : ball_spawner_system_t
	
	///...

	// TODO : point of no return - next iteration
}


struct int_iter_t {
	int operator* () const {
		return value;
	}

	bool operator== (int_iter_t iter) const {
		return value == iter.value;
	}

	int_iter_t& operator++ () {
		return value++, * this;
	}

	int_iter_t operator++ (int) {
		return int_iter_t{value++};
	}

	int_iter_t& operator-- () {
		return value--, * this;
	}

	int_iter_t operator-- (int) {
		return int_iter_t{value--};
	}

	int value{};
};

struct int_iter_range_t {
	int_iter_t begin() const { return {range_start}; }
	int_iter_t end() const { return {range_end}; }

	int range_start{};
	int range_end{};
};


// some pile of shit (tests)
void assert_check(bool value, std::string_view error) {
	if (!value) {
		std::cerr << error << "\n";
		std::abort();
	}
}

void assert_false(bool value, std::string_view error) {
	assert_check(!value, error);
}

void assert_true(bool value, std::string_view error) {
	assert_check(value, error);
}

void test_octotree_stuff() {
	aabb_t box{glm::vec3{0.0f}, glm::vec3{3.0f}};

	// box-box
	aabb_t box1{glm::vec3{-1.0, 1.0f, 1.0f}, glm::vec3{4.0f, 2.0f, 2.0f}};
	aabb_t box2{glm::vec3{1.0f, -1.0f, 1.0f}, glm::vec3{2.0f, 4.0f, 2.0f}};
	aabb_t box3{glm::vec3{1.0f, 1.0f, -1.0f}, glm::vec3{2.0f, 2.0f, 4.0f}};
	aabb_t box4{glm::vec3{4.0f}, glm::vec3{5.0f}};
	aabb_t box5{glm::vec3{-2.0f}, glm::vec3{-1.0f}};
	aabb_t box6{glm::vec3{3.0f}, glm::vec3{4.0f}};

	assert_check(test_intersection_aabb_aabb(box, box), "box-box: must intersect");
	assert_check(test_intersection_aabb_aabb(box, box1), "box-box: must intersect");
	assert_check(test_intersection_aabb_aabb(box, box2), "box-box: must intersect");
	assert_check(test_intersection_aabb_aabb(box, box3), "box-box: must intersect");

	assert_check(!test_intersection_aabb_aabb(box, box4), "box-box: must not intersect");
	assert_check(!test_intersection_aabb_aabb(box, box5), "box-box: must not intersect");

	assert_check(test_intersection_aabb_aabb(box, box6), "box-box: must intersect");

	// box-point
	glm::vec3 p1{4.0f};
	glm::vec3 p2{-1.0f};
	glm::vec3 p3{1.0f, 1.0f, 4.0f};
	glm::vec3 p4{1.0f, 4.0f, 1.0f};
	glm::vec3 p5{4.0f, 1.0f, 1.0f};
	glm::vec3 p6{1.0f};
	glm::vec3 p7{3.0f};

	assert_check(!test_intersection_aabb_point(box, p1), "box-point: must not intersect");
	assert_check(!test_intersection_aabb_point(box, p2), "box-point: must not intersect");
	assert_check(!test_intersection_aabb_point(box, p3), "box-point: must not intersect");
	assert_check(!test_intersection_aabb_point(box, p4), "box-point: must not intersect");
	assert_check(!test_intersection_aabb_point(box, p5), "box-point: must not intersect");
	assert_check(test_intersection_aabb_point(box, p6), "box-point: must intersect");
	assert_check(test_intersection_aabb_point(box, p7), "box-point: must intersect");

	std::cout << "octotree tests passed\n";
}

void test_sparse_grid_hash() {
	auto hash = sparse_cell_hasher_t{};

	std::unordered_map<std::uint32_t, std::uint32_t> hash_count;
	int_gen_t x(41, -100000000, 100000000);
	int_gen_t y(42, -100000000, 100000000);
	int_gen_t z(43, -100000000, 100000000);
	for (int i = 0; i < 100000000; i++) {
		auto h = hash(sparse_cell_t{x.gen(), y.gen(), z.gen()});
		// h = ((h >> 9) + h) & ((1 << 23) - 1);
		// h = ((h >> 10) + h) & ((1 << 22) - 1);
		h %= 200000081;
		hash_count[h]++;
	}

	std::uint32_t min_count = ~0;
	std::uint32_t max_count = 0;
	for (auto& [hash, count] : hash_count) {
		min_count = std::min(min_count, count);
		max_count = std::max(max_count, count);
	}

	std::unordered_map<std::uint32_t, std::uint32_t> hash_count_count;
	for (auto& [hash, count] : hash_count) {
		hash_count_count[count]++;
	}

	std::cout << "hashes: " << hash_count.size() << " min_count: " << min_count << " max_count: " << max_count << "\n";
	std::cout << "unique_counts: " << hash_count_count.size() << "\n";
}

void test_sparse_grid() {
	auto print_data = [&] (const sparse_grid_t<int>& grid, int head) {
		for (sparse_data_iterator_t iter{grid, head}; iter.valid(); iter.next()) {
			std::cout << iter.get().data << " ";
		}
		};

	sparse_grid_t<int> grid(2.0f, -100000, 100000);
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 4; k++) {
				grid.add(glm::vec3{i, j, k}, i * 16 + j * 4 + k);
			}
		}
	}

	for (auto& [cell, info] : grid.get_cell_storage()) {
		std::cout << "cell: " << cell.x << ":" << cell.y << ":" << cell.z << " count: " << info.count << "\n\t";
		print_data(grid, info.head);
		std::cout << "\n";
	}

	auto print_query = [&] (const sparse_grid_t<int>& grid, const glm::vec3& min, const glm::vec3& max, const std::vector<sparse_query_result_t>& result) {
		std::cout << "min: " << min.x << ":" << min.y << ":" << min.z
			<< " max: " << max.x << ":" << max.y << ":" << max.z
			<< " result size: " << result.size() << "\n";
		for (auto& [cell, head, count] : result) {
			std::cout << "cell: " << cell.x << ":" << cell.y << ":" << cell.z << " count: " << count << "\n\t";
			print_data(grid, head);
			std::cout << "\n";
		}
		std::cout << "\n";
		};

	std::vector<sparse_query_result_t> result;

	for (int i = 0; i < 8; i++) {
		glm::vec3 min{(i & 1) * 2, ((i >> 1) & 1) * 2, ((i >> 2) & 1) * 2};
		glm::vec3 max = min + glm::vec3{1};
		grid.query(min, max, result);
		print_query(grid, min, max, result);
		result.clear();
	}

	grid.query(sparse_cell_t{-1}, sparse_cell_t{-1}, result);
	assert_check(result.empty(), "result is not empty");
}

void test_thread_pool1() {
	struct some_job_t : public job_if_t {
		some_job_t() = default;

		some_job_t(int _start, int _end)
			: start{_start}
			, end{_end}
		{}

		some_job_t(const some_job_t&) = delete;
		some_job_t& operator=(const some_job_t&) = delete;

		some_job_t(some_job_t&&) noexcept = delete;
		some_job_t& operator=(some_job_t&&) noexcept = delete;

		void execute() override {
			for (int i = start; i < end; i++) {
				result += i;
			}
		}

		int start{};
		int end{};
		std::int64_t result{};
	};

	thread_pool_t pool(24);
	std::vector<std::unique_ptr<some_job_t>> jobs;
	for (int i = 0; i < 24; i++) {
		jobs.push_back(std::make_unique<some_job_t>(0, 1 << 29));
	}

	for (auto& job : jobs) {
		pool.push_job(job.get());
	}

	for (auto& job : jobs) {
		job->wait();
	}

	for (int i = 0; i < 24; i++) {
		auto& job = *jobs[i];
		std::cout << "job " << i << " start: " << job.start << " end: " << job.end << " result: " << job.result << "\n";
	}
}

void test_thread_pool2() {
	struct job_t : public job_if_t {
		job_t(std::atomic<int>* _swapped_number, int _number, int _swaps)
			: swapped_number{_swapped_number}
			, number{_number}
			, swaps{_swaps}
		{}

		void execute() override {
			for (int i = 0; i < swaps; i++) {
				number = swapped_number->exchange(number, std::memory_order_relaxed);
			}
		}

		std::atomic<int>* swapped_number{};
		int number{};
		int swaps{};
	};

	thread_pool_t pool(24);
	std::atomic<int> swapped_number{24};
	std::vector<std::unique_ptr<job_t>> jobs;
	for (int i = 0; i < 24; i++) {
		jobs.push_back(std::make_unique<job_t>(&swapped_number, i, 1 << 20));
	}

	for (auto& job : jobs) {
		pool.push_job(job.get());
	}

	for (auto& job : jobs) {
		job->wait();
	}

	std::cout << "swapped_number: " << swapped_number.load(std::memory_order_relaxed) << "\n";
	for (int i = 0; i < 24; i++) {
		std::cout << "job " << i << ": " << jobs[i]->number << "\n";
	}
}

void test_callback() {
	struct some_struct_t {
		static void callback(some_struct_t* ctx, int num) {
			auto* this_ptr = (some_struct_t*)ctx;
			std::cout << this_ptr->term + num << "\n";
		}

		int term{};
	};

	some_struct_t whatever{5};
	callback_t<void(int)> cb{whatever, some_struct_t::callback};
	cb(5);
}

struct job_range_t {
	int start{};
	int stop{};
};

static job_range_t compute_job_range(int job_size, int job_count, int job_id) {
	int job_part = (job_size + job_count - 1) / job_count;
	int job_start = std::min(job_id * job_part, job_size);
	int job_stop = std::min(job_start + job_part, job_size);
	return {job_start, job_stop};
}

template<class type_t>
struct data_range_t {
	type_t* start{};
	type_t* stop{};
};

template<class type_t>
static data_range_t<type_t> compute_data_range(type_t* data, int job_size, int job_count, int job_id) {
	auto [start, stop] = compute_job_range(job_size, job_count, job_id);
	return {data + start, data + stop};
}

template<class type_t>
void shuffle_vec(std::vector<type_t>& vec, seed_t seed = 0) {
	if (vec.empty()) {
		return;
	}

	basic_int_gen_t gen(seed);
	for (int i = vec.size() - 1; i > 0; i--) {
		std::swap(vec[i], vec[gen.gen() % i]);
	}
}

void test_lofi_hashtable() {
	struct ctx_t {
		using std_hashtable_t = std::unordered_multiset<sparse_cell_t, sparse_cell_hasher_t, sparse_cell_equals_t>;

		struct job_t : public job_if_t {
			job_t(ctx_t* _ctx, int _job_id)
				: ctx{_ctx}
				, job_id{_job_id}
			{}

			void execute() override {
				ctx->worker(this);
			}

			ctx_t* ctx{};
			int job_id{};
			int max_scans{};
			int total_scans{};
		};

		enum process_stage_t {
			HashtableReset,
			PrepareHashtable,
			BuildHashtable,
			BuildStdHashtable,
		};

		static std::uint32_t hash(ctx_t* ctx, int cell) {
			return sparse_cell_hasher_t{}(ctx->cells[cell]);
		}

		static bool equals(ctx_t* ctx, int cell1, int cell2) {
			return ctx->cells[cell1] == ctx->cells[cell2];
		}

		ctx_t(int _cell_count, int _repeat, int _job_count, bool should_shuffle = false)
			: cell_count{_cell_count}
			, repeat{_repeat}
			, job_count{_job_count}
			, total_cell_count{cell_count * repeat}
			, hashtable{total_cell_count, cell_count, lofi_hasher_t{this, hash}, lofi_equals_t{this, equals}}
			, used_buckets{total_cell_count}
			, thread_pool{job_count} {

			int_gen_t x_gen(561, -40000, 50000);
			int_gen_t y_gen(1442, -40000, 50000);
			int_gen_t z_gen(105001, -40000, 50000);

			cells.reserve(total_cell_count);
			for (int i = 0; i < cell_count; i++) {
				int x = x_gen.gen();
				int y = y_gen.gen();
				int z = z_gen.gen();
				for (int k = 0; k < repeat; k++) {
					cells.push_back(sparse_cell_t{x, y, z});
				}
			}

			if (should_shuffle) {
				shuffle_vec(cells, 666);
			}

			jobs.reserve(job_count);
			for (int i = 0; i < job_count; i++) {
				jobs.push_back(std::make_unique<job_t>(this, i));
			}

			std_hashtable.reserve(total_cell_count);
		}

		int master() {
			auto t1 = std::chrono::high_resolution_clock::now();

			hashtable.reset_size(total_cell_count, cell_count);
			used_buckets.reset();

			stage = process_stage_t::PrepareHashtable;
			dispatch_jobs();

			auto t2 = std::chrono::high_resolution_clock::now();

			stage = process_stage_t::BuildHashtable;
			dispatch_jobs();

			auto t3 = std::chrono::high_resolution_clock::now();

			stage = process_stage_t::BuildStdHashtable;
			dispatch_jobs();

			auto t4 = std::chrono::high_resolution_clock::now();


			using microseconds_t = std::chrono::duration<long long, std::micro>;

			auto dt21 = std::chrono::duration_cast<microseconds_t>(t2 - t1).count();
			auto dt32 = std::chrono::duration_cast<microseconds_t>(t3 - t2).count();
			auto dt43 = std::chrono::duration_cast<microseconds_t>(t4 - t3).count();

			std::cout << " ---=== in microseconds ===---" << "\n";
			std::cout << "lofi: " << dt32 + dt21 << " = " << dt32 << " + " << dt21 << "\n";
			std::cout << "std: " << dt43 << "\n";

			int max_count = 0;
			int inserted = 0;
			for (int i = 0; i < hashtable.get_buckets_count(); i++) {
				int count = hashtable.get_bucket(i).count.load(std::memory_order_relaxed);
				max_count = std::max(max_count, count);
				inserted += count;
			}

			int max_scans = 0;
			int total_scans = 0;
			for (auto& job : jobs) {
				max_scans = std::max(max_scans, job->max_scans);
				total_scans += job->total_scans;
			}

			std::cout << "max count: " << max_count << "\n";
			std::cout << "max scans: " << max_scans << "\n";
			std::cout << "total scans: " << total_scans << "\n";
			std::cout << "avg scans: " << (double)total_scans / total_cell_count << "\n";
			std::cout << "inserted: " << inserted << "\n";
			std::cout << "used buckets: " << used_buckets.get_size() << "\n";
			std::cout << "hashtable valid: " << check_lofi_hashtable() << "\n";

			return dt32 + dt21;
		}

		bool check_lofi_hashtable() {
			int added_count = 0;
			std::vector<bool> cell_added(total_cell_count);

			for (int i = 0, count = used_buckets.get_size(); i < count; i++) {
				auto& bucket = hashtable.get_bucket(used_buckets.data[i]);

				int curr = bucket.head.load(std::memory_order_relaxed);
				if (curr == -1) {
					return false;
				}
				while (true) {
					if (cell_added[curr]) {
						return false;
					}
					cell_added[curr] = true;
					added_count++;

					int next = hashtable.list_entries[curr].next;
					if (next == curr) {
						break;
					}
					curr = next;
				}
			}

			return added_count == total_cell_count;
		}

		void worker(job_t* job) {
			switch (stage) {
				// prepare
				case PrepareHashtable: {
					auto [start, stop] = compute_data_range(hashtable.get_buckets_data(), hashtable.get_buckets_count(), job_count, job->job_id);
					while (start != stop) {
						start->reset();
						start++;
					}
					break;
				}

				// put
				case BuildHashtable: {
					constexpr int batch_size = 128;

					int buckets[batch_size] = {};
					int buckets_count = 0;
					int max_scans = 0;
					int total_scans = 0;

					auto [start, stop] = compute_job_range(hashtable.get_object_count(), job_count, job->job_id);
					while (start != stop) {
						buckets_count = 0;

						int next_stop = std::min(stop, start + batch_size);
						while (start != next_stop) {
							auto [bucket, scans] = hashtable.put(start);
							if (bucket != -1) {
								buckets[buckets_count++] = bucket;
							}
							max_scans = std::max(max_scans, scans);
							total_scans += scans;
							start++;
						}

						if (buckets_count > 0) {
							int* used_buckets_mem = used_buckets.push(buckets_count);
							assert(used_buckets_mem);
							std::memcpy(used_buckets_mem, buckets, sizeof(buckets[0]) * buckets_count);
						}
					}

					job->max_scans = max_scans;
					job->total_scans = total_scans;
					break;
				}

				// std
				case BuildStdHashtable: {
					if (job->job_id == -1) {
						auto [start, stop] = compute_job_range(hashtable.get_object_count(), 1, 0);
						while (start != stop) {
							std_hashtable.insert(cells[start++]);
						}
					}
					break;
				}
			}
		}

		void dispatch_jobs() {
			for (auto& job : jobs) {
				thread_pool.push_job(job.get());
			}
			for (auto& job : jobs) {
				job->wait();
			}
		}

		int cell_count{};
		int repeat{};
		int job_count{};
		int total_cell_count{};

		std::vector<sparse_cell_t> cells{};
		lofi_hashtable1_t hashtable;
		lofi_stack_t<int> used_buckets;

		thread_pool_t thread_pool;
		std::vector<std::unique_ptr<job_t>> jobs;
		process_stage_t stage{};

		std_hashtable_t std_hashtable;
	};

	ctx_t ctx{1 << 17, 1 << 5, 24};

	std::vector<int> dts;
	for (int i = 0; i < 20; i++) {
		dts.push_back(ctx.master());
	}
	for (auto& dt : dts) {
		std::cout << dt << " ";
	}
	std::cout << "\n";
}



int main() {
	//test_octotree_stuff();
	//test_sparse_grid_hash();
	//test_sparse_grid();
	//test_thread_pool1();
	//test_thread_pool2();
	test_lofi_hashtable();
	//test_callback();
	//engine_t engine;
	//engine.execute();
	return 0;
}