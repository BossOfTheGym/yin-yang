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

int main() {
	glfw::guard_t glfw_guard;
	glfw::window_t window(800, 400);

	window.make_ctx_current(); {
		glew_guard_t glew_guard;
		while (!window.should_close()) {
			glfw::poll_events();

			glClearColor(1.0, 0.5, 0.25, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			window.swap_buffers();
		}
	}

	return 0;
}