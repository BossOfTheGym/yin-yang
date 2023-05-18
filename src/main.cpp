#include <bit>
#include <new>
#include <map>
#include <set>
#include <tuple>
#include <cmath>
#include <string>
#include <memory>
#include <random>
#include <vector>
#include <cassert>
#include <iomanip>
#include <utility>
#include <numeric>
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

#include <glfw.hpp>
#include <dt_timer.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

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

	struct demo_widget_t {
		void render() {
			if (show) {
				ImGui::ShowDemoWindow(&show);
			}
		}

		bool show{true};
	};

	static void help_marker(const char* desc) {
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	struct sim_widget_t {
		static constexpr ImGuiWindowFlags flags_init = ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;

		void render() {
			if (!opened) {
				return;
			}

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
			ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);
			if (ImGui::Begin("Test fullscreen", &opened, flags)) {
				if (ImGui::BeginChild("Side pane", ImVec2(150, 0), true)) {
					ImGui::Text("Mhhhh");
					ImGui::Button("Noice");
				} ImGui::EndChild();

				ImGui::SameLine();

				ImGui::BeginGroup();
				if (ImGui::BeginChild("Framebuffer"), ImVec2(0, 0), true) {
					ImGui::Text("Mhhhh");
					ImGui::Button("Noice");
				} ImGui::EndChild();
				ImGui::EndGroup();
			} ImGui::End();
		}

		ImGuiWindowFlags flags{flags_init};
		bool use_work_area{true};
		bool opened{true};
	};

	struct framebuffer_widget_t {
		static constexpr ImGuiWindowFlags flags_init = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

		void render(GLuint id, int width, int height) {
			if (!opened) {
				return;
			}

			ImGui::SetNextWindowPos(ImVec2(0, 0));
			//ImGui::SetNextWindowSize(ImVec2(width + 10, height + 10));
			if (ImGui::Begin("Framebuffer", &opened, flags)) {
				// workaround to flip texture
				ImVec2 pos = ImGui::GetCursorPos();
				pos.x += 0;
				pos.y += height;
				ImGui::SetCursorPos(pos);
				ImGui::Image(reinterpret_cast<ImTextureID>(id), ImVec2(width, -height));
			} ImGui::End();
		}

		ImGuiWindowFlags flags{flags_init};
		bool opened{true};
	};

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

			sphere_to_cartesian(glm::vec3(1.0, south_the, 0.0 * 2.0 * pi / 5.0 + 2.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 1.0 * 2.0 * pi / 5.0 + 2.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 2.0 * 2.0 * pi / 5.0 + 2.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 3.0 * 2.0 * pi / 5.0 + 2.0 * pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 4.0 * 2.0 * pi / 5.0 + 2.0 * pi / 10.0)),
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
		} for (int i = 0; i < subdiv; i++) {
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
			} return *this;
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
			} return *this;
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
			} return uniforms;
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
			} return program;
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
			} return *this;
		}

		void use() const {
			glUseProgram(id);
		}

		bool set_float(const char* name, GLfloat value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT);
				glProgramUniform1f(id, it->second.location, value);
			} return false;
		}

		bool set_int(const char* name, GLint value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_INT);
				glProgramUniform1i(id, it->second.location, value);
			} return false;
		}

		bool set_mat3(const char* name, const glm::mat3& value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT_MAT3);
				glProgramUniformMatrix3fv(id, it->second.location, 1, GL_FALSE, glm::value_ptr(value));
				return true;
			} return false;
		}

		bool set_mat4(const char* name, const glm::mat4& value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT_MAT4);
				glProgramUniformMatrix4fv(id, it->second.location, 1, GL_FALSE, glm::value_ptr(value));
				return true;
			} return false;
		}

		bool set_vec3(const char* name, const glm::vec3& value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT_VEC3);
				glProgramUniform3fv(id, it->second.location, 1, glm::value_ptr(value));
				return true;
			} return false;
		}

		bool set_vec4(const char* name, const glm::vec4& value) const {
			if (auto it = uniforms.find(name); it != uniforms.end()) {
				assert(it->second.type == GL_FLOAT_VEC4);
				glProgramUniform4fv(id, it->second.location, 1, glm::value_ptr(value));
				return true;
			} return false;
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
	float diffuse_coef = clamp(dot(light_ray, normal), 0.0, 1.0);
	vec3 diffuse = diffuse_coef * u_light_color;

	vec3 look_ray = normalize(u_eye_pos - world_pos);
	vec3 light_reflected = reflect(-light_ray, normal);
	float specular_coef = u_specular_strength * pow(clamp(dot(look_ray, light_reflected), 0.0, 1.0), u_shininess);
	vec3 specular = specular_coef * u_light_color;

	float att_coef = 1.0;
	if (diffuse_coef > 0.0) {
		float d = length(u_light_pos - world_pos);
		//att_coef = 1.0 / (1.0 + 0.1 * d + 0.1 * d * d);
	}

	color = vec4((ambient + diffuse + specular) * object_color, 1.0);
	color *= att_coef;
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
		} return std::make_tuple(std::move(program), out.str());
	}


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
				} return false;
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
					} return nullptr;
				} else {
					if (auto [it, inserted] = components.insert({handle, component_t{std::forward<args_t>(args)...}}); inserted) {
						return &it->second;
					} return nullptr;
				}
			}

			component_t* add(handle_t handle, const component_t& component) {
				if (auto [it, inserted] = components.insert({handle, component}); inserted) {
					return &it->second;
				} return nullptr;
			}

			component_t* add(handle_t handle, component_t&& component) {
				if (auto [it, inserted] = components.insert({handle, std::move(component)}); inserted) {
					return &it->second;
				} return nullptr;
			}

			virtual void remove(handle_t handle) override {
				components.erase(handle);
			}

			component_t* get(handle_t handle) {
				if (auto it = components.find(handle); it != components.end()) {
					return &it->second;
				} return nullptr;
			}

			component_t* peek() {
				if (!components.empty()) {
					return &components.begin()->second;
				} return nullptr;
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
			} return nullptr;
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
	// TODO : releasing a handle during destruction causes crash
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
			} return component_registry.emplace<component_t>(handle, std::forward<args_t>(args)...);
		}

		template<class component_t>
		component_t* add_component(handle_t handle, const component_t& component) {
			if (!is_alive(handle)) {
				std::cerr << "trying to add component to invalid handle " << handle << std::endl;
				return nullptr;
			} return component_registry.add<component_t>(handle, component);
		}

		template<class component_t>
		component_t* add_component(handle_t handle, component_t&& component) {
			if (!is_alive(handle)) {
				std::cerr << "trying to add component to invalid handle " << handle << std::endl;
				return nullptr;
			} return component_registry.add<component_t>(handle, std::move(component));
		}

		template<class component_t>
		void remove_component(handle_t handle) {
			if (!is_alive(handle)) {
				std::cerr << "trying to remove component using invalid handle " << handle << std::endl;
				return;
			} component_registry.remove<component_t>(handle);
		}

		template<class component_t>
		component_t* get_component(handle_t handle) {
			if (!is_alive(handle)) {
				std::cerr << "trying to get component using invalid handle " << handle << std::endl;
				return nullptr;
			} return component_registry.get<component_t>(handle);
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
			} return true;
		}

		bool remove_system(const std::string& name) {
			if (!system_registry.remove(name)) {
				std::cerr << "failed to remove system " << std::quoted(name) << std::endl;
				return false;
			} return true;
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
			} return *this;
		}

		shared_entity_t& operator = (shared_entity_t&& another) noexcept {
			if (this != &another) {
				std::swap(entity, another.entity);
			} return *this;
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
			} return *this;
		}

		weak_entity_t& operator = (weak_entity_t&& another) noexcept {
			if (this != &another) {
				std::swap(entity, another.entity);
			} return *this;
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
			, dt_split{info.dt_split}
		{}

	private:
		void cache_components() {
			cached_components.clear();
			index_to_handle.clear();
			for (auto& [handle, component] : get_ctx()->iterate_components<physics_t>()) {
				index_to_handle.push_back(handle);
				cached_components.push_back(&component);
			} total_objects = index_to_handle.size();
		}

		struct impact_data_t {
			glm::vec3 v1{}; float m1{};
			glm::vec3 v2{}; float m2{};
		};

		struct impact_t {
			glm::vec3 v{}; float m{};
		};

		glm::vec3 get_collision_axis(const physics_t& body1, const physics_t& body2) {
			glm::vec3 n = body2.pos - body1.pos;
			float n_len = glm::length(n);
			if (n_len > eps) {
				return n / n_len;
			} return glm::vec3(0.0f);
		}

		std::optional<impact_data_t> resolve_impact(const physics_t& body1, const physics_t& body2) {
			glm::vec3 n = get_collision_axis(body1, body2);
			if (glm::dot(n, n) < eps) {
				return std::nullopt;
			}
			// compute impulse change
			glm::vec3 v1 = body1.vel;
			glm::vec3 v2 = body2.vel;
			glm::vec3 dv = v2 - v1;
			float v1n = glm::dot(v1, n);
			float v2n = glm::dot(v2, n);
			float dvn = glm::dot(v2 - v1, n);
			float m1 = body1.mass;
			float m2 = body2.mass;
			float m = m1 + m2;
			float pn = m1 * v1n + m2 * v2n;
			float new_v1n = (pn + impact_cor * m2 * dvn) / m;
			float new_v2n = (pn - impact_cor * m1 * dvn) / m;
			glm::vec3 new_v1 = new_v1n * n + impact_v_loss * (v1 - v1n * n);
			glm::vec3 new_v2 = new_v2n * n + impact_v_loss * (v2 - v2n * n);
			return impact_data_t{new_v1, m1, new_v2, m2};
		}

		// r - distance between objects
		// r0 - radius (proj) of the first object
		// r1 - radius (proj) of the second object 
		// a - first point of overlap segment
		// b - second point of overlap segment
		struct overlap_t {
			float r{}, r0{}, r1{}, a{}, b{};
		};

		std::optional<overlap_t> get_sphere_sphere_overlap(const glm::vec3& r0, float rad0, const glm::vec3& r1, float rad1, float eps) {
			glm::vec3 dr = r1 - r0;
			float rr = glm::dot(dr, dr);
			if (rr - (rad0 + rad1) * (rad0 + rad1) > 0.0) {
				return std::nullopt;
			}
			float r = std::sqrt(rr);
			float a = std::max(-rad0, r - rad1);
			float b = std::min(+rad0, r + rad1);
			return overlap_t{r, rad0, rad1, a, b};
		}

		void resolve_overlaps() {
			last_overlap.resize(total_objects);
			for (auto& line : last_overlap) {
				line.clear();
				line.resize(total_objects, std::nullopt);
			}
			
			for (int k = 0; k < overlap_resolution_iters; k++) {
				for (int i = 0; i < total_objects; i++) {
					auto& body_i = *cached_components[i];
					for (int j = i + 1; j < total_objects; j++) {
						auto& body_j = *cached_components[j];
						
						last_overlap[i][j] = get_sphere_sphere_overlap(body_i.pos, body_i.radius, body_j.pos, body_j.radius, eps);
						if (!last_overlap[i][j]) {
							continue;
						} auto& overlap = *last_overlap[i][j];

						float coef = 0.5f * overlap_coef * (overlap.b - overlap.a) / overlap.r;
						glm::vec3 dr = coef * (body_i.pos - body_j.pos);

						float ki = body_i.mass / (body_i.mass + body_j.mass);
						float kj = body_j.mass / (body_i.mass + body_j.mass);
						body_i.pos += ki * dr;
						body_j.pos -= kj * dr;
					}
				}
			}

			impacts.resize(total_objects);
			for (auto& line : impacts) {
				line.clear();
			}

			for (int i = 0; i < total_objects; i++) {
				auto& body_i = *cached_components[i];
				for (int j = i + 1; j < total_objects; j++) {
					auto& body_j = *cached_components[j];

					if (!last_overlap[i][j]) {
						continue;
					}

					if (auto impact = resolve_impact(body_i, body_j)) {
						impacts[i].push_back({impact->v1, impact->m2});
						impacts[j].push_back({impact->v2, impact->m1});
					}
				}
			}

			for (int i = 0; i < total_objects; i++) {
				if (impacts[i].empty()) {
					continue;
				}
				float m = 0.0f;
				for (auto& data : impacts[i]) {
					m += data.m;
				}
				glm::vec3 v = glm::vec3(0.0f);
				for (auto& data : impacts[i]) {
					v += data.v * (data.m / m);
				}
				cached_components[i]->vel = v;
			}
		}

		glm::vec3 limit_vec(const glm::vec3& vec, float max_len) const {
			if (float len = glm::length(vec); len > max_len && len > eps) {
				return vec * (max_len / len);
			} return vec;
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
				} if (attractor.drag_min_dist <= dr_mag && dr_mag <= attractor.drag_max_dist) {
					float c0 = attractor.drag_min_coef;
					float c1 = attractor.drag_max_coef;
					float d0 = attractor.drag_min_dist;
					float d1 = attractor.drag_max_dist;
					float coef = (1.0f - c1 + c0) * glm::smoothstep(d1, d0, dr_mag) + c0;
					acc -= coef * state.vel;
				}
			} for (auto& [handle, force] : get_ctx()->iterate_components<force_t>()) {
				acc += force.dir * force.mag;
			} return acc;
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
				auto state = body_state_t{physics.pos, physics.vel, physics.force, physics.mass};
				state.force += compute_force(state);
				auto new_state = integrate_motion(state, dt);
				new_state.pos = limit_movement(state.pos, new_state.pos, movement_limit);
				new_state.vel = limit_vec(new_state.vel, velocity_limit);
				integrator_updates.push_back(new_state);
			}
		}

		struct print_vec_t{
			friend std::ostream& operator << (std::ostream& os, const print_vec_t& printer) {
				return os << printer.label << printer.vec.x << " " << printer.vec.y << " " << printer.vec.z;
			}

			const char* label{};
			const glm::vec3& vec;
		};

		void update_physics() {
			for (int i = 0; i < total_objects; i++) {
				auto& physics = *cached_components[i];
				auto& updated = integrator_updates[i];
				physics.pos = updated.pos;
				physics.vel = updated.vel;
				physics.force = glm::vec3(0.0f);

				// TODO : move somewhere else
				if (!physics.valid()) {
					std::cout << "object " << i << " nan detected on frame " << frame << std::endl;
					physics.pos = glm::vec3(0.0f);
					physics.vel = glm::vec3(0.0f);
				} if (glm::length(physics.pos) > 200.0f) {
					glm::vec3 dir = glm::normalize(physics.pos);
					physics.pos -= 50.0f * dir;
					physics.vel = -std::abs(glm::dot(physics.vel, dir)) * dir;
					std::cout << "adjusting  object " << i << ": "
						<< print_vec_t{"pos:", physics.pos} << " " << print_vec_t{"vel:", physics.vel} << std::endl;
				}
			}
		}

	public:
		void update(float dt) {
			cache_components();
			for (int i = 0; i < dt_split; i++) {
				resolve_overlaps();
				get_integrator_updates(dt / dt_split);
				update_physics();
			} frame++;
		}

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
		int dt_split{};

		int frame{};
		std::size_t total_objects{};
		std::vector<physics_t*> cached_components;
		std::vector<handle_t> index_to_handle;
		std::vector<std::vector<std::optional<overlap_t>>> last_overlap;
		std::vector<std::vector<impact_t>> impacts;
		std::vector<body_state_t> integrator_updates;
	};


	using tick_t = float;
	using timer_component_t = dt_timer_t<tick_t>;

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


	struct sync_transform_physics_t {};

	class sync_transform_physics_system_t : public system_if_t {
	public:
		sync_transform_physics_system_t(engine_ctx_t* ctx) : system_if_t(ctx) {}

		void update() {
			for (auto& [handle, sync] : get_ctx()->iterate_components<sync_transform_physics_t>()) {
				auto* transform = get_ctx()->get_component<transform_t>(handle);
				if (!transform) {
					std::cerr << "missing transform component. handle " << handle << std::endl;
					continue;
				}

				auto* physics = get_ctx()->get_component<physics_t>(handle);
				if (!physics) {
					std::cerr << "missing physics component. handle " << handle << std::endl;
					continue;
				}

				transform->translation = physics->pos;
			}
		}
	};


	struct sync_transform_attractor_t {};

	class sync_transform_attractor_system_t : public system_if_t {
	public:
		sync_transform_attractor_system_t(engine_ctx_t* ctx) : system_if_t(ctx) {}

		void update() {
			for (auto& [handle, sync] : get_ctx()->iterate_components<sync_transform_attractor_t>()) {
				auto* transform = get_ctx()->get_component<transform_t>(handle);
				if (!transform) {
					std::cerr << "missing transform component. handle " << handle << std::endl;
					continue;
				}

				auto* attractor = get_ctx()->get_component<attractor_t>(handle);
				if (!attractor) {
					std::cerr << "missing attractor component. handle " << handle << std::endl;
					continue;
				}

				transform->translation = attractor->pos;
			}
		}
	};


	class basic_resources_system_t : public system_if_t {
	public:
		basic_resources_system_t(engine_ctx_t* ctx, int tex_width, int tex_height) : system_if_t(ctx) {
			auto program_ptr = ([&] () {
				auto [program, info_log] = gen_basic_shader_program();
					if (!program.valid()) {
						std::cerr << info_log << std::endl;
						return std::make_shared<shader_program_t>();
					} return std::make_shared<shader_program_t>(std::move(program));
				}
			)();

			auto sphere_mesh_ptr = std::make_shared<mesh_t>(gen_sphere_mesh(1));
			auto sphere_vao_ptr = std::make_shared<vertex_array_t>(gen_vertex_array_from_mesh(*sphere_mesh_ptr));
			auto color = std::make_shared<texture_t>(gen_empty_texture(tex_width, tex_height, Rgba32f));
			auto depth = std::make_shared<texture_t>(gen_depth_texture(tex_width, tex_height, Depth32f));

			ctx->add_resource<shader_program_t>("basic_program", std::move(program_ptr));
			ctx->add_resource<mesh_t>("sphere", std::move(sphere_mesh_ptr));
			ctx->add_resource<vertex_array_t>("sphere", std::move(sphere_vao_ptr));
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

	struct basic_material_t {
		glm::vec3 color{};
		float specular_strength{};
		float shininess{};
		resource_ref_t<vertex_array_t> vao;
	};

	struct viewport_t {
		float get_aspect_ratio() const {
			return (float)width / height;
		}

		int x{}, y{}, width{}, height{};
	};

	struct basic_pass_t {
		resource_ref_t<framebuffer_t> framebuffer;
		viewport_t viewport{};
		glm::vec4 clear_color{};
		float clear_depth{};
		weak_entity_t camera{};
		weak_entity_t light{};
	};

	// TODO : instanced rendering
	class basic_renderer_system_t : public system_if_t {
	public:
		basic_renderer_system_t(engine_ctx_t* ctx) : system_if_t(ctx) {
			program = get_ctx()->get_resource<shader_program_t>("basic_program");
		}

	private:
		void render_pass(basic_pass_t* pass, camera_t* camera, omnidir_light_t* light) {
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
					} if (cull_face) {
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

			auto& viewport = pass->viewport;
			glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
			auto& col = pass->clear_color;
			glClearColor(col.r, col.g, col.b, col.a);
			glClearDepth(pass->clear_depth);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);

			program->use();
			program->set_mat4("u_v", camera->get_view());
			program->set_mat4("u_p", camera->get_proj(viewport.get_aspect_ratio()));
			program->set_vec3("u_eye_pos", camera->eye);
			program->set_vec3("u_ambient_color", light->ambient);
			program->set_vec3("u_light_color", light->color);
			program->set_vec3("u_light_pos", light->pos);

			GLuint prev_vao{};
			for (auto& [handle, material] : get_ctx()->iterate_components<basic_material_t>()) {
				auto* transform = get_ctx()->get_component<transform_t>(handle);
				if (!transform) {
					std::cerr << "missing transform component. handle: " << handle << std::endl;
					continue;
				}

				if (auto vao = material.vao.lock()) {
					if (vao->id != prev_vao) {
						vao->bind();
						prev_vao = vao->id;
					}
					program->set_mat4("u_m", transform->to_mat4());
					program->set_vec3("u_object_color", material.color);
					program->set_float("u_shininess", material.shininess);
					program->set_float("u_specular_strength", material.specular_strength);
					glDrawArrays(vao->mode, 0, vao->count);
				} else {
					std::cerr << "vao expired" << std::endl;
					continue;
				}
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

				render_pass(&pass, camera, light);
			}
		}

	private:
		resource_ptr_t<shader_program_t> program;
	};


	struct imgui_pass_t {
		resource_ref_t<framebuffer_t> framebuffer;
		viewport_t viewport{};
		glm::vec4 clear_color{};
		float clear_depth{};
	};

	// for now simplified
	class imgui_system_t : public system_if_t {
	public:
		imgui_system_t(engine_ctx_t* ctx, GLFWwindow* window, const std::string& version) : system_if_t(ctx) {
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGui::StyleColorsDark();
			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init(version.c_str());

			texture = get_ctx()->get_resource<texture_t>("color");
		}

		~imgui_system_t() {
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}

	private:
		void do_gui() {
			if (texture) {
				framebuffer_widget.render(texture->id, texture->width, texture->height);
			}
		}

	public:
		void set_texture(resource_ptr_t<texture_t> value) {
			texture = std::move(value);
		}

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
					} if (cull_face) {
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

	private:
		framebuffer_widget_t framebuffer_widget{};
		resource_ptr_t<texture_t> texture{};
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
		}

		glfw::window_t& get_window() {
			return *window;
		}

	private:
		std::unique_ptr<glfw::guard_t> glfw_guard;
		std::unique_ptr<glfw::window_t> window;
		std::unique_ptr<glew_guard_t> glew_guard;
	};


	using seed_t = std::uint64_t;

	seed_t shuffle(seed_t value) {
		return std::rotl(value, 17) * 0x123456789ABCDEF0 + std::rotr(value, 17);
	}

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
			, b_gen(shuffle(seed + 2), 0.0f, 1.0f) {}

		glm::vec3 gen() {
			float r = r_gen.gen(), g = g_gen.gen(), b = b_gen.gen();
			return glm::vec3(r, g, b);
		}

	private:
		float_gen_t r_gen, g_gen, b_gen;
	};

	class hsl_color_gen_t {
	public:
		hsl_color_gen_t(seed_t seed)
			: h_gen(shuffle(seed    ), 0.0f, 360.0f)
			, s_gen(shuffle(seed + 1), 0.0f, 1.0f)
			, l_gen(shuffle(seed + 2), 0.0f, 1.0f) {}

		glm::vec3 gen() {
			float h = h_gen.gen(), s = s_gen.gen(), l = l_gen.gen();
			return glm::vec3(h, s, l);
		}

	private:
		float_gen_t h_gen, s_gen, l_gen;
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
		} return rgb + glm::vec3(l - c * 0.5f);
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

	// TODO : test
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
		} return rgb + glm::vec3(v - c);
	}

	class hsv_to_rgb_color_gen_t : public hsv_color_gen_t {
	public:
		using base_t = hsv_color_gen_t;

		hsv_to_rgb_color_gen_t(seed_t seed) : base_t(seed) {}

		glm::vec3 gen() {
			return hsv_to_rgb(base_t::gen());
		}
	};

	struct level_system_info_t {
		// TODO : whatever configuration
	};

	class level_system_t : public system_if_t {
		void create_balls() {
			transform_t transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(1.0f),
				.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
				.translation = glm::vec3(0.0f) 
			};
			physics_t physics = {
				.pos = glm::vec3(0.0f, 0.0f, 0.0f),
				.vel = glm::vec3(0.0f, 0.0f, 0.0f),
				.mass = 1.0f,
				.radius = 1.0f,
			};
			basic_material_t material = {
				.color = glm::vec3(1.0f, 1.0f, 1.0f),
				.specular_strength = 0.5f,
				.shininess = 32.0f,
				.vao = get_ctx()->get_resource_ref<vertex_array_t>("sphere")
			};

			float_gen_t mass_gen(42, 0.5f, 1.0f);
			float_gen_t rad_gen(42, 0.3f, 1.0f);
			float_gen_t coord_gen(42, -50.0f, +50.0f);
			float_gen_t vel_gen(42, -30.0f, +30.0f);
			rgb_color_gen_t color_gen(42);

			int count = 200;

			ball_handles.clear();
			for (int i = 0; i < count; i++) {
				float rx = coord_gen.gen(), ry = coord_gen.gen(), rz = coord_gen.gen();
				float vx = vel_gen.gen(), vy = vel_gen.gen(), vz = vel_gen.gen();
				float radius = 0.5f;//rad_gen.gen();
				float mass = 1.0f;//mass_gen.gen();

				transform.scale = glm::vec3(radius);

				physics.pos = glm::vec3(rx, ry, rz);
				physics.vel = glm::vec3(vx, vy, vz);
				physics.radius = radius;
				physics.mass = mass;

				material.color = color_gen.gen();

				entity_t object = entity_t(get_ctx());
				object.add_component(physics);
				object.add_component(transform);
				object.add_component(material);
				object.add_component<sync_transform_physics_t>();

				ball_handles.push_back(object.incweakref().get_handle());
			}
		}

		void create_attractors() {
			transform_t transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(0.5f),
				.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
				.translation = glm::vec3(0.0f)
			};
			attractor_t attractor = {
				.pos = glm::vec3(0.0f),
				.gm = 1000.0f,
				.min_dist = 2.0f,
				.max_dist = 200.0f,
				.drag_min_coef = 0.2,
				.drag_max_coef = 1.0f,
				.drag_min_dist = 0.0f,
				.drag_max_dist = 7.0f,
			};
			basic_material_t material = {
				.color = glm::vec3(1.0f, 0.0f, 0.0f),
				.specular_strength = 0.5f,
				.shininess = 32.0f,
				.vao = get_ctx()->get_resource_ref<vertex_array_t>("sphere")
			};

			entity_t object = entity_t(get_ctx());
			object.add_component(transform);
			object.add_component(attractor);
			object.add_component(material);
			object.add_component<sync_transform_attractor_t>();

			attractor_handle = object.incweakref().get_handle();
		}

		void create_light() {
			omnidir_light_t light = {
				.ambient = glm::vec3(0.2f),
				.color = glm::vec3(1.0f),
				.pos = glm::vec3(5.0f),
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
				.eye = glm::vec3(20.0f, 0.0, 0.0),
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

	public:
		level_system_t(engine_ctx_t* ctx) : system_if_t(ctx) {
			create_balls();
			create_attractors();
			create_light();
			create_viewer();
			create_passes();
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

	private:
		std::vector<handle_t> ball_handles;
		handle_t attractor_handle{};
		handle_t light_handle{};
		handle_t viewer_handle{};
		handle_t basic_pass_handle{};
		handle_t imgui_pass_handle{};
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
			constexpr int window_width = 300;
			constexpr int window_height = 300;
			constexpr int tex_width = 280;
			constexpr int tex_height = 280;

			physics_system_info_t physics_system_info{
				.eps = 1e-6f,
				.overlap_coef = 0.4f,
				.overlap_resolution_iters = 2,
				.movement_limit = 200.0f,
				.velocity_limit = 200.0f,
				.impact_cor = 0.8f,
				.impact_v_loss = 0.99f,
				.dt_split = 8,
			};

			window_system = std::make_shared<window_system_t>(ctx, window_width, window_height);
			ctx->add_system("window", window_system);

			basic_resources_system = std::make_shared<basic_resources_system_t>(ctx, tex_width, tex_height);
			ctx->add_system("basic_resources", basic_resources_system);

			basic_renderer_system = std::make_shared<basic_renderer_system_t>(ctx);
			ctx->add_system("basic_renderer", basic_renderer_system);

			imgui_system = std::make_shared<imgui_system_t>(ctx, window_system->get_window().get_handle(), "#version 460 core");
			ctx->add_system("imgui", imgui_system);

			physics_system = std::make_shared<physics_system_t>(ctx, physics_system_info);
			ctx->add_system("physics", physics_system);

			sync_transform_physics_system = std::make_shared<sync_transform_physics_system_t>(ctx);
			ctx->add_system("sync_transform_physics", sync_transform_physics_system);

			sync_transform_attractor_system = std::make_shared<sync_transform_attractor_system_t>(ctx);
			ctx->add_system("sync_transform_attractor", sync_transform_attractor_system);

			timer_system = std::make_shared<timer_system_t>(ctx);
			ctx->add_system("timer", timer_system);

			level_system = std::make_shared<level_system_t>(ctx);
			ctx->add_system("level", level_system);
		}

		~mainloop_t() {
			// TODO : now there is no way to provide clean destruction of resources
			// but we can create system that will clear all dependent resources from engine_ctx
			// it can be basic_resources_system here, for example
			// many resources (like textures) depend on corresponding systems so this dependency management is something to be considered
			// for now this is fine, we got a bunch of workarounds so it must work 
			ctx->remove_system("level");
			level_system.reset();

			ctx->remove_system("timer");
			timer_system.reset();

			ctx->remove_system("sync_transform_attractor");
			sync_transform_attractor_system.reset();

			ctx->remove_system("sync_transform_physics");
			sync_transform_physics_system.reset();

			ctx->remove_system("physics");
			physics_system.reset();

			ctx->remove_system("imgui");
			imgui_system.reset();

			ctx->remove_system("basic_renderer");
			basic_renderer_system.reset();

			ctx->remove_system("basic_resources");
			basic_resources_system.reset();

			ctx->remove_system("window");
			window_system.reset();
		}

		virtual void execute() override {
			auto& window = window_system->get_window();
			auto& physics = *physics_system;
			auto& basic_renderer = *basic_renderer_system;
			auto& imgui = *imgui_system;
			auto& sync_transform_physics = *sync_transform_physics_system;
			auto& sync_transform_attractor = *sync_transform_attractor_system;
			auto& timer = *timer_system;

			float dt = 0.017f;
			while (!window.should_close()) {
				window.swap_buffers();

				glfw::poll_events();

				timer.update(dt);
				physics.update(dt);
				sync_transform_physics.update();
				sync_transform_attractor.update();
				basic_renderer.update();
				imgui.update();
			}
		}

	private:
		engine_ctx_t* ctx{};
		std::shared_ptr<window_system_t> window_system;
		std::shared_ptr<basic_resources_system_t> basic_resources_system;
		std::shared_ptr<basic_renderer_system_t> basic_renderer_system;
		std::shared_ptr<imgui_system_t> imgui_system;
		std::shared_ptr<physics_system_t> physics_system;
		std::shared_ptr<sync_transform_physics_system_t> sync_transform_physics_system;
		std::shared_ptr<sync_transform_attractor_system_t> sync_transform_attractor_system;
		std::shared_ptr<timer_system_t> timer_system;
		std::shared_ptr<level_system_t> level_system;
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

	// TODO : ball_spawner_system_t
	// TODO : fancy_attractor_system_t
	
	// TODO : logging

	// TODO : advanced physics
	// TODO : grid update method
	// TODO : multithreaded update

	// TODO : advanced drawing
	// TODO : G-buffer
	// TODO : create depth texture
	// TODO : create color texture
	// TODO : create normal texture
	// TODO : create pos texture
	// TODO : instanced drawing
	// TODO : create first pass : draw everything into the G-buffer
	// TODO : create second pass : apply light

	// TODO : whatever TODO you see
}

int main() {
	engine_t engine;
	engine.execute();
	return 0;
}