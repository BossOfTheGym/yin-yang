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
	class handle_storage_t {
	public:
		using handles_t = handle_pool_t;	
		using storage_t = std::unordered_map<handle_t, object_t>;

		// simplified
		[[nodiscard]] handle_t add(const object_t& object) {
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

	private:
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
		float mag{};
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
			return glm::perspective(glm::radians(45.0f), aspect_ratio, near, far);
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

	struct controller_if_t {
		virtual ~controller_if_t() = default;
	};

	struct controller_t {
		controller_if_t* operator -> () {
			return impl.get();
		}

		std::unique_ptr<controller_if_t> impl;
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
		controller_if_t controller;
	};

	using object_accessor_t = std::function<object_t*(handle_t)>;

	// TODO
	struct basic_renderer_t {
		object_accessor_t object_accessor;
		handle_t viewer{null_handle};
		std::unordered_set<handle_t> renderables;
		basic_render_seq_t render_seq;
		basic_pass_t render_pass;
	};
	
	struct body_state_t {
		glm::vec3 pos{};
		glm::vec3 vel{};
		glm::vec3 force{};
		float mass{};
	};

	struct velocity_verlet_integrator_t {
		template<class accessor_t>
		void set_object_accessor(accessor_t&& accessor) {
			object_accessor = std::forward<accessor_t>(accessor);
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


		glm::vec3 compute_force(const body_state_t& state) {
			glm::vec3 acc{};
			for (auto handle : attractor_handles) {
				auto& attractor = object_accessor(handle)->attractor;
				glm::vec3 dr = attractor.pos - state.pos;
				float dr_mag = glm::length(dr);
				float dr_mag2 = glm::dot(dr, dr);
				if (attractor.min_dist <= dr_mag && dr_mag <= attractor.max_dist) {
					acc += attractor.gm / dr_mag2 * dr / dr_mag;
				}
			} for (auto handle : force_handles) {
				auto& force = object_accessor(handle)->force;
				acc += force.dir * force.mag;
			} return acc;
		}

		// simplification, compute acceleration beforehand
		// this method will update state considering it acceleration as const
		body_state_t update(const body_state_t& state, float dt) {
			body_state_t new_state = state;
			glm::vec3 acc = state.force / state.mass;
			glm::vec3 vel_tmp = state.vel + 0.5f * acc * dt;
			new_state.pos = state.pos + vel_tmp * dt;
			new_state.vel = vel_tmp + 0.5f * acc * dt;
			return new_state;
		}
	};

	template<class type_t, class = void>
	struct is_integrator_t : std::false_type {};

	template<class type_t>
	struct is_integrator_t<type_t, 
		std::enable_if_t<
			std::is_invocable_r_v<force_t, decltype(&type_t::compute_acc), const type_t*, const body_state_t&>
			&& std::is_invocable_r_v<body_state_t, decltype(&type_t::update), const type_t*, const body_state_t&, float>
		>> : std::true_type {};

	template<class type_t>
	inline constexpr bool is_integrator_v = is_integrator_t<type_t>::value;

	struct integrator_proxy_t {
		using compute_force_func_t = glm::vec3(*)(void*, const body_state_t&);
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

		glm::vec3 compute_force(const body_state_t& state) const {
			return compute_force_func(instance, state);
		}

		body_state_t update(const body_state_t& state, float dt) const {
			return update_func(instance, state, dt);
		}

		void* instance{};
		compute_force_func_t compute_force_func{};
		update_func_t update_func{};
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
		float impulse_cor{};
		float impact_thresh{};
		int dt_split{};
		integrator_proxy_t integrator{};
	};

	class physics_system_t {
	public:
		static constexpr float no_collision = 2.0f;

		physics_system_t(const physics_system_info_t& info)
			: eps{info.eps}
			, overlap_coef{info.overlap_coef}
			, overlap_vel_coef{info.overlap_vel_coef}
			, overlap_thresh{info.overlap_thresh}
			, overlap_spring_coef{info.overlap_spring_coef}
			, overlap_resolution_iters{info.overlap_resolution_iters}
			, movement_limit{info.movement_limit}
			, velocity_limit{info.velocity_limit}
			, touch_thresh{info.touch_thresh}
			, touch_coef_workaround{info.touch_coef_workaround}
			, impulse_cor{info.impulse_cor}
			, impact_thresh{info.impact_thresh}
			, dt_split{info.dt_split}
			, integrator{info.integrator}
			{}

		template<class accessor_t>
		void set_object_accessor(accessor_t&& accessor) {
			dirty = true;
			object_accessor = std::forward<accessor_t>(accessor);
		}

		template<class integrator_t>
		void set_integrator(integrator_t& _integrator) {
			integrator = _integrator;
		}

		void add(handle_t handle) {
			dirty = true;
			objects.insert(handle);
		}

		void del(handle_t handle) {
			dirty = true;
			objects.erase(handle);
		}


	private:
		void cache_objects() {
			if (!dirty) {
				return;
			}
			cached_objects.clear();
			index_to_handle.clear();
			for (auto& handle : objects) {
				index_to_handle.push_back(handle);
				cached_objects.push_back(object_accessor(handle));
			} dirty = false;
		}

		struct collision_axes_t {
			glm::vec3 n{}, u1{}, u2{};
		};

		std::optional<collision_axes_t> get_collision_axes(const physics_t& body1, const physics_t& body2) {
			glm::vec3 n = body2.pos - body1.pos;
			float n_len = glm::length(n);
			if (n_len > eps) {
				n /= n_len;
			} else {
				return std::nullopt;
			}

			glm::vec3 u1 = glm::cross(n, body1.vel); // zero-vector or colinear vector cases go here 
			float u1_len = glm::length(u1);
			if (u1_len < eps) {
				u1 = glm::cross(n, body2.vel); // zero-vector or colinear vector cases go here
				u1_len = glm::length(u1);
			} if (u1_len > eps) {
				u1 /= u1_len;
			} else {
				return collision_axes_t{n, glm::vec3(0.0f), glm::vec3(0.0f)}; // both velocities colinear with normal
			}

			glm::vec3 u2 = glm::normalize(glm::cross(n, u1)); // guaranteed to exist
			return collision_axes_t{n, u1, u2};
		}

		struct resolved_impact_t {
			glm::vec3 v1{};
			glm::vec3 v2{};
		};

		std::optional<resolved_impact_t> resolve_impact(const physics_t& body1, const physics_t& body2) {
			// axes we project our velocities on
			auto axes_opt = get_collision_axes(body1, body2);
			if (!axes_opt) {
				return std::nullopt;
			} auto& [n, u1, u2] = *axes_opt;

			float n_len = glm::length(n), u1_len = glm::length(u1), u2_len = glm::length(u2);

			// compute impulse change
			glm::vec3 v1 = body1.vel;
			glm::vec3 v2 = body2.vel;

			float v1_len = glm::length(v1), v2_len = glm::length(v2);

			float m1 = body1.mass;
			float m2 = body2.mass;
			float p1 = glm::dot(v1, n);
			float p2 = glm::dot(v2, n);
			glm::vec3 dv = v2 - v1;
			float m = m1 + m2;
			float dvn = glm::dot(dv, n);
			float v1n = (m1 * p1 + m2 * p2 + impulse_cor * m2 * dvn) / m;
			float v2n = (m1 * p1 + m2 * p2 - impulse_cor * m1 * dvn) / m;
			glm::vec3 new_v1 = v1n * n + glm::dot(v1, u1) * u1 + glm::dot(v1, u2) * u2;
			glm::vec3 new_v2 = v2n * n + glm::dot(v2, u1) * u1 + glm::dot(v2, u2) * u2;

			float new_v1_len = glm::length(new_v1), new_v2_len = glm::length(new_v2);

			return resolved_impact_t{new_v1, new_v2};
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

		void resolve_overlaps(float dt) {
			for (int i = 0; i < objects.size(); i++) {
				auto& body_i = cached_objects[i]->physics;
				for (int j = i + 1; j < objects.size(); j++) {
					auto& body_j = cached_objects[j]->physics;
					
					auto overlap_opt = get_sphere_sphere_overlap(body_i.pos, body_i.radius, body_j.pos, body_j.radius, eps);
					if (!overlap_opt) {
						continue;
					}

					float coef = 0.5f * (overlap_opt->b - overlap_opt->a) / overlap_opt->r;
					glm::vec3 dr = coef * (body_i.pos - body_j.pos);

					float ki = body_i.mass / (body_i.mass + body_j.mass);
					float kj = body_j.mass / (body_i.mass + body_j.mass);
					body_i.pos += ki * dr;
					body_j.pos -= kj * dr;
					if (auto vels = resolve_impact(body_i, body_j)) {
						body_i.vel = vels->v1;
						body_j.vel = vels->v2;
					}
				}
			}

			// apply spring to unresolved overlaps
			for (int i = 0; i < objects.size(); i++) {
				auto& body_i = cached_objects[i]->physics;
				for (int j = i + 1; j < objects.size(); j++) {
					auto& body_j = cached_objects[j]->physics;

					glm::vec3 dr = body_i.pos - body_j.pos;
					float r = glm::dot(dr, dr);
					float r1r2 = body_i.radius + body_j.radius;
					if (r > r1r2 * r1r2) {
						continue;
					}

					r = std::sqrt(r);
					dr /= r;

					// spring model
					float k = overlap_spring_coef * (r - r1r2);
					body_i.force += +k * dr;
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
			for (int i = 0; i < objects.size(); i++) {
				auto& physics = cached_objects[i]->physics;
				auto state = body_state_t{physics.pos, physics.vel, physics.force, physics.mass};
				state.force += integrator.compute_force(state);
				auto new_state = integrator.update(state, dt);
				new_state.pos = limit_movement(state.pos, new_state.pos, movement_limit);
				new_state.vel = limit_vec(new_state.vel, velocity_limit);
				integrator_updates.push_back(new_state);
			}
		}

		void update_physics() {
			for (int i = 0; i < objects.size(); i++) {
				auto& physics = cached_objects[i]->physics;
				auto& updated = integrator_updates[i];
				physics.pos = updated.pos;
				physics.vel = updated.vel;
				physics.force = glm::vec3(0.0f);
			}
		}

		void sync() {
			for (auto* object : cached_objects) {
				object->transform.translation = object->physics.pos;
			}
		}

	public:
		void update(float dt) {
			cache_objects();
			for (int i = 0; i < dt_split; i++) {
				resolve_overlaps(dt / dt_split);
				get_integrator_updates(dt / dt_split);
				update_physics();
			} sync();
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
		float impulse_cor{};
		float impact_thresh{};
		int dt_split{};
		integrator_proxy_t integrator;

		object_accessor_t object_accessor;

		bool dirty{true};
		std::unordered_set<handle_t> objects;
		std::vector<object_t*> cached_objects;
		std::vector<handle_t> index_to_handle;
		std::vector<body_state_t> integrator_updates;
	};

	using tick_t = std::uint64_t;
	using timer_release_func_t = void(*)(void* instance, handle_t handle);

	class timer_handle_t {
	public:
		timer_handle_t() = default;
		timer_handle_t(handle_t _handle, void* _instance, timer_release_func_t _release)
			: handle{_handle}, instance{_instance}, release{_release} {}

		~timer_handle_t() {
			release_handle();
		}

		timer_handle_t(const timer_handle_t&) = delete;
		timer_handle_t(timer_handle_t&& another) noexcept {
			*this = std::move(another);
		}

		timer_handle_t& operator = (const timer_handle_t&) = delete;
		timer_handle_t& operator = (timer_handle_t&& another) noexcept {
			if (this != &another) {
				std::swap(handle, another.handle);
				std::swap(instance, another.instance);
				std::swap(release, another.release);
			} return *this;
		}

		void release_handle() {
			release(instance, handle);
		}

		handle_t get_handle() const {
			return handle;
		}

	private:
		handle_t handle{};
		void* instance{};
		timer_release_func_t release{};
	};

	class timer_system_t {
	private:
		static void release_timer(void* instance, handle_t handle) {
			timer_system_t* sys = (timer_system_t*)instance;
			sys->remove(handle);
		}

	public:
		template<class handler_t>
		[[nodiscard]] timer_handle_t add(tick_t tick, handler_t&& handler) {
			handle_t handle = handle_pool.acquire();
			auto& timer = timers[handle];
			timer.set_tick(tick);
			timer.set_timeout_handler(std::forward<handler_t>(handler));
			return timer_handle_t(handle, this, release_timer);
		}

		void remove(handle_t handle) {
			timers.erase(handle);
		}

		void update(tick_t dt) {
			for (auto& [handle, timer] : timers) {
				timer.update(dt);
			}
		}

	private:
		std::unordered_map<handle_t, dt_timer_t<tick_t>> timers;
		handle_pool_t handle_pool;
	};

	// struct cone_vec_gen_t {

	// };

	// class ball_spawner_t : public controller_if_t {
	// public:
	// 	ball_spawner_t(engine_ctx_if_t* _ctx, std::uint64_t _seed, float _radius, float _radius_variance, float _mass, float _mass_variance, const glm::vec3& ) {

	// 	}

	// 	void spawn_ball() {

	// 	}

	// private:
	// 	engine_ctx_if_t* ctx{};
	// 	std::uint64_t seed{};
	// 	float radius{};
	// 	float radius_variance{};
	// 	float mass{};
	// 	float mass_variance{};
	// 	glm::vec3 velocity{};
		
	// 	tick_t interval{};
	// 	timer_handle_t timer_handle{null_handle};
	// };

	// object_t create_ball_spawner() {

	// }

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

	std::vector<object_t> create_balls() {
		object_t ball{
			.transform = {
				.base = glm::mat4(1.0f),
				.scale = glm::vec3(1.0f), .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), .translation = glm::vec3(0.0f) 
			},
			.physics = {
				.pos = glm::vec3(-5.0f, 0.0f, 0.0f), .vel = glm::vec3(0.0f, 0.0f, 0.0f), .mass = 0.2f, .radius = 1.0f
			},
			.material = { .color = glm::vec3(0.0f, 0.0f, 1.0f), .specular_strength = 0.5f, .shininess = 32.0f, }
		};
		std::vector<object_t> balls;
		for (int i = 0; i < 20; i++) {
			float r = 10.0f;
			float angle = glm::radians(360.0f) / 20.0f * i;
			ball.physics.pos = glm::vec3(r * std::cos(angle), 0.0f, r * std::sin(angle));
			balls.push_back(ball);
		} return balls;
	}
}

int main() {
	constexpr int window_width = 1280;
	constexpr int window_height = 720;
	constexpr int tex_width = 256;
	constexpr int tex_height = 256;
	constexpr int fbo_width = 1100;
	constexpr int fbo_height = 680;

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

		handle_t viewer = objects.add({
			.camera = {
				.fov = 75.0f, .aspect_ratio = (float)fbo_width / fbo_height, .near = 1.0f, .far = 300.0f,
				.eye = glm::vec3(0.0f, 20.0f, 0.0f),
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
				.translation = glm::vec3(0.0f, 0.0f, 0.0f)
			},
			.material = { .color = glm::vec3(1.0f, 0.0f, 0.0f), .specular_strength = 0.5, .shininess = 32.0f },
			.attractor = { .pos = glm::vec3(0.0f, 0.0f, 0.0f), .gm = 300.0f, .min_dist = 1.0, .max_dist = 200.0f }
		});

		std::vector<handle_t> balls;
		for (auto& obj : create_balls()) {
			balls.push_back(objects.add(obj));
		}

		std::unordered_set<handle_t> basic_renderables = {attractor0};
		for (auto handle : balls) {
			basic_renderables.insert(handle);
		}

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

		physics_system_info_t physics_system_info{
			.eps = 1e-6f,
			.overlap_coef = 0.5f,
			.overlap_spring_coef = 2.0f,
			.overlap_resolution_iters = 4,
			.movement_limit = 100.0f,
			.velocity_limit = 100.0f,
			.impulse_cor = 0.5f,
			.impact_thresh = 1e-3f,
			.dt_split = 1,
			.integrator = vel_ver_int,
		};
		physics_system_t physics(physics_system_info);
		physics.set_object_accessor(object_accessor);
		for (auto& handle : balls) {
			physics.add(handle);
		}

		while (!window.should_close()) {
			glfw::poll_events();

			physics.update(0.003f);

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