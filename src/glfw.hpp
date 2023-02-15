#pragma once

#include <string>
#include <vector>
#include <cassert>
#include <utility>
#include <functional>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#undef GLFW_INCLUDE_NONE

namespace glfw {
	// lib initialization guard
	struct guard_t {
		guard_t();
		~guard_t();
	};

	// some glfw3 function wrappers 
	void poll_events();
	double get_time();
	void reset_time();

	// enums
	enum input_key_state_t : int {
		Pressed = GLFW_PRESS,
		Released = GLFW_RELEASE,
	};

	enum input_key_t : int {
		KeyW = GLFW_KEY_W,
		KeyA = GLFW_KEY_A,
		KeyS = GLFW_KEY_S,
		KeyD = GLFW_KEY_D,
		KeyQ = GLFW_KEY_Q,
		KeyE = GLFW_KEY_E,
		KeyR = GLFW_KEY_R,
		KeyF = GLFW_KEY_F,

		KeyUp = GLFW_KEY_UP,
		KeyLeft = GLFW_KEY_LEFT,
		KeyDown = GLFW_KEY_DOWN,
		KeyRight = GLFW_KEY_RIGHT,

		KeySpace = GLFW_KEY_SPACE,

		MouseButtonLeft = GLFW_MOUSE_BUTTON_LEFT,
		MouseButtonRight = GLFW_MOUSE_BUTTON_RIGHT,
		MouseButtonMiddle = GLFW_MOUSE_BUTTON_MIDDLE,
	};

	inline const input_key_t keyboard_v[] = {KeyW, KeyA, KeyS, KeyD, KeyQ, KeyE, KeyR, KeyF, KeyUp, KeyLeft, KeyDown, KeyRight, KeySpace};
	inline const input_key_t mouse_v[] = {MouseButtonLeft, MouseButtonRight, MouseButtonMiddle};

	// window
	struct int_hint_t {
		int hint{};
		int vlaue{};
	};

	class window_params_t {
	public:
		static window_params_t create_basic_opengl(std::string title, int width, int height, int major, int minor);

	public:
		inline window_params_t(std::string _title, int _width, int _height)
			: title(std::move(title)), width{_width}, height{_height} {}

		inline void set_hint(int hint, int value) {
			hints.push_back({hint, value});
		}

		inline void set_title(std::string value) {
			title = std::move(value);
		}

		inline void set_width(int value) {
			width = value;
		}

		inline void set_height(int value) {
			height = value;
		}

		inline const std::vector<int_hint_t>& get_hints() const {
			return hints;
		}

		inline const std::string& get_title() const {
			return title;
		}

		inline int get_width() const {
			return width;
		}

		inline int get_height() const {
			return height;
		}

	private:
		std::vector<int_hint_t> hints;
		std::string title;
		int width{};
		int height{};
	};

	struct exts_t {
		int width{};
		int height{};
	};

	class window_t {
	public:
		using key_ev_handler_t = std::function<void(int key, int scancode, int action, int mods)>;
	private:
		static void key_cb(GLFWwindow* window, int key, int scancode, int action, int mods);
	public:
		template<class handler_t>
		void set_key_ev_handler(handler_t&& handler) {
			key_ev_handler = std::forward<handler_t>(handler);
		}

		void reset_key_ev_handler();
	private:
		key_ev_handler_t key_ev_handler;

	public:
		using mouse_button_ev_handler_t = std::function<void(int button, int action, int mods)>;
	private:
		static void mouse_button_cb(GLFWwindow* window, int button, int action, int mods);
	public:
		template<class handler_t>
		void set_mouse_button_ev_handler(handler_t&& handler) {
			mouse_button_ev_handler = std::forward<handler_t>(handler);
		}

		void reset_mouse_button_ev_handler();
	private:
		mouse_button_ev_handler_t mouse_button_ev_handler;

	public:
		using cursor_pos_ev_handler_t = std::function<void(double x, double y)>;
	private:
		static void cursor_pos_cb(GLFWwindow* window, double x, double y);
	public:
		template<class handler_t>
		void set_cursor_pos_ev_handler(handler_t&& handler) {
			cursor_pos_ev_handler = std::forward<handler_t>(handler);
		}
	
		void reset_cursor_pos_ev_handler();
	private:
		cursor_pos_ev_handler_t cursor_pos_ev_handler;

	public:
		using cursor_enter_ev_handler_t = std::function<void(bool entered)>; // bool instead of int
	private:
		static void cursor_enter_cb(GLFWwindow* window, int entered);
	public:
		template<class handler_t>
		void set_cursor_enter_ev_handler(handler_t && handler) {
			cursor_enter_ev_handler = std::forward<handler_t>(handler);
		}

		void reset_cursor_enter_ev_handler();
	private:
		cursor_enter_ev_handler_t cursor_enter_ev_handler;

