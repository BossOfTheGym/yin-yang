#include "glfw.hpp"

#include <exception>
#include <stdexcept>

namespace glfw {
	guard_t::guard_t() {
		if (!glfwInit()) {
			throw std::runtime_error("Failed to initialize glfw.");
		}
	}

	guard_t::~guard_t() {
		glfwTerminate();
	}


	void poll_events() {
		glfwPollEvents();
	}

	double get_time() {
		return glfwGetTime();
	}

	void reset_time() {
		glfwSetTime(0.0);
	}


	window_params_t window_params_t::create_basic_opengl(std::string title, int width, int height, int major, int minor) {
		window_params_t params(std::move(title), width, height);
		params.set_hint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
		params.set_hint(GLFW_CENTER_CURSOR, GLFW_TRUE);
		params.set_hint(GLFW_CLIENT_API, GLFW_OPENGL_API);
		params.set_hint(GLFW_CONTEXT_VERSION_MAJOR, major);
		params.set_hint(GLFW_CONTEXT_VERSION_MINOR, minor);
		return params;
	}

	void window_t::key_cb(GLFWwindow* window, int key, int scancode, int action, int mods) {
		window_t* wnd = (window_t*)glfwGetWindowUserPointer(window);
		if (wnd->key_ev_handler) {
			wnd->key_ev_handler(key, scancode, action, mods);
		}
	} 

	void window_t::reset_key_ev_handler() {
		key_ev_handler = key_ev_handler_t();
	}


	void window_t::mouse_button_cb(GLFWwindow* window, int button, int action, int mods) {
		window_t* wnd = (window_t*)glfwGetWindowUserPointer(window);
		if (wnd->mouse_button_ev_handler) {
			wnd->mouse_button_ev_handler(button, action, mods);
		}
	}

	void window_t::reset_mouse_button_ev_handler() {
		mouse_button_ev_handler = mouse_button_ev_handler_t();
	}


	void window_t::cursor_pos_cb(GLFWwindow* window, double x, double y) {
		window_t* wnd = (window_t*)glfwGetWindowUserPointer(window);
		if (wnd->cursor_pos_ev_handler) {
			wnd->cursor_pos_ev_handler(x, y);
		}
	}

	void window_t::reset_cursor_pos_ev_handler() {
		cursor_pos_ev_handler = cursor_pos_ev_handler_t();
	}


	void window_t::cursor_enter_cb(GLFWwindow* window, int entered) {
		window_t* wnd = (window_t*)glfwGetWindowUserPointer(window);
		if (wnd->cursor_enter_ev_handler) {
			wnd->cursor_enter_ev_handler(entered);
		}
	}

	void window_t::reset_cursor_enter_ev_handler() {
		cursor_enter_ev_handler = cursor_enter_ev_handler_t();
	}


	void window_t::scroll_cb(GLFWwindow* window, double dx, double dy) {
		window_t* wnd = (window_t*)glfwGetWindowUserPointer(window);
		if (wnd->scroll_ev_handler) {
			wnd->scroll_ev_handler(dx, dy);
		}
	}

	void window_t::reset_scroll_ev_handler() {
		scroll_ev_handler = scroll_ev_handler_t();
	}


	void window_t::framebuffer_size_cb(GLFWwindow* window, int width, int height) {
		window_t* wnd = (window_t*)glfwGetWindowUserPointer(window);
		if (wnd->framebuffer_size_ev_handler) {
			wnd->framebuffer_size_ev_handler(width, height);
		}
	}

	void window_t::reset_framebuffer_size_ev_handler() {
		framebuffer_size_ev_handler = framebuffer_size_ev_handler_t();
	}


	void window_t::window_size_cb(GLFWwindow* window, int width, int height) {
		window_t* wnd = (window_t*)glfwGetWindowUserPointer(window);
		if (wnd->window_size_ev_handler) {
			wnd->window_size_ev_handler(width, height);
		}
	}

	void window_t::reset_window_size_ev_handler() {
		window_size_ev_handler = window_size_ev_handler_t();
	}


	void window_t::register_cbs() {
		glfwSetKeyCallback(window, key_cb);
		glfwSetMouseButtonCallback(window, mouse_button_cb);
		glfwSetCursorPosCallback(window, cursor_pos_cb);
		glfwSetCursorEnterCallback(window, cursor_enter_cb);
		glfwSetScrollCallback(window, scroll_cb);
		glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);
		glfwSetWindowSizeCallback(window, window_size_cb);
	}
		
	void window_t::set_usr_ptr(void* usr_ptr) {
		glfwSetWindowUserPointer(window, usr_ptr);
	}


	window_t::window_t(const window_params_t& params) {
		for (auto [hint, value] : params.get_hints()) {
			glfwWindowHint(hint, value);
		}
		window = glfwCreateWindow(params.get_width(), params.get_height(), params.get_title().c_str(), nullptr, nullptr);
		if (!window) {
			throw std::runtime_error("Failed to create window");
		}
		set_usr_ptr(this);
		register_cbs();
	}

	window_t::~window_t() {
		glfwDestroyWindow(window);
	}


	void window_t::make_ctx_current() {
		glfwMakeContextCurrent(window);
	}

	void window_t::swap_buffers() {
		glfwSwapBuffers(window);
	}

	bool window_t::should_close() const {
		return glfwWindowShouldClose(window);
	}


	exts_t window_t::get_framebuffer_size() const {
		exts_t sz{};
		glfwGetFramebufferSize(window, &sz.width, &sz.height);
		return sz;
	}

	exts_t window_t::get_window_size() const {
		exts_t sz{};
		glfwGetWindowSize(window, &sz.width, &sz.height);
		sz.width = sz.width != 0 ? sz.width : 1;
		sz.height = sz.height != 0 ? sz.height : 1;
		return sz;
	}

	input_key_state_t window_t::get_key_state(input_key_t key) const {
		return (input_key_state_t)glfwGetKey(window, key);
	}

	bool window_t::is_key_pressed(input_key_t key) const {
		return get_key_state(key) == input_key_state_t::Pressed;
	}

	bool window_t::is_key_released(input_key_t key) const {
		return get_key_state(key) == input_key_state_t::Released;
	}


	input_key_state_t window_t::get_mouse_button_state(input_key_t key) const {
		return (input_key_state_t)glfwGetMouseButton(window, key);
	}

	bool window_t::is_mouse_button_pressed(input_key_t key) const {
		return get_mouse_button_state(key) == input_key_state_t::Pressed;
	}

	bool window_t::is_mouse_button_released(input_key_t key) const {
		return get_mouse_button_state(key) == input_key_state_t::Released;
	}
}