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
#include <cstdint>
#include <cstddef>
#include <variant>
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
				ImGui::Image(id, ImVec2(width, height));
			} ImGui::End();
		}

		ImGuiWindowFlags flags{};
		bool opened{true};
	};

	struct texture_t {
		texture_t() = default;
		texture_t(GLuint _id, int _width, int _height) : id{_id}, width{_width}, height{_height} {}

		~texture_t() {
			if (id) {
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

		texture_t tex{(GLuint)0, width, height};
		glCreateTextures(GL_TEXTURE_2D, 1, &tex.id);
		glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(tex.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(tex.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureStorage2D(tex.id, 1, GL_RG32F, width, height);
		glTextureSubImage2D(tex.id, 0, 0, 0, width, height, GL_RG, GL_FLOAT, tex_data.data());
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
			if (id != 0) {
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
	constexpr glm::vec3 sphere_to_cartesian(const glm::vec3& coords) {
		float r = coords.x;
		float cos_the = std::cos(coords.y), sin_the = std::sin(coords.y);
		float cos_phi = std::cos(coords.z), sin_phi = std::sin(coords.z);
		return glm::vec3(r * sin_the * cos_phi, r * cos_the, r * sin_the * sin_phi);
	}

	// TODO : subdivision
	mesh_t gen_sphere_mesh(int subdiv = 0) {
		constexpr float pi = 3.14159265359;
		constexpr float angle = std::atan(0.5);
		constexpr float north_the = pi / 2.0 - angle;
		constexpr float south_the = pi / 2.0 + angle;

		constexpr int ico_vertices = 12;
		constexpr int total_faces = 20;
		constexpr int total_vertices = 3 * total_faces;

		glm::vec3 icosahedron[ico_vertices] = {
			sphere_to_cartesian(glm::vec3(1.0, 0.0, 0.0)), // north pole
			sphere_to_cartesian(glm::vec3(1.0, north_the, 0.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, north_the, 1.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, north_the, 2.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, north_the, 3.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, north_the, 4.0 * pi / 5.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 0.0 * pi / 5.0 + pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 1.0 * pi / 5.0 + pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 2.0 * pi / 5.0 + pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 3.0 * pi / 5.0 + pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, south_the, 4.0 * pi / 5.0 + pi / 10.0)),
			sphere_to_cartesian(glm::vec3(1.0, pi, 0.0)), // south pole
		};

		glm::vec3 triags[total_vertices] = {
			icosahedron[0], icosahedron[1], icosahedron[2],
			icosahedron[0], icosahedron[2], icosahedron[3],
			icosahedron[0], icosahedron[3], icosahedron[4],
			icosahedron[0], icosahedron[4], icosahedron[5],
			icosahedron[0], icosahedron[5], icosahedron[1],

			icosahedron[1], icosahedron[6], icosahedron[2],
			icosahedron[2], icosahedron[6], icosahedron[7],
			icosahedron[2], icosahedron[7], icosahedron[3],
			icosahedron[3], icosahedron[7], icosahedron[8],
			icosahedron[3], icosahedron[8], icosahedron[4],
			icosahedron[4], icosahedron[8], icosahedron[9],
			icosahedron[4], icosahedron[9], icosahedron[5],
			icosahedron[5], icosahedron[9], icosahedron[10],
			icosahedron[5], icosahedron[10], icosahedron[1],
			icosahedron[1], icosahedron[10], icosahedron[6],

			icosahedron[11], icosahedron[6], icosahedron[7],
			icosahedron[11], icosahedron[7], icosahedron[8],
			icosahedron[11], icosahedron[8], icosahedron[9],
			icosahedron[11], icosahedron[9], icosahedron[10],
			icosahedron[11], icosahedron[10], icosahedron[6],
		};

		mesh_t mesh{};
		glCreateBuffers(1, &mesh.id);
		glNamedBufferStorage(mesh.id, sizeof(triags), triags, 0);
		mesh.vertex_buffer.offset = 0;
		mesh.vertex_buffer.stride = 3 * sizeof(float);
		mesh.vertex_attrib.attrib_index = Vertex0;
		mesh.vertex_attrib.normalized = GL_FALSE;
		mesh.vertex_attrib.relative_offset = 0;
		mesh.vertex_attrib.size = 3;
		mesh.vertex_attrib.type = GL_FLOAT;
		mesh.faces = total_faces;
		mesh.vertices = total_vertices;
		return mesh;
	}

	struct vao_t {
		vao_t() = default;
		vao_t(vao_t&& another) noexcept {
			*this = std::move(another);
		}

		~vao_t() {
			if (id) {
				glDeleteVertexArrays(1, &id);
			}
		}

		vao_t& operator = (vao_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
				std::swap(mode, another.mode);
				std::swap(count, another.count);
			} return *this;
		}

		GLuint id{};
		GLenum mode{};
		int count{};
	};

	vao_t gen_vao_from_mesh(const mesh_t& mesh) {
		vao_t vao{};

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

	struct fbo_attachment_t {
		GLenum attachment{};
		GLenum tex_target{};
		GLuint texture{};
		GLint level{};
	};

	struct framebuffer_t {
		framebuffer_t() = default;
		framebuffer_t(framebuffer_t&& another) noexcept {
			*this = std::move(another);
		}

		~framebuffer_t() {
			if (id) {
				glDeleteFramebuffers(1, &id);	
			}
		}

		framebuffer_t& operator = (framebuffer_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
			} return *this;
		}

		void attach(const fbo_attachment_t& attachment) {
			glNamedFramebufferTexture(id, attachment.attachment, attachment.texture, attachment.level);
		}

		void detach(const fbo_attachment_t& attachment) {
			glNamedFramebufferTexture(id, attachment.attachment, 0, attachment.level);
		}

		bool get_complete_status(GLenum target) {
			return glCheckNamedFramebufferStatus(id, target) == GL_FRAMEBUFFER_COMPLETE;
		}

		GLuint id{};
	};

	framebuffer_t create_framebuffer() {
		framebuffer_t frame{};
		glCreateFramebuffers(1, &frame.id);
		return frame;
	}

	struct uniform_t {
		static constexpr int total_props = 4;
		static constexpr const GLenum all_props[] = {GL_BLOCK_INDEX, GL_TYPE, GL_NAME_LENGTH, GL_LOCATION};

		static uniform_t from_props(const GLint props[total_props]) {
			return uniform_t{
				.block_index = props[0], .type = props[1], .name_length = props[2], .location = props[3],
			};
		}

		static GLint get_props_block_index(const GLint props[total_props]) {
			return props[0];
		}

		GLint block_index{};
		GLint type{};
		GLint name_length{};
		GLint location{};
	};

	struct shader_program_t {
		template<class ... shader_t>
		static shader_program_t create(shader_t&& ... shaders) {
			// TODO
			return {};
		}

		shader_program_t(GLuint _id = 0) : id{_id} {
			GLint active_uniforms{}, uniform_max_name_length{};
			glGetProgramInterfaceiv(id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &active_uniforms);
			glGetProgramInterfaceiv(id, GL_UNIFORM, GL_MAX_NAME_LENGTH, &uniform_max_name_length);

			auto name_buffer = std::make_unique<char[]>(uniform_max_name_length);
			for (GLuint i = 0; i < active_uniforms; i++) {
				GLint props[uniform_t::total_props];
				glGetProgramResourceiv(id, GL_UNIFORM, i,
							uniform_t::total_props, uniform_t::all_props, uniform_t::total_props, nullptr, props);
				if (uniform_t::get_props_block_index(props) != -1) {
					continue;
				}

				glGetProgramResourceName(id, GL_UNIFORM, i, uniform_max_name_length, nullptr, name_buffer.get());
				uniforms[std::string(name_buffer.get())] = uniform_t::from_props(props);
			}
		}

		shader_program_t(shader_program_t&& another) noexcept {
			*this = std::move(another);
		}

		~shader_program_t() {
			if (id) {
				glDeleteProgram(id);
			}
		}

		shader_program_t& operator = (shader_program_t&& another) noexcept {
			if (this != &another) {
				std::swap(id, another.id);
				std::swap(uniforms, another.uniforms);
			} return *this;
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

		GLuint id{};
		std::unordered_map<std::string, uniform_t> uniforms;
	};

	// TODO : basic mesh shader + basic light
}

int main() {
	glfw::guard_t glfw_guard;
	glfw::window_t window(glfw::window_params_t::create_basic_opengl("yin-yang", 1280, 720, 4, 6));

	window.make_ctx_current();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	const char* glsl_version = "#version 460";
	ImGui_ImplGlfw_InitForOpenGL(window.get_handle(), true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	bool show_demo_window = true;
	{
		constexpr int tex_width = 256;
		constexpr int tex_height = 256;

		glew_guard_t glew_guard;
		demo_widget_t demo;
		sim_widget_t sim;
		framebuffer_widget_t framebuffer_widget;

		texture_t tex = gen_test_texture(tex_width, tex_height);
		if (tex.id == 0) {
			return -1;
		}

		while (!window.should_close()) {
			glfw::poll_events();

			glClearColor(1.0, 0.5, 0.25, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			//demo.render();
			//sim.render();
			framebuffer_widget.render(reinterpret_cast<ImTextureID>(tex.id), tex.width, tex.height);

			ImGui::Render();
			auto [w, h] = window.get_framebuffer_size();
			glViewport(0, 0, w, h);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			window.swap_buffers();
		}
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	return 0;
}