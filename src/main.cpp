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
		static constexpr ImGuiWindowFlags flags_init = 0;

		void render(ImTextureID id, int width, int height) {
			if (!opened) {
				return;
			}

			if (ImGui::Begin("Framebuffer", &opened, flags)) {
				// workaround to flip texture
				ImVec2 pos = ImGui::GetCursorPos();
				pos.x += 0;
				pos.y += height;
				ImGui::SetCursorPos(pos);
				ImGui::Image(id, ImVec2(width, -height));
			} ImGui::End();
		}

		ImGuiWindowFlags flags{};
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

	// TODO
	struct vertex_array_proxy_t {
		std::shared_ptr<vertex_array_t> vao;
		GLenum mode{};
		int first{};
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

	// general_setup: some common setups, so there is no need it for each draw command
	// vao_setups: some sutps specific for each vao draw command
	// vao_drawers: draw command, how to draw each vao
	struct basic_render_seq_t {
		using shader_program_setup_t = std::function<void(shader_program_t& program)>;
		using vertex_array_drawer_t = std::function<void(shader_program_t& program, vertex_array_t& vao)>;

		using empty_setup_t = decltype([] (shader_program_t&) {});
		using empty_drawer_t = decltype([] (shader_program_t&, vertex_array_t&) {});

		basic_render_seq_t(std::shared_ptr<shader_program_t> _program) : program{std::move(_program)} {
			assert(program && program->valid());
		}

		template<class setup_t>
		void set_general_setup(setup_t&& setup) {
			general_setup = std::forward<setup_t>(setup);
		}

		template<class drawer_t>
		void add_draw_command(std::shared_ptr<vertex_array_t> vao, drawer_t&& drawer) {
			vaos.push_back(std::move(vao));
			vao_drawers.push_back(std::forward<drawer_t>(drawer));
		}

		void draw() {
			program->use();
			if (general_setup) {
				general_setup(*program);
			}

			GLuint prev_vao_id = 0;
			for (int i = 0; i < vaos.size(); i++) {
				if (vaos[i]->id != prev_vao_id) {
					vaos[i]->bind();
				} vao_drawers[i](*program, *vaos[i]);
			}
		}

		std::shared_ptr<shader_program_t> program;
		shader_program_setup_t general_setup;
		std::vector<std::shared_ptr<vertex_array_t>> vaos;
		std::vector<vertex_array_drawer_t> vao_drawers;
	};

	struct basic_pass_t {
		using pass_setup_t = std::function<void(framebuffer_t&)>;
		using pass_action_t = pass_setup_t;

		basic_pass_t() : fbo{framebuffer_t::create()} {}

		template<class setup_t>
		void add_setup(setup_t&& setup) {
			setups.push_back(std::forward<setup_t>(setup));
		}

		template<class action_t>
		void execute_action(action_t&& action) {
			fbo.bind();
			for (auto& setup : setups) {
				setup(fbo);
				action(fbo);
			}
		}

		framebuffer_t fbo;
		std::vector<pass_setup_t> setups;
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
			handle_t acquire() {
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
				assert((size_t)handle < handles.size());
				return handles[handle] == handle;
			}

			void clear() {
				head = 0;
				for (int i = 1; i < handles.size(); i++) {
					handles[i - 1] = i;
				} handles.back() = null_handle;
			}

		private:
			std::vector<handle_t> handles;
			handle_t head{null_handle};
		};
	}
	
	using handle_t = unsigned;
	inline constexpr handle_t null_handle = ~0;

	using handle_pool_t = impl::handle_pool_t<handle_t, null_handle>;

	template<class object_t>
	struct handle_storage_t {
		using handles_t = handle_pool_t;	
		using storage_t = std::unordered_map<handle_t, object_t>;

		// simplified
		handle_t add(const object_t& object) {
			handle_t handle = handles.acquire();
			assert(get(handle) == nullptr);
			objects[handle] = object;
			return handle;
		}

		void del(handle_t handle) {
			objects.erase(handle);
			handles.release(handle);
		}

		object_t* get(handle_t handle) {
			if (auto it = objects.find(handle); it != objects.end()) {
				return &it->second;
			} return nullptr;
		}

		auto begin() {
			return objects.begin();
		}

		auto begin() const {
			return objects.begin();
		}

		auto end() {
			return objects.end();
		}

		auto end() const {
			return objects.end();
		}

		handles_t handles;
		storage_t objects;
	};

	// point attractor
	// a = GM / |r - r0|^2 * (r - r0) / |r - r0|
	struct attractor_t {
		glm::vec3 pos{};
		float gm{};
		float min_dist{};
		float max_dist{};
	};

	// directional force
	struct force_t {
		glm::vec3 dir{};
	};

	struct body_state_t {
		glm::vec3 pos{};
		glm::vec3 vel{};
		glm::vec3 force{};
		float mass{};
	};

	struct physics_t {
		glm::vec3 pos{};
		glm::vec3 vel{};
		glm::vec3 force{};
		float mass{};
		float radius{};
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

	struct material_t {
		glm::vec3 color{};
		float specular_strength{};
		float shininess{};
	};

	struct camera_t {
		glm::mat4 get_view() const {
			return glm::translate(glm::lookAt(eye, center, up), -eye);
		}

		glm::mat4 get_proj() const {
			return glm::perspective(glm::radians(45.0f), aspect_ratio, 0.1f, 100.0f);
		}

		float fov{};
		float aspect_ratio{};
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

	struct vao_t {
		std::shared_ptr<vertex_array_t> vao;
	};

	// giant abomination, yep, very cheap solution
	// TODO : dismember?
	struct object_t {
		transform_t transform{};
		physics_t physics{};
		material_t material{};
		attractor_t attractor{};
		force_t force{};
		camera_t camera{};
		omnidir_light_t omnilight{};
		vao_t vao{};
	};

	struct velocity_verlet_integrator_t {
		using object_accessor_t = std::function<object_t*(handle_t)>;

		template<class accessor_t>
		void set_object_accessor(accessor_t&& accessor) {
			object_accessor = std::forward<accessor_t>(accessor);
		}

		object_t& access_object(handle_t handle) {
			return *object_accessor(handle);
		}

		object_accessor_t object_accessor;


		void add_attractor(handle_t handle) {
			attractor_handles.insert(handle);
		}

		void del_attractor(handle_t handle) {
			attractor_handles.erase(handle);
		}

		std::unordered_set<handle_t> attractor_handles;


		void add_force(handle_t handle) {
			force_handles.insert(handle);
		}

		void del_force(handle_t handle) {
			force_handles.erase(handle);
		}

		std::unordered_set<handle_t> force_handles;


		force_t compute_force(const body_state_t& state) {
			force_t total_force{state.force / state.mass};
			for (auto handle : attractor_handles) {
				auto& attractor = access_object(handle).attractor; // oh no, we're going to crash :D
				glm::vec3 dr = attractor.pos - state.pos;
				float dr_mag = glm::length(dr);
				float dr_mag2 = glm::dot(dr, dr);
				if (attractor.min_dist <= dr_mag && dr_mag <= attractor.max_dist) {
					total_force.dir += attractor.gm / dr_mag2 * dr / dr_mag;
				}
			} for (auto handle : force_handles) {
				auto& force = access_object(handle).force; // oh no, we're going to crash :D
				total_force.dir += force.dir;
			} return total_force;
		}

		body_state_t update(const body_state_t& state, float dt) {
			body_state_t new_state = state;
			force_t f0 = compute_force(state);
			glm::vec3 vel_tmp = state.vel + 0.5f * f0.dir * dt;
			new_state.pos = state.pos + vel_tmp * dt;
			force_t f1 = compute_force(new_state);
			new_state.vel = vel_tmp + 0.5f * f1.dir * dt;
			return new_state;
		}
	};

	template<class type_t, class = void>
	struct is_integrator_t : std::false_type {};

	template<class type_t>
	struct is_integrator_t<type_t, 
		std::enable_if_t<
			std::is_invocable_r_v<force_t, decltype(&type_t::compute_force), const type_t*, const body_state_t&>
			&& std::is_invocable_r_v<body_state_t, decltype(&type_t::update), const type_t*, const body_state_t&, float>
		>> : std::true_type {};

	template<class type_t>
	inline constexpr bool is_integrator_v = is_integrator_t<type_t>::value;

	struct integrator_proxy_t {
		using compute_force_func_t = force_t(*)(void*, const body_state_t&);
		using update_func_t = body_state_t(*)(void*, const body_state_t&, float);

		integrator_proxy_t() = default;

		template<class integrator_t>
		integrator_proxy_t(integrator_t& integrator) noexcept {
			*this = integrator;
		}

		template<class integrator_t>
		integrator_proxy_t& operator = (integrator_t& integrator) noexcept {
			if ((void*)this == (void*)&integrator) {
				return *this;
			}

			instance = &integrator;
			compute_force_func = [] (void* inst, const body_state_t& state) {
				return ((integrator_t*)inst)->compute_force(state);
			};
			update_func = [] (void* inst, const body_state_t& state, float dt) {
				return ((integrator_t*)inst)->update(state, dt);
			};
			return *this;
		}

		integrator_proxy_t(const integrator_proxy_t&) = default;
		integrator_proxy_t(integrator_proxy_t&&) noexcept = default;

		~integrator_proxy_t() = default;

		integrator_proxy_t& operator = (const integrator_proxy_t&) = default;
		integrator_proxy_t& operator = (integrator_proxy_t&&) noexcept = default;

		force_t compute_force(const body_state_t& state) const {
			return compute_force_func(instance, state);
		}

		body_state_t update(const body_state_t& state, float dt) const {
			return update_func(instance, state, dt);
		}

		void* instance{};
		compute_force_func_t compute_force_func{};
		update_func_t update_func{};
	};

	// center = r + t * v, t from [0, 1]
	// returns t - time of the first contact
	std::optional<float> collide_moving_sphere_sphere(const glm::vec3& r0, const glm::vec3& dr0, float rad0,
									const glm::vec3& r1, const glm::vec3& dr1, float rad1, float eps) {
		glm::vec3 dr = r1 - r0;
		glm::vec3 dv = dr1 - dr0; // yes, it will be called dv here but it is not velocity
		float rad = rad0 + rad1;
		float c = glm::dot(dr, dr) - rad * rad;
		if (c < 0.0f) {
			return 0.0f;
		}
		float a = glm::dot(dv, dv);
		if (a < eps) {
			return std::nullopt;
		}
		float b = glm::dot(dr, dv);
		if (b > eps) {
			return std::nullopt;
		}
		float d = b * b - a * c;
		if (d < eps) {
			return std::nullopt;
		} return (-b - std::sqrt(d)) / a;
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

	struct resolved_t {
		glm::vec3 mv1{};
		glm::vec3 mv2{};
	};

	template<class coefs_t>
	resolved_t get_overlap_movements(const physics_t& body0, const physics_t& body1, const overlap_t& overlap,
								float coef, float eps, coefs_t coefs) {
		if (overlap.r < eps) {
			return {glm::vec3(0.0f), glm::vec3(0.0f)};
		}
		glm::vec3 dr = (body1.pos - body0.pos) / overlap.r;
		float mv = (overlap.b - overlap.a) * coef * 0.5f;
		auto [mv0, mv1] = coefs(body0, body1, overlap, mv, eps);
		return {dr * mv0, dr * mv1};
	}

	resolved_t get_overlap_movements_overlap(const physics_t& body0, const physics_t& body1, const overlap_t& overlap,
									float coef, float eps) {
		return get_overlap_movements(body0, body1, overlap, coef, eps,
			[] (auto& body0, auto& body1, auto& overlap, float mv, float eps) {
				float r0r1 = overlap.r0 + overlap.r1;
				if (r0r1 > eps) {
					float mv0 = -mv * overlap.r0 / (overlap.r0 + overlap.r1);
					float mv1 = +mv * overlap.r1 / (overlap.r0 + overlap.r1);
					return std::tuple{mv0, mv1};
				} return std::tuple{0.0f, 0.0f};
			}
		);
	}

	resolved_t get_overlap_movements_mass(const physics_t& body0, const physics_t& body1, const overlap_t& overlap,
										float coef, float eps) {
		return get_overlap_movements(body0, body1, overlap, coef, eps,
			[] (auto& body0, auto& body1, auto& overlap, float mv, float eps) {
				float mv0 = -mv * body1.mass / (body0.mass + body1.mass);
				float mv1 = +mv * body0.mass / (body0.mass + body1.mass);
				return std::tuple{mv0, mv1};
			}
		);
	}

	struct physics_system_info_t {
		float eps{};
		float overlap_coef{};
		float overlap_thresh{};
		float overlap_spring_coef{};
		int overlap_resolution_iters{};
		float movement_limit{};
		float velocity_limit{};
		integrator_proxy_t integrator{};
	};

	class physics_system_t {
	public:
		static constexpr float no_collision = 2.0f;

		physics_system_t(const physics_system_info_t& info)
			: eps{info.eps}
			, overlap_coef{info.overlap_coef}
			, overlap_thresh{info.overlap_thresh}
			, overlap_spring_coef{info.overlap_spring_coef}
			, overlap_resolution_iters{info.overlap_resolution_iters}
			, movement_limit{info.movement_limit}
			, velocity_limit{info.velocity_limit}
			, integrator{info.integrator}
			{}

		using object_accessor_t = std::function<object_t*(handle_t)>;

		template<class accessor_t>
		void set_object_accessor(accessor_t&& accessor) {
			object_accessor = std::forward<accessor_t>(accessor);
		}

		object_t& access_object(handle_t handle) {
			return *object_accessor(handle);
		}

		template<class integrator_t>
		void set_integrator(integrator_t& _integrator) {
			integrator = _integrator;
		}

		void add(handle_t handle) {
			objects.insert(handle);
		}

		void del(handle_t handle) {
			objects.erase(handle);
		}


	private:
		void rebuild_index_to_handle_map() {
			index_to_handle.clear();
			for (auto& handle : objects) {
				index_to_handle.push_back(handle);
			}
		}

		void resolve_overlaps() {
			for (int i = 0; i < overlap_resolution_iters; i++) {
				overlap_matrix.resize(objects.size());
				for (auto& line : overlap_matrix) {
					line.clear();
					line.resize(objects.size(), std::nullopt);
				}

				overlap_movement.clear();
				overlap_movement.resize(objects.size(), glm::vec3(0.0f));
				for (int i = 0; i < objects.size(); i++) {
					auto& body_i = access_object(index_to_handle[i]).physics;
					for (int j = i + 1; j < objects.size(); j++) {
						auto& body_j = access_object(index_to_handle[j]).physics;
						auto overlap_opt = get_sphere_sphere_overlap(body_i.pos, body_i.radius, body_j.pos, body_j.radius, eps);
						overlap_matrix[i][j] = overlap_opt;
						if (!overlap_opt) {
							continue;
						}
						auto [mvi, mvj] = get_overlap_movements_mass(body_i, body_j, *overlap_opt, overlap_coef, eps);
						overlap_movement[i] += mvi;
						overlap_movement[j] += mvj;
					}
				}

				for (int i = 0; i < objects.size(); i++) {
					access_object(index_to_handle[i]).physics.pos += overlap_movement[i];
				}

				glm::vec3 accum = glm::vec3(0.0f);
				for (auto& mv : overlap_movement) {
					accum += glm::abs(mv);
				} if (accum.x + accum.y + accum.y < overlap_thresh) {
					return; // no more movements
				}
			}

			for (int i = 0; i < objects.size(); i++) {
				auto& body_i = access_object(index_to_handle[i]).physics;
				for (int j = i + 1; j < objects.size(); j++) {
					auto& body_j = access_object(index_to_handle[j]).physics;
					auto& overlap_opt = overlap_matrix[i][j];
					if (!overlap_opt) {
						continue;
					}
					// spring model
					auto& overlap = *overlap_opt;
					if (overlap.r < eps) {
						continue;
					}
					float base_length = body_i.radius + body_j.radius;
					float curr_length = overlap.r;
					float k = overlap_spring_coef * (curr_length - base_length);
					glm::vec3 dr = (body_j.pos - body_i.pos) / overlap.r;
					body_i.force +=  k * dr;
					body_j.force += -k * dr;
				}
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

		void get_integrator_updates(float dt) {
			integrator_updates.clear();
			for (auto handle : index_to_handle) {
				auto& physics = access_object(handle).physics;
				auto state = body_state_t{physics.pos, physics.vel, physics.force, physics.mass};
				auto new_state = integrator.update(state, dt);
				new_state.pos = limit_movement(state.pos, new_state.pos, movement_limit);
				new_state.vel = limit_vec(new_state.vel, velocity_limit);
				integrator_updates.push_back(new_state);
			}
		}

		struct resolved_impulse_t {
			glm::vec3 resolved_velocity{};
			float affecting_mass{};
		};

		void sync_object_transform_physics(object_t& object) {
			object.transform.translation = object.physics.pos;
		}

		// we also apply here some physics
		// we will ignore t = 0
		void resolve_collisions() {
			first_collision.clear();
			first_collision.resize(objects.size(), no_collision);
			
			collision_matrix.resize(objects.size());
			for (auto& line : collision_matrix) {
				line.clear();
				line.resize(objects.size(), no_collision);
			}
			
			for (int i = 0; i < objects.size(); i++) {
				auto& body_i = access_object(index_to_handle[i]).physics;
				auto& updated_i = integrator_updates[i];
				for (int j = i + 1; j < objects.size(); j++) {
					auto& body_j = access_object(index_to_handle[j]).physics;
					auto& updated_j = integrator_updates[j];
					float t = collide_moving_sphere_sphere(body_i.pos, updated_i.pos - body_i.pos, body_i.radius,
								body_j.pos, updated_j.pos - body_j.pos, body_j.radius, eps).value_or(no_collision);
					collision_matrix[i][j] = t;
					collision_matrix[j][i] = t;
				} first_collision[i] = *std::min_element(collision_matrix[i].begin(), collision_matrix[i].end());
			}

			for (int i = 0; i < objects.size(); i++) {
				for (int j = i + 1; j < objects.size(); j++) {
					if (first_collision[j] < collision_matrix[i][j]) {
						collision_matrix[i][j] = no_collision;
						collision_matrix[j][i] = no_collision;
					}
				} first_collision[i] = *std::min_element(collision_matrix[i].begin(), collision_matrix[i].end());
			}

			// TODO : when t = 0 objects stick together
			// algorithm thinks that they collide so coef = 0.0 and their position is not updated 
			for (int i = 0; i < objects.size(); i++) {
				auto& physics = access_object(index_to_handle[i]).physics;
				auto& updated = integrator_updates[i];
				float coef = first_collision[i] != no_collision ? first_collision[i] : 1.0f;
				physics.pos += coef * (updated.pos - physics.pos);
				physics.vel = updated.vel;
				physics.force = glm::vec3(0.0f);
			}

			resolved_impulses.resize(objects.size());
			for (auto& line : resolved_impulses) {
				line.clear();
				line.resize(objects.size(), {});
			}

			auto get_collision_axes = [&] (auto& body1, auto& body2) {
				glm::vec3 n = body2.pos - body1.pos;
				float n_len = glm::length(n);
				if (n_len > eps) {
					n /= n_len;
				} else {
					return std::tuple{false, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f)};
				}

				glm::vec3 u1 = glm::cross(n, body1.vel); // zero-vector or colinear vector cases go here 
				float u1_len = glm::length(u1);
				if (u1_len < eps) {
					u1 = glm::cross(n, body2.vel); // zero-vector or colinear vector cases go here
					u1_len = glm::length(u1);
				} if (u1_len > eps) {
					u1 /= u1_len;
				} else {
					return std::tuple{true, n, glm::vec3(0.0f), glm::vec3(0.0f)}; // both velocities colinear with normal
				}

				glm::vec3 u2 = glm::normalize(glm::cross(n, u1)); // guaranteed to exist
				return std::tuple{true, n, u1, u2};
			};

			for (int i = 0; i < objects.size(); i++) {
				auto& body_i = access_object(index_to_handle[i]).physics;
				for (int j = i + 1; j < objects.size(); j++) {
					auto& body_j = access_object(index_to_handle[j]).physics;
					if (collision_matrix[i][j] == no_collision || collision_matrix[i][j] > first_collision[i]) {
						continue;
					}

					// axes we project our velocities on
					auto [any_axes, n, u1, u2] = get_collision_axes(body_i, body_j);
					if (!any_axes) {
						continue;
					}

					// compute impulse change
					glm::vec3 dv = body_j.vel - body_i.vel;
					float m = body_i.mass + body_j.mass;
					float dvn = glm::dot(dv, n);
					float vin = glm::dot(body_i.vel, n) + 2.0f * dvn * (body_j.mass / m);
					float vjn = glm::dot(body_j.vel, n) - 2.0f * dvn * (body_i.mass / m);
					glm::vec3 vi = vin * n + glm::dot(body_i.vel, u1) * u1 + glm::dot(body_i.vel, u2) * u2;
					glm::vec3 vj = vjn * n + glm::dot(body_j.vel, u1) * u1 + glm::dot(body_j.vel, u2) * u2;

					resolved_impulses[i][j] = {vi, body_j.mass};
					resolved_impulses[j][i] = {vj, body_i.mass};
				}
			}

			for (int i = 0; i < objects.size(); i++) {
				float m = std::accumulate(resolved_impulses[i].begin(), resolved_impulses[i].end(), 0.0f,
					[&] (const auto& acc, const auto& data) {
						return acc + data.affecting_mass;
					}
				);
				if (m < eps) {
					continue;
				}
				glm::vec3 v = std::accumulate(resolved_impulses[i].begin(), resolved_impulses[i].end(), glm::vec3(0.0f),
					[&] (const auto& acc, const auto& data) {
						return acc + data.resolved_velocity * (data.affecting_mass / m);
					}
				);
				auto& object = access_object(index_to_handle[i]);
				object.physics.vel = v;
			}

			for (auto handle : index_to_handle) {
				sync_object_transform_physics(access_object(handle));
			}
		}

	public:
		void update(float dt) {
			rebuild_index_to_handle_map();
			resolve_overlaps();
			get_integrator_updates(dt);
			resolve_collisions();
		}

	private:
		float eps{};

		float overlap_coef{};
		float overlap_thresh{};
		float overlap_spring_coef{};
		int overlap_resolution_iters{};
		std::vector<glm::vec3> overlap_movement;
		std::vector<std::vector<std::optional<overlap_t>>> overlap_matrix;

		float movement_limit{};
		float velocity_limit{};
		integrator_proxy_t integrator;

		object_accessor_t object_accessor;
		std::unordered_set<handle_t> objects;

		std::vector<handle_t> index_to_handle;
		std::vector<body_state_t> integrator_updates;
		std::vector<float> first_collision;
		std::vector<std::vector<float>> collision_matrix; // values from [0, 1] or no_collision value
		std::vector<std::vector<resolved_impulse_t>> resolved_impulses;
	};

	// TODO : entity spawner

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
}

int main() {
	constexpr int window_width = 400;
	constexpr int window_height = 400;
	constexpr int tex_width = 256;
	constexpr int tex_height = 256;
	constexpr int fbo_width = 300;
	constexpr int fbo_height = 300;

	glfw::guard_t glfw_guard;
	glfw::window_t window(glfw::window_params_t::create_basic_opengl("yin-yang", window_width, window_height, 4, 6));

	window.make_ctx_current();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	const char* glsl_version = "#version 460";
	ImGui_ImplGlfw_InitForOpenGL(window.get_handle(), true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	bool show_demo_window = true;
	{
		glew_guard_t glew_guard;
		demo_widget_t demo;
		sim_widget_t sim;
		framebuffer_widget_t framebuffer_widget;

		auto test_tex = ([&] () {
			return std::make_shared<texture_t>(gen_test_texture(tex_width, tex_height));
		})();

		auto program_ptr = ([&] () {
			auto [program, info_log] = gen_basic_shader_program();
			if (!program.valid()) {
				std::cerr << info_log << std::endl;
				return std::shared_ptr<shader_program_t>();
			} return std::make_shared<shader_program_t>(std::move(program));
		})();
		if (!program_ptr) {
			return -1;
		}

		for (auto& [name, props] : program_ptr->uniforms) {
			std::cout << props << std::endl;
		}

		auto sphere_mesh_ptr = ([&] () { return std::make_shared<mesh_t>(gen_sphere_mesh(1)); })();

		auto sphere_vao_ptr = ([&] () {
			return std::make_shared<vertex_array_t>(gen_vertex_array_from_mesh(*sphere_mesh_ptr));
		})();

		auto fbo_color = ([&] () {
			return std::make_shared<texture_t>(gen_empty_texture(fbo_width, fbo_height, Rgba32f));
		})();

		auto fbo_depth = ([&] () {
			return std::make_shared<texture_t>(gen_depth_texture(fbo_width, fbo_height, Depth32f));
		})();

		basic_pass_t pass;
		pass.add_setup([&] (framebuffer_t& fbo) {
			fbo.attach({Color0, fbo_color->id, 0});
			fbo.attach({Depth, fbo_depth->id, 0});
			glClearColor(0.0, 0.0, 0.0, 1.0);
			glClearDepth(1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glViewport(0, 0, fbo_width, fbo_height);
		});

		handle_storage_t<object_t> objects;

		handle_t ball0 = objects.add({
			.transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(1.0f), .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), .translation = glm::vec3(0.0f) 
			},
			.physics = {
				.pos = glm::vec3(0.0f, 0.0f, 5.0f), .vel = glm::vec3(0.0f, 0.0f, 0.0f), .mass = 0.2f, .radius = 1.0f,
			},
			.material = { .color = glm::vec3(0.0f, 1.0f, 0.0f), .specular_strength = 0.5f, .shininess = 32.0f, }
		});

		handle_t ball1 = objects.add({
			.transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(1.0f), .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), .translation = glm::vec3(0.0f) 
			},
			.physics = {
				.pos = glm::vec3(0.0f, 0.0f, -5.0f), .vel = glm::vec3(0.0f, 0.0f, 0.0f), .mass = 0.2f, .radius = 1.0f
			},
			.material = { .color = glm::vec3(0.0f, 0.0f, 1.0f), .specular_strength = 0.5f, .shininess = 32.0f, }
		});

		handle_t viewer = objects.add({
			.camera = {
				.fov = 45.0f, .aspect_ratio = (float)fbo_width / fbo_height, .near = 0.1f, .far = 100.0f,
				.eye = glm::vec3(0.0f, 10.0f, 0.0f),
				.center = glm::vec3(0.0f, 0.0f, 0.0f),
				.up = glm::vec3(0.0f, 0.0f, 1.0f)
			}
		});

		handle_t light_source = objects.add({
			.omnilight = {
				.ambient = glm::vec3(0.1f, 0.1f, 0.1f),
				.color = glm::vec3(1.0f, 1.0f, 1.0f),
				.pos = glm::vec3(10.0f, 10.0f, 10.0f),
			}
		});

		handle_t attractor0 = objects.add({
			.transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(0.5f),
				.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
				.translation = glm::vec3(2.0f, 0.0f, 0.0f)
			},
			.material = { .color = glm::vec3(1.0f, 0.0f, 0.0f), .specular_strength = 0.5, .shininess = 32.0f },
			.attractor = { .pos = glm::vec3(+2.0f, 0.0f, 0.0f), .gm = 35.0f, .min_dist = 0.1, .max_dist = 200.0f }
		});

		handle_t attractor1 = objects.add({
			.transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(0.5f),
				.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
				.translation = glm::vec3(-2.0f, 0.0f, 0.0f)
			},
			.material = { .color = glm::vec3(1.0f, 0.0f, 0.0f), .specular_strength = 0.5, .shininess = 32.0f },
			.attractor = { .pos = glm::vec3(-2.0f, 0.0f, 0.0f), .gm = 35.0f, .min_dist = 0.1, .max_dist = 200.0f }
		});

		std::unordered_set<handle_t> basic_renderables = {ball0, ball1, attractor0, attractor1};

		basic_render_seq_t render_seq(program_ptr);
		render_seq.set_general_setup([&] (shader_program_t& program) {
			auto viewer_ptr = objects.get(viewer);
			auto light_source_ptr = objects.get(light_source);
			program.set_mat4("u_v", viewer_ptr->camera.get_view());
			program.set_mat4("u_p", viewer_ptr->camera.get_proj());
			program.set_vec3("u_ambient_color", light_source_ptr->omnilight.ambient);
			program.set_vec3("u_light_color", light_source_ptr->omnilight.color);
			program.set_vec3("u_light_pos", light_source_ptr->omnilight.pos);
			program.set_vec3("u_eye_pos", viewer_ptr->camera.eye);
		});

		render_seq.add_draw_command(sphere_vao_ptr,
			[&] (shader_program_t& program, vertex_array_t& vao) {
				for (auto handle : basic_renderables) {
					auto& object = *objects.get(handle);
					program.set_mat4("u_m", object.transform.to_mat4());
					program.set_vec3("u_object_color", object.material.color);
					program.set_float("u_shininess", object.material.shininess);
					program.set_float("u_specular_strength", object.material.specular_strength);
					glDrawArrays(vao.mode, 0, vao.count);
				}
			}
		);

		auto object_accessor = [&] (handle_t handle) { return objects.get(handle); };

		velocity_verlet_integrator_t vel_ver_int;
		vel_ver_int.set_object_accessor(object_accessor);
		vel_ver_int.add_attractor(attractor0);
		vel_ver_int.add_attractor(attractor1);

		physics_system_info_t physics_system_info{
			.eps = 1e-6,
			.overlap_coef = 0.3,
			.overlap_thresh = 1e-4,
			.overlap_spring_coef = 1.0,
			.overlap_resolution_iters = 5,
			.movement_limit = 10.0f,
			.velocity_limit = 10.0f,
			.integrator = vel_ver_int,
		};
		physics_system_t physics(physics_system_info);
		physics.set_object_accessor(object_accessor);
		physics.add(ball0);
		physics.add(ball1);

		while (!window.should_close()) {
			glfw::poll_events();

			physics.update(0.05f);

			pass.execute_action([&] (framebuffer_t& fbo) {
				render_seq.draw();
			});

			// TODO : default framebuffer setup
			auto [w, h] = window.get_framebuffer_size();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClearColor(0.2, 0.2, 0.2, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			glViewport(0, 0, w, h);

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			//demo.render();
			//sim.render();
			framebuffer_widget.render(reinterpret_cast<ImTextureID>(fbo_color->id), fbo_color->width, fbo_color->height);

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			window.swap_buffers();
		}
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	return 0;
}