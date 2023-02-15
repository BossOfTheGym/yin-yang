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
			id = std::exchange(another.id, 0);
			width = std::exchange(another.width, 0);
			height = std::exchange(another.height, 0);
		} return *this;
	}

	GLuint id{};
	int width{};
	int height{};
};

texture_t create_test_texture(int width, int height) {
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
	if (tex.id == 0) {
		return texture_t{};
	}
	glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(tex.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(tex.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureStorage2D(tex.id, 1, GL_RG32F, width, height);
	glTextureSubImage2D(tex.id, 0, 0, 0, width, height, GL_RG, GL_FLOAT, tex_data.data());
	return tex;
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

		texture_t tex = create_test_texture(tex_width, tex_height);
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
			framebuffer_widget.render((ImTextureID)tex.id, tex.width, tex.height);

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