	public:
		using scroll_ev_handler_t = std::function<void(double dx, double dy)>;
	private:
		static void scroll_cb(GLFWwindow* window, double dx, double dy);
	public:
		template<class handler_t>
		void set_scroll_ev_handler(handler_t&& handler) {
			scroll_ev_handler = std::forward<handler_t>(handler);
		}

		void reset_scroll_ev_handler();
	private:
		scroll_ev_handler_t scroll_ev_handler;

	public:
		using framebuffer_size_ev_handler_t = std::function<void(int width, int height)>;
	private:
		static void framebuffer_size_cb(GLFWwindow* window, int width, int height);
	public:
		template<class handler_t>
		void set_framebuffer_size_ev_handler(handler_t&& handler) {
			framebuffer_size_ev_handler = std::forward<handler_t>(handler);
		}

		void reset_framebuffer_size_ev_handler();
	private:
		framebuffer_size_ev_handler_t framebuffer_size_ev_handler;

	public:
		using window_size_ev_handler_t = std::function<void(int width, int height)>;
	private:
		static void window_size_cb(GLFWwindow* window, int width, int height);
	public:
		template<class handler_t>
		void set_window_size_ev_handler(handler_t&& handler) {
			window_size_ev_handler = std::forward<handler_t>(handler);
		}

		void reset_window_size_ev_handler();
	private:
		window_size_ev_handler_t window_size_ev_handler;

	private:
		void register_cbs();
		void set_usr_ptr(void* usr_ptr);
		
	public:
		window_t(const window_params_t& params);
		~window_t();

		void make_ctx_current();
		void swap_buffers();

		bool should_close() const;

		exts_t get_framebuffer_size() const;
		exts_t get_window_size() const;
		
		input_key_state_t get_key_state(input_key_t key) const;
		bool is_key_pressed(input_key_t key) const;
		bool is_key_released(input_key_t key) const;

		input_key_state_t get_mouse_button_state(input_key_t key) const;
		bool is_mouse_button_pressed(input_key_t key) const;
		bool is_mouse_button_released(input_key_t key) const;

		inline GLFWwindow* get_handle() const {
			return window;
		}

	private:
		GLFWwindow* window{};
	};

	// input
	template<size_t __N, size_t __S>
	struct input_state_t {
		inline static constexpr size_t N = __N;
		inline static constexpr size_t S = __S;

		static_assert(S >= 2, "count of states must be greater or equal than 2");

		input_state_t() {
			reset();
		}

		void update(input_key_t key, input_key_state_t value) {
			assert(key >= 0 && key < N);
			auto& states = state_history[key];
			for (int i = 0; i < S - 1; i++) {
				states[i] = states[i + 1];
			} states[S - 1] = value;
		}

		void reset(input_key_state_t value = input_key_state_t::Released) {
			for (auto& states : state_history) {
				for (auto& state : states) {
					state = value;
				}
			}	
		}

		bool was_pressed(input_key_t key) const {
			assert(key >= 0 && key < N);
			auto& states = state_history[key];
			return states[S - 2] == input_key_state_t::Released && states[S - 1] == input_key_state_t::Pressed;
		}

		bool pressed(input_key_t key) const {
			assert(key >= 0 && key < N);
			auto& states = state_history[key];
			return states[S - 1] == input_key_state_t::Pressed;
		}

		bool was_released(input_key_t key) const {
			assert(key >= 0 && key < N);
			auto& states = state_history[key];
			return states[S - 2] == input_key_state_t::Pressed && states[S - 1] == input_key_state_t::Released;
		}

		bool released(input_key_t key) const {
			assert(key >= 0 && key < N);
			auto& states = state_history[key];
			return states[S - 1] == input_key_state_t::Released;
		}

		input_key_state_t state_history[N][S] = {};
	};

	template<size_t __S = 2>
	struct input_t {
		inline static constexpr size_t S = __S;

		using key_input_t = input_state_t<512, S>;
		using mouse_input_t = input_state_t<3, S>;

		void update(const window_t& window) {
			for (auto key : keyboard_v) {
				key_input.update(key, window.get_key_state(key));
			} for (auto key : mouse_v) {
				mouse_input.update(key, window.get_mouse_button_state(key));
			}
		}

		void reset(input_key_state_t value) {
			key_input.reset(value);
			mouse_input.reset(value);
		}

		bool was_key_pressed(input_key_t key) const {
			return key_input.was_pressed(key);
		}

		bool key_pressed(input_key_t key) const {
			return key_input.pressed(key);
		}

		bool was_key_released(input_key_t key) const {
			return key_input.was_released(key);
		}

		bool key_released(input_key_t key) const {
			return key_input.released(key);
		}

		bool was_mouse_button_pressed(input_key_t key) const {
			return mouse_input.was_pressed(key);
		}

		bool mouse_button_pressed(input_key_t key) const {
			return mouse_input.pressed(key);
		}

		bool was_mouse_button_released(input_key_t key) const {
			return mouse_input.was_released(key);
		}

		bool mouse_button_released(input_key_t key) const {
			return mouse_input.released(key);
		}

		key_input_t key_input;
		mouse_input_t mouse_input;
	};
}