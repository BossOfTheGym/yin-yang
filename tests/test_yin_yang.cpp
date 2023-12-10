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

#include <ecs.hpp>
#include <glfw.hpp>
#include <lofi.hpp>
#include <utils.hpp>
#include <dt_timer.hpp>
#include <sparse_cell.hpp>
#include <thread_pool.hpp>

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

#include <nlohmann/json.hpp>

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
			<< " location: " << props.location << "\n";
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
	vec4 pos = m[gl_InstanceID] * vec4(attr_pos, 1.0);
	world_pos = pos.xyz;
	object_id = gl_InstanceID;
	gl_Position = u_p * u_v * pos;
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
		gpu_buffer_t(std::uint64_t _size, bool hardcore = true)
			: size{_size}
			, im_insane{hardcore} {
			glCreateBuffers(1, &buffer_id);

			GLbitfield buffer_flags = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;
			glNamedBufferStorage(buffer_id, size, nullptr, buffer_flags);
			
			GLbitfield mapping_flags = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT/*(hardcore ? GL_MAP_FLUSH_EXPLICIT_BIT : 0)*/;
			mapped_pointer = glMapNamedBufferRange(buffer_id, 0, size, mapping_flags);
		}

		~gpu_buffer_t() {
			glUnmapNamedBuffer(buffer_id);
			glDeleteBuffers(1, &buffer_id);
		}

		void flush(std::uint64_t offset, std::uint64_t size) {
			/*if (im_insane) {
				glFlushMappedNamedBufferRange(buffer_id, offset, size);
			}*/
		}

		GLuint buffer_id{};
		std::uint64_t size{};
		bool im_insane{};
		void* mapped_pointer{};
	};

	struct gpu_fence_sync_t {
		enum {
			None = 0,
			SyncFlushCommands = GL_SYNC_FLUSH_COMMANDS_BIT,
		};

		~gpu_fence_sync_t() {
			if (id) {
				glDeleteSync(id);
			}
		}

		void wait(int flags) {
			if (id) {
				GLenum result = glClientWaitSync(id, flags, 0);
				if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
					glDeleteSync(id);
					id = 0;
				}
				if (result == GL_TIMEOUT_EXPIRED) {
					std::cout << "expired" << "\n";
					glDeleteSync(id);
					id = 0;
				}
				if (result == GL_WAIT_FAILED) {
					std::abort();
				}
			}
		}

		void sync() {
			assert(!id);
			id = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}

		GLsync id{};
	};

	template<class resource_t>
	using resource_ptr_t = std::shared_ptr<resource_t>;

	template<class resource_t>
	using resource_ref_t = std::weak_ptr<resource_t>;

	class resource_registry_t {
		struct resource_tag_t;

		template<class resource_t>
		static std::size_t uid() { return type_id_t<resource_tag_t>::template get<resource_t>(); }

		class resource_entry_if_t {
		public:
			virtual ~resource_entry_if_t() {}
		};

		using resource_storage_t = if_placeholder_t<resource_entry_if_t>;

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

			std::size_t res_uid = uid<resource_t>();
			if (res_uid >= entries.size()) {
				entries.resize(res_uid + 1);
				entries[res_uid] = resource_storage_t::create<entry_t>();
			}
			return *entries[res_uid].template get<entry_t>();
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
		std::vector<resource_storage_t> entries;
	};

	template<class tag_t, class ... component_t>
	struct component_storage_desc_t {};

	class component_registry_t {
		struct component_tag_t;

		class entry_if_t {
		public:
			virtual ~entry_if_t() {}
			virtual void remove(handle_t handle) = 0;
			virtual void clear() = 0;
		};

		using entry_placeholder_t = if_placeholder_t<entry_if_t>;

		template<class tag_t, class ... component_t>
		class entry_t : public sparse_storage_t<component_t...>, public entry_if_t {
		public:
			using storage_base_t = sparse_storage_t<component_t...>;

			virtual void remove(handle_t handle) override {
				storage_base_t::erase(handle);
			}

			virtual void clear() override {
				storage_base_t::clear();
			}
		};

		template<class tag_t, class ... component_t>
		static std::size_t uid() { return type_id_t<component_tag_t>::template get<tag_t, component_t...>(); }

		template<class tag_t, class ... component_t>
		auto& acquire_entry() {
			std::size_t index = uid<tag_t, component_t...>();
			if (index >= entries.size()) {
				entries.resize(index + 1);
				entries[index] = entry_placeholder_t::create<entry_t<tag_t, component_t...>>();
			}
			return *entries[index].template get<entry_t<tag_t, component_t...>>();
		}

	public:
		void release(handle_t handle) {
			for (auto& entry : entries) {
				entry.get_if()->remove(handle);
			}
		}

		template<class tag_t, class ... component_t>
		auto emplace(component_storage_desc_t<tag_t, component_t...>, handle_t handle) {
			return acquire_entry<tag_t, component_t...>().emplace(handle);
		}

		template<class ... component_init_t, class tag_t, class ... component_t>
		auto emplace(component_storage_desc_t<tag_t, component_t...>, handle_t handle, component_init_t&& ... init) {
			return acquire_entry<tag_t, component_t...>().emplace(handle, std::forward<component_init_t>(init)...);
		}

		template<class tag_t, class ... component_t>
		void erase(component_storage_desc_t<tag_t, component_t...>, handle_t handle) {
			acquire_entry<tag_t, component_t...>().erase(handle);
		}

		template<class tag_t, class ... component_t>
		auto get(component_storage_desc_t<tag_t, component_t...>, handle_t handle) {
			return acquire_entry<tag_t, component_t...>().get(handle);
		}

		template<class tag_t, class ... component_t>
		auto const_get(component_storage_desc_t<tag_t, component_t...>, handle_t handle) {
			return acquire_entry<tag_t, component_t...>().const_get(handle);
		}

		template<class tag_t, class ... component_t>
		auto view(component_storage_desc_t<tag_t, component_t...>) {
			return acquire_entry<tag_t, component_t...>().view();
		}

		template<class tag_t, class ... component_t>
		auto const_view(component_storage_desc_t<tag_t, component_t...>) {
			return acquire_entry<tag_t, component_t...>().const_view();
		}

		template<class tag_t, class ... component_t>
		auto handles_view(component_storage_desc_t<tag_t, component_t...>) {
			return acquire_entry<tag_t, component_t...>().handles_view();
		}

		template<class tag_t, class ... component_t>
		void clear(component_storage_desc_t<tag_t, component_t...>) {
			acquire_entry<component_t>().clear();
		}

		void clear() {
			for (auto& entry : entries) {
				entry.get_if()->clear();
			}
		}

	private:
		std::vector<entry_placeholder_t> entries;
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
		using system_storage_t = if_placeholder_t<system_if_t>;

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
	class engine_ctx_t {
	public:
		~engine_ctx_t() {
			clear();
		}

		void clear() {
			component_registry.clear();
			resource_registry.clear();
			system_registry.clear();
			handles.clear();
			refcount.clear();
			weakrefcount.clear();
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


		template<class system_t>
		bool add_system(const std::string& name, std::shared_ptr<system_t> sys) {
			return system_registry.add(name, std::move(sys));
		}

		bool remove_system(const std::string& name) {
			return system_registry.remove(name);
		}

		template<class system_t>
		system_t* get_system(const std::string& name) {
			if (system_t* sys = system_registry.get<system_t>(name)) {
				return sys;
			}
			return nullptr;
		}

		void clear_systems() {
			system_registry.clear();
		}



		void release_components(handle_t handle) {
			component_registry.release(handle);
		}

		template<class _component_storage_desc_t>
		auto emplace(handle_t handle) {
			return component_registry.emplace(_component_storage_desc_t{}, handle);
		}

		template<class _component_storage_desc_t, class ... component_init_t>
		auto emplace(handle_t handle, component_init_t&& ... init) {
			return component_registry.emplace(_component_storage_desc_t{}, handle, std::forward<component_init_t>(init)...);
		}

		template<class _component_storage_desc_t>
		void erase(handle_t handle) {
			component_registry.erase(_component_storage_desc_t{}, handle);
		}

		template<class _component_storage_desc_t>
		auto get(handle_t handle) {
			return component_registry.get(_component_storage_desc_t{}, handle);
		}

		template<class _component_storage_desc_t>
		auto const_get(handle_t handle) {
			return component_registry.const_get(_component_storage_desc_t{}, handle);
		}

		template<class _component_storage_desc_t>
		auto view() {
			return component_registry.view(_component_storage_desc_t{});
		}

		template<class _component_storage_desc_t>
		auto const_view() {
			return component_registry.const_view(_component_storage_desc_t{});
		}

		template<class _component_storage_desc_t>
		auto handles_view() {
			return component_registry.handles_view(_component_storage_desc_t{});
		}

		template<class _component_storage_desc_t>
		void clear_components() {
			component_registry.clear(_component_storage_desc_t{});
		}

		void clear_components() {
			component_registry.clear();
		}


		bool is_alive(handle_t handle) const {
			assert(handle != null_handle);
			return handles.is_used(handle) && refcount[handle] > 0;
		}

		bool is_alive_weak(handle_t handle) const {
			assert(handle != null_handle);
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
			if (handle == null_handle) {
				return handle;
			}

			assert(is_alive(handle));
			refcount[handle]++;
			return handle;
		}

		[[nodiscard]] handle_t incweakref(handle_t handle) {
			if (handle == null_handle) {
				return handle;
			}

			assert(is_alive_weak(handle));
			weakrefcount[handle]++;
			return handle;
		}

		void release(handle_t handle) {
			if (handle == null_handle) {
				return;
			}

			assert(is_alive(handle));
			int refs = --refcount[handle];
			if (refs == 0) {
				release_weak(handle);
				component_registry.release(handle);
			}
		}

		void release_weak(handle_t handle) {
			assert(is_alive_weak(handle));
			int refs = --weakrefcount[handle];
			if (refs == 0) {
				handles.release(handle);
			}
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

		void release_components() {
			ctx->release_components(handle);
		}

		template<class _component_storage_desc_t>
		auto emplace() {
			return ctx->emplace<_component_storage_desc_t>(handle);
		}

		template<class _component_storage_desc_t, class ... component_init_t>
		auto emplace(component_init_t&& ... init) {
			return ctx->emplace<_component_storage_desc_t>(handle, std::forward<component_init_t>(init)...);
		}

		template<class _component_storage_desc_t>
		void erase() {
			ctx->erase<_component_storage_desc_t>(handle);
		}

		template<class _component_storage_desc_t>
		auto get() {
			return ctx->get<_component_storage_desc_t>(handle);
		}

		template<class _component_storage_desc_t>
		auto const_get() {
			return ctx->const_get<_component_storage_desc_t>(handle);
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


	class thread_pool_system_t : public system_if_t, public thread_pool_t {
	public:
		thread_pool_system_t(engine_ctx_t* ctx, int thread_count)
			: system_if_t(ctx), thread_pool_t{thread_count} {}
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
			// TODO
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

	using sync_component_desc_t = component_storage_desc_t<void, sync_component_t>;

	class sync_component_system_t : public system_if_t {
	public:
		sync_component_system_t(engine_ctx_t* ctx) : system_if_t(ctx) {}

		void update() {
			auto* ctx = get_ctx();
			for (auto [handle, sync] : ctx->view<sync_component_desc_t>()) {
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

	using imgui_pass_desc_t = component_storage_desc_t<void, imgui_pass_t>;

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
			for (auto [handle, pass] : get_ctx()->view<imgui_pass_desc_t>()) {
				if (auto framebuffer = pass.framebuffer.lock()) {
					framebuffer->bind();
				} else {
					std::cerr << "framebuffer expired" << "\n";
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
					std::cerr << info_log << "\n";
					return std::make_shared<shader_program_t>();
				}
				return std::make_shared<shader_program_t>(std::move(program));
			})();

			auto basic_instanced_program_ptr = ([&] () {
				auto [program, info_log] = gen_basic_instanced_shader_program();
				if (!program.valid()) {
					std::cerr << info_log << "\n";
					return std::make_shared<shader_program_t>();
				}
				return std::make_shared<shader_program_t>(std::move(program));
			})();

			auto sphere_mesh_ptr = std::make_shared<mesh_t>(gen_sphere_mesh(1));
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
	};

	struct basic_pass_t {
		resource_ref_t<framebuffer_t> framebuffer;
		viewport_t viewport{};
		glm::vec4 clear_color{};
		float clear_depth{};
	};

	#define std430_vec3 alignas(sizeof(glm::vec4)) glm::vec3
	#define std430_vec4 alignas(sizeof(glm::vec4)) glm::vec4
	#define std430_float alignas(sizeof(float)) float

	struct offset_utility_t {
		offset_utility_t(std::uint64_t _offset, std::uint64_t _alignment)
			: offset{_offset}
			, alignment{_alignment} {
			assert(std::has_single_bit(alignment));
			offset = (offset + alignment - 1) & ~(alignment - 1);
		}

		std::uint64_t push(std::uint64_t amount) {
			std::uint64_t old_offset = offset;
			offset = (offset + amount + alignment - 1) & ~(alignment - 1);
			return old_offset;
		}

		std::uint64_t offset{};
		std::uint64_t alignment{};
	};

	void* advance_ptr(void* ptr, std::ptrdiff_t amount) {
		return (char*)ptr + amount;
	}

	using basic_renderer_pass_desc_t = component_storage_desc_t<void, basic_pass_t, camera_t, omnidir_light_t>;

	struct basic_renderer_settings_t {
		int max_instances{10000};
	};

	class basic_renderer_system_t : public system_if_t {
	public:
		struct std430_basic_material_t {
			std430_vec3 color{};
			std430_float shininess{};
			std430_float specular_strength{};
		};

		basic_renderer_system_t(engine_ctx_t* ctx, const basic_renderer_settings_t& settings)
			: system_if_t(ctx)
			, max_instances{settings.max_instances} {
			instanced_program = get_ctx()->get_resource<shader_program_t>("basic_instanced_program");
			sphere = get_ctx()->get_resource<vertex_array_t>("sphere");

			offset_utility_t offset_utility{0, 256}; // just in case

			mats_size = max_instances * sizeof(glm::mat4);
			mats_offset_prev = offset_utility.push(mats_size);
			mats_offset_next = offset_utility.push(mats_size);

			mtls_size = max_instances * sizeof(std430_basic_material_t);
			mtls_offset_prev = offset_utility.push(mtls_size);
			mtls_offset_next = offset_utility.push(mtls_size);

			instance_buffer = std::make_unique<gpu_buffer_t>(offset_utility.offset);

			auto* imgui = get_ctx()->get_system<imgui_system_t>("imgui");
			imgui->register_callback([&] () {
				ImGui::SetNextWindowSize(ImVec2{256, 256}, ImGuiCond_Once);
				if (ImGui::Begin("basic renderer")) {
					ImGui::Text("Hello");
				}
				ImGui::End();
				return true;
			});
		}

	private:
		void render_pass_instanced(basic_pass_t* pass, camera_t* camera, omnidir_light_t* light) {
			if (prev_frame_submitted <= 0) {
				return;
			}

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
				std::cerr << "framebuffer expired" << "\n";
				return;
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
			instanced_program->set_mat4("u_v", camera->get_view());
			instanced_program->set_mat4("u_p", camera->get_proj(viewport.get_aspect_ratio()));
			instanced_program->set_vec3("u_eye_pos", camera->eye);
			instanced_program->set_vec3("u_ambient_color", light->ambient);
			instanced_program->set_vec3("u_light_color", light->color);
			instanced_program->set_vec3("u_light_pos", light->pos);

			sphere->bind();
			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, instance_buffer->buffer_id, render_prev_buffer ? mats_offset_prev : mats_offset_next, prev_frame_submitted * sizeof(glm::mat4));
			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, instance_buffer->buffer_id, render_prev_buffer ? mtls_offset_prev : mtls_offset_next, prev_frame_submitted * sizeof(std430_basic_material_t));
			glDrawArraysInstanced(sphere->mode, 0, sphere->count, prev_frame_submitted);
		}

	public:
		void render_prev_frame() {
			for (auto [handle, pass, camera, light] : get_ctx()->view<basic_renderer_pass_desc_t>()) {
				render_pass_instanced(&pass, &camera, &light);
			}
		}

		struct submit_region_t {
			glm::mat4* mat{};
			std430_basic_material_t* mtl{};
			int count{};
			int global_start{};
		};

		submit_region_t submit_next_frame(int count) {
			int global_start = next_frame_submitted.fetch_add(count);
			int insts_count = std::min(max_instances, global_start + count) - global_start;
			int mat_offset = (render_prev_buffer ? mats_offset_next : mats_offset_prev) + global_start * sizeof(glm::mat4);
			int mtl_offset = (render_prev_buffer ? mtls_offset_next : mtls_offset_prev) + global_start * sizeof(std430_basic_material_t);
			return {
				(glm::mat4*)advance_ptr(instance_buffer->mapped_pointer, mat_offset),
				(std430_basic_material_t*)advance_ptr(instance_buffer->mapped_pointer, mtl_offset),
				insts_count,
				global_start
			};
		}

		void finish_next_frame() {
			int submitted = std::min(next_frame_submitted.load(std::memory_order_relaxed), max_instances);
			next_frame_submitted.store(0, std::memory_order_relaxed);

			instance_buffer->flush(render_prev_buffer ? mats_offset_next : mats_offset_prev, submitted * sizeof(glm::mat4));
			instance_buffer->flush(render_prev_buffer ? mtls_offset_next : mtls_offset_prev, submitted * sizeof(std430_basic_material_t));

			prev_frame_submitted = submitted;
			render_prev_buffer ^= 1;
		}

	private:
		int max_instances{};

		resource_ptr_t<shader_program_t> instanced_program;
		resource_ptr_t<vertex_array_t> sphere;

		gpu_fence_sync_t sync{};

		std::unique_ptr<gpu_buffer_t> instance_buffer;

		std::uint64_t mats_offset_prev{};
		std::uint64_t mats_offset_next{};
		std::uint64_t mats_size{};

		std::uint64_t mtls_offset_prev{};
		std::uint64_t mtls_offset_next{};
		std::uint64_t mtls_size{};

		std::atomic<int> next_frame_submitted{};
		int prev_frame_submitted{};
		int render_prev_buffer{};
	};


	class window_system_t : public system_if_t {
	public:
		static void gl_debug_callback(
			GLenum source,
			GLenum type,
			GLuint id,
			GLenum severity,
			GLsizei length,
			const GLchar* message,
			const void* userParam) {
			if (type == GL_DEBUG_TYPE_ERROR) {
				std::cerr << "gl err: " << message << "\n";
			}
		}

		window_system_t(engine_ctx_t* ctx, int window_width, int window_height) : system_if_t(ctx) {
			glfw_guard = std::make_unique<glfw::guard_t>();
			window = std::make_unique<glfw::window_t>(
				glfw::window_params_t::create_basic_opengl("yin-yang", window_width, window_height, 4, 6)
			);
			window->make_ctx_current();
			glew_guard = std::make_unique<glew_guard_t>();

			glEnable(GL_DEBUG_OUTPUT);
			glDebugMessageCallback(gl_debug_callback, nullptr);

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


	struct particle_t {
		glm::vec3 pos{};
		glm::vec3 vel{};
		int id{};
	};

	struct attractor_t {
		glm::vec3 pos{};
		float GM{};
	};

	struct physics_system_settings_t {
		float eps{};
		float dt_step{};
		int max_particles{};
		float grid_scale{};
		float particle_r{};
		float particle_repulse_coef{};
		int heavy_cell_thresh{};
		int updates_per_frame{};
	};

	// TODO : simd
	// TODO : reordering bring havok and mess into the code
	// TODO : rename to strange_particle_system
	class physics_system_t : public system_if_t {
	public:
		friend class sparse_grid_ops_t;

		struct sparse_grid_ops_t {
			sparse_cell_t cell(int id) const {
				return get_sparse_cell(ctx->get_particle(id).pos, ctx->grid_scale);
			}

			std::uint32_t hash(int id) const {
				return hash(cell(id));
			}

			std::uint32_t hash(const sparse_cell_t& cell) const {
				return sparse_cell_hasher_t{}(cell);
			}

			bool equals(int id1, int id2) const {
				return cell(id1) == cell(id2);
			}

			bool equals(int id, const sparse_cell_t& c) const {
				return cell(id) == c;
			}

			physics_system_t* ctx{};
		};

		using sparse_grid_t = lofi_hashtable1_t<sparse_grid_ops_t>;

		enum update_phase_t {
			PrepareSparseGrid,
			BuildSparseGrid,
			ReorderData,
			UpdateLightCells,
			UpdateHeavyCells,
			SubmitToRender,
			UpdatePhaseCount,
		};

		friend struct update_job_t;

		struct update_job_t : job_if_t {
			update_job_t(physics_system_t* _ctx, int _job_id)
				: ctx{_ctx}
				, job_id{_job_id}
			{}

			void execute() override {
				ctx->execute_job(this);
			}

			physics_system_t* ctx{};
			int job_id{};
		};

		struct sparse_neighbours_t {
			static constexpr int offset_count = 26;

			static constexpr const sparse_cell_t offsets[26] = {
				sparse_cell_t{-1, -1, -1},
				sparse_cell_t{0, -1, -1},
				sparse_cell_t{1, -1, -1},
				sparse_cell_t{-1, 0, -1},
				sparse_cell_t{0, 0, -1},
				sparse_cell_t{1, 0, -1},
				sparse_cell_t{-1, 1, -1},
				sparse_cell_t{0, 1, -1},
				sparse_cell_t{1, 1, -1},
				
				sparse_cell_t{-1, -1, 0},
				sparse_cell_t{0, -1, 0},
				sparse_cell_t{1, -1, 0},
				sparse_cell_t{-1, 0, 0},
				// sparse_cell_t{0, 0, 0}, // center cell
				sparse_cell_t{1, 0, 0},
				sparse_cell_t{-1, 1, 0},
				sparse_cell_t{0, 1, 0},
				sparse_cell_t{1, 1, 0},

				sparse_cell_t{-1, -1, 1},
				sparse_cell_t{0, -1, 1},
				sparse_cell_t{1, -1, 1},
				sparse_cell_t{-1, 0, 1},
				sparse_cell_t{0, 0, 1},
				sparse_cell_t{1, 0, 1},
				sparse_cell_t{-1, 1, 1},
				sparse_cell_t{0, 1, 1},
				sparse_cell_t{1, 1, 1},
			};

			static sparse_cell_t neighbour(const sparse_cell_t& c, int i) {
				return c + offsets[i];
			}

			int center{};
			int neighbours[26] = {};
		};

		// TODO : looks like little mess
		physics_system_t(engine_ctx_t* ctx, const physics_system_settings_t& settings)
			: system_if_t(ctx)
			, eps{settings.eps}
			, dt_step{settings.dt_step}
			, grid_scale{1.0f / settings.grid_scale}
			, max_particles{settings.max_particles}
			, particle_r{settings.particle_r}
			, particle_repulse_coef{settings.particle_repulse_coef}
			, heavy_cell_thresh{settings.heavy_cell_thresh}
			, updates_per_frame{settings.updates_per_frame}

			, sparse_grid{settings.max_particles, sparse_grid_ops_t{this}}
			, used_buckets{max_particles}
			, heavy_buckets{max_particles}
			, particles_reordered{max_particles}
		{
			auto* thread_pool = get_ctx()->get_system<thread_pool_system_t>("thread_pool");
			for (int i = 0; i < thread_pool->worker_count(); i++) {
				update_jobs.push_back(std::make_unique<update_job_t>(this, i));
			}

			auto* imgui = get_ctx()->get_system<imgui_system_t>("imgui");
			imgui->register_callback([&] (){
				return draw_ui();
			});
		}

		void update(float dt) {
			if (particles.empty()) {
				return;
			}

			for (int i = 0; i < updates_per_frame; i++) {
				sparse_grid.reset_size(particles.size());
				used_buckets.reset();
				heavy_buckets.reset();
				particles_reordered.reset();
				reordered = false;

				dispatch_jobs(PrepareSparseGrid);
				dispatch_jobs(BuildSparseGrid);

				dispatch_jobs(ReorderData);
				reordered = true;

				dispatch_jobs(UpdateLightCells);
				dispatch_jobs(UpdateHeavyCells);
			}
			dispatch_jobs(SubmitToRender);
		}

	private:
		bool draw_ui() {
			ImGui::SetNextWindowSize(ImVec2{256, 256}, ImGuiCond_Once);
			if (ImGui::Begin("physics")) {
				ImGui::Text("particles: %d", (int)particles.size());
				ImGui::Text("attractors: %d", (int)attractors.size());

				ImGui::PushItemWidth(-1.0f);
				ImGui::DragFloat("##repulse_coef", &particle_repulse_coef, 1.0f, 0.0f, 1000.0f, "repulse coef: %.1f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat("##dt_step", &dt_step, 0.0001f, 0.0f, 0.1f, "dt step: %.4f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat("##grid_scale", &grid_scale, 0.005f, 0.05f, 10.0f, "grid scale: %.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat("##particle_r", &particle_r, 0.005f, 0.05f, 10.0f, "particle r: %.3f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragInt("##updates_per_frame", &updates_per_frame, 1.0f, 0, 100, "updates per frame: %d", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragInt("##heavy_bucket_thresh", &heavy_cell_thresh, 1000.0f, 0, 10000000, "heavy cell thresh: %d", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat("##catch_radius", &catch_radius, 0.01f, 0.5f, 20.0f, "catch radius: %.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat("##bounding_r", &bounding_r, 1.0f, 1.0f, 10000.0f, "bounding r: %.3f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::PopItemWidth();

				ImGui::Checkbox("sync grid scale & particle", &sync_grid_scale_n_particle_r);
				if (sync_grid_scale_n_particle_r) {
					grid_scale = 1.0 / particle_r;
				}

				if (ImGui::CollapsingHeader("attractors")) {
					for (int i = 0; i < attractors.size(); i++) {
						ImGui::PushID(i);

						attractor_t& attractor = attractors[i];
						ImGui::PushItemWidth(-1.0f);
						ImGui::AlignTextToFramePadding();
						ImGui::Text("pos");
						ImGui::SameLine();
						ImGui::InputFloat3("##pos", glm::value_ptr(attractor.pos));
						ImGui::DragFloat("##GM", &attractor.GM, 10.0f, -1e5, +1e5, "GM: %.2f", ImGuiSliderFlags_AlwaysClamp);
						ImGui::PopItemWidth();

						ImGui::PopID();
					}
				}
			}
			ImGui::End();
			return true;
		}

		void dispatch_jobs(update_phase_t phase) {
			update_phase = phase;

			auto* thread_pool = get_ctx()->get_system<thread_pool_system_t>("thread_pool");
			for (auto& job : update_jobs) {
				thread_pool->push_job(job.get());
			}
			for (auto& job : update_jobs) {
				job->wait();
			}
		}

		void execute_job(update_job_t* job) {
			switch (update_phase) {
				case PrepareSparseGrid: {
					prepare_sparse_grid(job);
					break;
				}

				case BuildSparseGrid: {
					build_sparse_grid(job);
					break;
				}

				case ReorderData: {
					reorder_data(job);
					break;
				}

				case UpdateLightCells: {
					update_light_cells(job);
					break;
				}

				case UpdateHeavyCells: {
					update_heavy_cells(job);
					break;
				}

				case SubmitToRender: {
					submit_to_render(job);
					break;
				}
			}
		}

		void prepare_sparse_grid(update_job_t* job) {
			auto [start, stop] = compute_job_range(sparse_grid.get_buckets_count(), update_jobs.size(), job->job_id);

			lofi_bucket_t* buckets = sparse_grid.get_buckets_data();
			while (start != stop) {
				buckets[start++].reset();
			}
		}

		void build_sparse_grid(update_job_t* job) {
			auto [start, stop] = compute_job_range(std::min((int)particles.size(), max_particles), update_jobs.size(), job->job_id);

			constexpr int batch_size = 256;

			static_vector_t<int, batch_size> buckets{};
			while (start != stop) {
				int next_stop = std::min(start + batch_size, stop);
				while (start != next_stop) {
					auto [bucket, scans] = sparse_grid.put(start);
					assert(scans != -1);
					if (bucket != -1) {
						buckets.push_back(bucket);
					}
					start++;
				}

				if (!buckets.empty()) {
					int* insert_place = used_buckets.push(buckets.size());
					assert(insert_place);
					std::memcpy(insert_place, buckets.data(), buckets.size() * sizeof(int));
					buckets.reset();
				}
			}
		}

		void reorder_data(update_job_t* job) {
			auto [start, stop] = compute_job_range(used_buckets.get_size(), update_jobs.size(), job->job_id);

			int total_inserted = 0;
			for (int i = start; i < stop; i++) {
				total_inserted += sparse_grid.get_bucket(used_buckets[i]).get_count();
			}

			int region_start = particles_reordered.push_ext(total_inserted);
			for (int i = start; i < stop; i++) {
				int new_head = region_start;
				for (auto iter = sparse_grid.iter(used_buckets[i]); iter.valid(); iter.next()) {
					particles_reordered[region_start++] = particles[iter.get()];
				}
				sparse_grid.redir(used_buckets[i], new_head); // set head to the start of the consequent range
			}
		}

		void update_light_cells(update_job_t* job) {
			constexpr int batch_size = 256;

			auto [start, stop] = compute_job_range(used_buckets.get_size(), update_jobs.size(), job->job_id);

			static_vector_t<int, batch_size> heavy_buckets_batch;
			for (int i = start; i < stop; i++) {
				lofi_bucket_t& bucket = sparse_grid.get_bucket(used_buckets[i]);

				sparse_neighbours_t neighbours = get_neighbours(used_buckets[i]);
				if (!heavy_cell(neighbours)) {
					update_cell(neighbours, 0, bucket.get_count());
				} else {
					if (heavy_buckets_batch.can_push()) {
						heavy_buckets_batch.push_back(neighbours.center);
					} else {
						int* insert_place = heavy_buckets.push(heavy_buckets_batch.size());
						std::memcpy(insert_place, heavy_buckets_batch.data(), sizeof(int) * heavy_buckets_batch.size());
						heavy_buckets_batch.reset();
					}
				}
			}

			if (!heavy_buckets_batch.empty()) {
				int* insert_place = heavy_buckets.push(heavy_buckets_batch.size());
				std::memcpy(insert_place, heavy_buckets_batch.data(), sizeof(int) * heavy_buckets_batch.size());
				heavy_buckets_batch.reset();
			}
		}

		bool heavy_cell(const sparse_neighbours_t& neighbours) {
			if (sparse_grid.get_bucket(neighbours.center).get_count() < update_jobs.size()) {
				return false; // we cant split work for this cell
			}
			return compute_cell_work_amount(neighbours) >= heavy_cell_thresh;
		}

		std::int64_t compute_cell_work_amount(const sparse_neighbours_t& neighbours) {
			std::int64_t center_count = sparse_grid.get_bucket(neighbours.center).get_count();
			std::int64_t total_work = center_count * (center_count + 1);
			for (int neighbour : neighbours.neighbours) {
				if (neighbour == -1) {
					continue;
				}
				total_work += sparse_grid.get_bucket(neighbour).get_count() * center_count;
			}
			return total_work;
		}

		void update_heavy_cells(update_job_t* job) {
			int count = heavy_buckets.get_size();
			for (int i = 0; i < count; i++) {
				lofi_bucket_t& bucket = sparse_grid.get_bucket(heavy_buckets[i]);

				auto [start, stop] = compute_job_range(bucket.get_count(), update_jobs.size(), job->job_id);
				if (start >= stop) {
					continue;
				}

				sparse_neighbours_t neighbours = get_neighbours(heavy_buckets[i]);
				update_cell(neighbours, start, stop);
			}
		}

		void submit_to_render(update_job_t* job) {
			auto [start, stop] = compute_job_range(particles.size(), update_jobs.size(), job->job_id);
			if (start >= stop) {
				return;
			}

			auto* renderer = get_ctx()->get_system<basic_renderer_system_t>("basic_renderer");
			auto region = renderer->submit_next_frame(stop - start);

			for (int i = 0; i < region.count; i++) {
				particle_t& particle = particles[start + i];

				glm::mat4 mat{particle_r};
				mat[3] = glm::vec4(particle.pos, 1.0f); // TODO : very ugly
				region.mat[i] = mat;

				auto& mtl = region.mtl[i];
				mtl.color = glm::vec3(1.0f, 0.5f, 0.25f); //colors[particle.id];
				mtl.shininess = 64.0f;
				mtl.specular_strength = 1.0f;
			}
		}

		sparse_neighbours_t get_neighbours(int bucket_index) {
			lofi_bucket_t& bucket = sparse_grid.get_bucket(bucket_index);
			int head = bucket.get_head();

			sparse_cell_t center = get_sparse_cell(particles_reordered[head].pos, grid_scale);

			sparse_neighbours_t neighbours{bucket_index};
			for (int i = 0; i < sparse_neighbours_t::offset_count; i++) {
				neighbours.neighbours[i] = sparse_grid.get(sparse_neighbours_t::neighbour(center, i));
			}
			return neighbours;
		}

		struct cell_data_t {
			particle_t* particles_updated{};
			particle_t* particles_reordered{};
			int count{};
		};

		particle_t& get_particle(int id) {
			return !reordered ? particles[id] : particles_reordered[id];
		}

		cell_data_t get_cell_data(int cell) {
			lofi_bucket_t& bucket = sparse_grid.get_bucket(cell);
			int offset = bucket.get_head();
			int count = bucket.get_count();
			return {particles.data() + offset, particles_reordered.get_data(offset), count};
		}

		void update_cell(const sparse_neighbours_t& neighbours, int start, int stop) {
			auto center_cell_data = get_cell_data(neighbours.center);
			for (int i = start; i < stop; i++) {
				glm::vec3 acc{};
				particle_t& curr_particle = center_cell_data.particles_reordered[i];

				for (int neighbour : neighbours.neighbours) {
					if (neighbour == -1) {
						continue;
					}

					auto neighbour_cell_data = get_cell_data(neighbour);
					for (int j = 0; j < neighbour_cell_data.count; j++) {
						acc += spring_force_on_by(curr_particle.pos, neighbour_cell_data.particles_reordered[j].pos);
					}
				}
				
				for (int j = 0; j < center_cell_data.count; j++) {
					if (i == j) {
						continue;
					}
					acc += spring_force_on_by(curr_particle.pos, center_cell_data.particles_reordered[j].pos);
				}

				acc += env_force(curr_particle.pos, curr_particle.vel);

				particle_t& updated_particle = center_cell_data.particles_updated[i];
				std::tie(updated_particle.pos, updated_particle.vel) = integrate_motion(curr_particle.pos, curr_particle.vel, acc);
				updated_particle.id = curr_particle.id;
			}
		}

		// TODO : can be used to compute force on both particles (good idea but it does not seem to be the bottleneck)
		glm::vec3 spring_force_on_by(const glm::vec3& on, const glm::vec3& by) {
			glm::vec3 dr = on - by;
			float r = glm::length(dr);
			if (r < eps) {
				return glm::vec3{};
			}
			dr *= 1.0f / r;

			float l = r;
			float l0 = 2.0f * particle_r;
			float dl = l0 - std::min(l, l0);
			float k = particle_repulse_coef;

			return (k * dl) * dr; // spring-like
		}

		glm::vec3 env_force(const glm::vec3& pos, const glm::vec3& vel) {
			glm::vec3 acc{};
			for (auto& attractor : attractors) {
				glm::vec3 dr = pos - attractor.pos;
				float r = glm::length(dr);
				if (r < catch_radius) {
					return -vel * std::sqrt(std::max(r / catch_radius - 1.0f, 0.0f));
				}
				float ri = 1.0f / r;
				acc -= (attractor.GM * ri * ri) * (dr * ri);
			}
			return acc;
		}
		
		// TODO : this is ugly
		// TODO : split to integrate & contraint
		std::tuple<glm::vec3, glm::vec3> integrate_motion(const glm::vec3 r0, const glm::vec3& v0, const glm::vec3& a) {
			glm::vec3 v1 = v0 + dt_step * a;
			glm::vec3 r1 = r0 + dt_step * v1;
			if (float rr = glm::dot(r1, r1); rr > bounding_r * bounding_r) {
				r1 *= (bounding_r / std::sqrt(rr));

				glm::vec3 nr1 = r1 * (1.0f / bounding_r);
				float proj_v1r1 = glm::dot(v1, nr1);
				if (proj_v1r1 > eps) {
					v1 -= nr1 * (2.0f * proj_v1r1);
				}
			}
			return {r1, v1};
		}

	public:
		void add_particle(const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& color) {
			particles.push_back({pos, vel, (int)particles.size()});
			colors.push_back(color);
		}

		void add_attractor(const attractor_t& attractor) {
			attractors.push_back(attractor);
		}

	private:
		float eps{};
		float dt_step{};
		float grid_scale{};
		const int max_particles;
		float particle_r{};
		float particle_repulse_coef{};
		int heavy_cell_thresh{};
		int updates_per_frame{};
		
		bool sync_grid_scale_n_particle_r{};

		float catch_radius{5.0f};
		float bounding_r{100.0f};

		std::vector<attractor_t> attractors{};
		std::vector<particle_t> particles{};
		std::vector<glm::vec3> colors{};

		sparse_grid_t sparse_grid; 
		lofi_stack_t<int> used_buckets;
		lofi_stack_t<int> heavy_buckets;
		lofi_stack_t<particle_t> particles_reordered;
		bool reordered{};

		std::vector<std::unique_ptr<update_job_t>> update_jobs;
		update_phase_t update_phase{};
	};


	struct level_system_settings_t {
		int balls_count{};
	};

	class level_system_t : public system_if_t {
		void spawn_balls(int count) {
			auto* physics = get_ctx()->get_system<physics_system_t>("physics");
			for (int i = 0; i < count; i++) {
				glm::vec3 color = color_gen.gen();
				glm::vec3 pos = glm::vec3{coord_gen.gen(), coord_gen.gen(), coord_gen.gen()};
				glm::vec3 vel{};
				physics->add_particle(pos, vel, color);
			}
			balls_count += count;
		}

		void create_attractors() {
			auto* physics = get_ctx()->get_system<physics_system_t>("physics");
			physics->add_attractor({
				.pos = glm::vec3{0.0f, 0.0f, 0.0f},
				.GM = 1000.0f
			});
		}

		void create_basic_pass() {
			auto* ctx = get_ctx();

			auto color = ctx->get_resource<texture_t>("color");
			auto depth = ctx->get_resource<texture_t>("depth");

			auto framebuffer = std::make_shared<framebuffer_t>(framebuffer_t::create());
			framebuffer->attach(framebuffer_attachment_t{Color0, color->id});
			framebuffer->attach(framebuffer_attachment_t{Depth, depth->id});
			ctx->add_resource<framebuffer_t>("frame", framebuffer);

			omnidir_light_t light = {
				.ambient = glm::vec3(0.3f),
				.color = glm::vec3(10.0f),
				.pos = glm::vec3(0.0f),
			};

			camera_t camera = {
				.fov = glm::radians(60.0f),
				.near = 1.0f,
				.far = 200.0f,
				.eye = glm::vec3(0.0f, 0.0, 13.0),
				.center = glm::vec3(0.0f, 0.0f, 0.0f),
				.up = glm::vec3(0.0f, 1.0f, 0.0f)
			};

			basic_pass_t pass{
				framebuffer,
				viewport_t{0, 0, color->width, color->height},
				glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
				1.0f,
			};

			entity_t basic_pass(ctx);
			basic_pass.emplace<basic_renderer_pass_desc_t>(std::move(pass), std::move(camera), std::move(light));

			basic_pass_handle = basic_pass.get_handle();
		}

		void create_imgui_pass() {
			auto* ctx = get_ctx();

			auto& window = ctx->get_system<window_system_t>("window")->get_window();
			auto [width, height] = window.get_framebuffer_size();

			auto framebuffer = std::make_shared<framebuffer_t>();
			ctx->add_resource<framebuffer_t>("default_frame", framebuffer);

			entity_t imgui_pass(ctx);
			imgui_pass.emplace<imgui_pass_desc_t>(imgui_pass_t{
				.framebuffer = framebuffer,
				.viewport = {0, 0, width, height},
				.clear_color = glm::vec4(1.0f, 0.5f, 0.25f, 1.0f),
				.clear_depth = 1.0f,
			});

			imgui_pass_handle = imgui_pass.get_handle();
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
				ImGui::Text("balls: %d", balls_count);

				ImGui::SetNextItemWidth(100.0f);
				ImGui::InputInt("##balls_to_spawn", &balls_count_gui);
				ImGui::SameLine();
				if (ImGui::Button("spawn balls")) {
					spawn_balls(balls_count_gui);
				}
				ImGui::Checkbox("enable flying", &flying);
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
		level_system_t(engine_ctx_t* ctx, const level_system_settings_t& settings)
			: system_if_t(ctx) {
			spawn_balls(settings.balls_count);
			create_attractors();
			create_passes();
			create_shitty_gui();
		}

		~level_system_t() {
			auto* ctx = get_ctx();

			ctx->release(basic_pass_handle);
			ctx->release(imgui_pass_handle);

			ctx->remove_resource<framebuffer_t>("frame");
			ctx->remove_resource<framebuffer_t>("default_frame");
		}

		void update() {
			if (!flying) {
				return;
			}

			float t = glfw::get_time() * 0.1f;

			entity_t basic_pass{get_ctx(), basic_pass_handle};
			auto [pass, camera, light] = basic_pass.get<basic_renderer_pass_desc_t>();
			camera.eye = glm::vec3(30.0f * std::cos(t), 10.0f * std::sin(t * 1.0f), 30.0f * std::sin(t));
		}

	private:
		hsv_to_rgb_color_gen_t color_gen{42};
		float_gen_t coord_gen{42, -30.0f, +30.0f};
		handle_t basic_pass_handle{};
		handle_t imgui_pass_handle{};
		int balls_count{};
		int balls_count_gui{};
		bool flying{};

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
			constexpr int max_balls = 1 << 18;

			thread_pool = std::make_shared<thread_pool_system_t>(ctx, 24);
			ctx->add_system("thread_pool", thread_pool);

			window_system = std::make_shared<window_system_t>(ctx, window_width, window_height);
			ctx->add_system("window", window_system);

			imgui_system = std::make_shared<imgui_system_t>(ctx, window_system->get_window().get_handle(), "#version 460 core");
			ctx->add_system("imgui", imgui_system);

			basic_resources_system = std::make_shared<basic_resources_system_t>(ctx, tex_width, tex_height);
			ctx->add_system("basic_resources", basic_resources_system);

			basic_renderer_settings_t basic_renderer_settings{
				.max_instances = max_balls
			};

			basic_renderer_system = std::make_shared<basic_renderer_system_t>(ctx, basic_renderer_settings);
			ctx->add_system("basic_renderer", basic_renderer_system);

			physics_system_settings_t physics_settings{
				.eps = 1e-6f,
				.dt_step = 1e-3f,
				.max_particles = max_balls,
				.grid_scale = 1.0f,
				.particle_r = 2.0f,
				.particle_repulse_coef = 10.0f,
				.heavy_cell_thresh = 10000,
				.updates_per_frame = 4,
			};

			physics_system = std::make_shared<physics_system_t>(ctx, physics_settings);
			ctx->add_system("physics", physics_system);

			timer_system = std::make_shared<timer_system_t>(ctx);
			ctx->add_system("timer", timer_system);

			level_system_settings_t level_settings{
				.balls_count = 1,
			};

			level_system = std::make_shared<level_system_t>(ctx, level_settings);
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

			ctx->remove_system("thread_pool");
			thread_pool.reset();
		}

		virtual void execute() override {
			auto& window = window_system->get_window();
			auto& physics = *physics_system;
			auto& basic_renderer = *basic_renderer_system;
			auto& imgui = *imgui_system;
			auto& sync = *sync_component_system;
			auto& timer = *timer_system;
			auto& level = *level_system;

			glfwSwapInterval(0);

			double t0 = glfwGetTime();
			while (!window.should_close()) {
				double t1 = glfwGetTime();
				float dt = t1 - t0;
				t0 = t1;

				window.swap_buffers();

				glfw::poll_events();

				basic_renderer.render_prev_frame();

				physics.update(dt);
				timer.update(dt);
				imgui.update();
				level.update();
				sync.update();

				basic_renderer.finish_next_frame();
			}
		}

	private:
		engine_ctx_t* ctx{};
		std::shared_ptr<thread_pool_system_t> thread_pool;
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

	// TODO : point of no return - next iteration
}


// some pile of shit (tests)
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

int main() {
	//test_octotree_stuff();
	//test_sparse_grid_hash();
	//test_sparse_grid();
	//test_thread_pool1();
	//test_thread_pool2();
	//test_callback();

	engine_t engine;
	engine.execute();
	return 0;
}