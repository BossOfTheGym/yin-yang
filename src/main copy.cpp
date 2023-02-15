#include <bit>
#include <new>
#include <tuple>
#include <cmath>
#include <string>
#include <memory>
#include <random>
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

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>

#include "gen.hpp"
#include "glfw.hpp"
#include "grid.hpp"
#include "field.hpp"
#include "fixed_ds.hpp"
#include "odd_utils.hpp"

class impossible_error : public std::runtime_error {};

struct glew_t {
	glew_t() {
		if (glewInit() != GLEW_OK) {
			throw std::runtime_error("Failed to initialize glew.");
		}
	}
};

std::string get_shader_info_log(GLuint shader) {
	int log_length = {};
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

	GLsizei max_length = log_length;
	GLsizei length = {};

	std::string info_log(max_length, '\0');
	glGetShaderInfoLog(shader, log_length, &length, info_log.data());
	return info_log;
}

bool create_shader(GLuint& shader, GLenum type, const GLchar *source, GLint size) {
	shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, &size);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	return status == GL_TRUE;
}

std::string get_program_info_log(GLuint program) {
	int log_length = {};
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

	GLsizei max_length = log_length;
	GLsizei length = {};

	std::string info_log(max_length, '\0');
	glGetProgramInfoLog(program, log_length, &length, info_log.data());
	return info_log;	
}

template<class ... shader_t>
bool create_program(GLuint& program, shader_t ... shaders) {
	program = glCreateProgram();
	(glAttachShader(program, shaders), ...);
	glLinkProgram(program);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	(glDetachShader(program, shaders), ...);
	return status == GL_TRUE;
}

// still prototype, not enough c++ in this class
struct quad_program_t {
	static inline const std::string vertex_src = R"shader_source(
		#version 460 core

		uniform float u_z; // z-value
		uniform mat3 u_mvp; // model, view, projection
		uniform vec2 u_grid_exts; // width and height
		uniform vec2 u_cell_size; // cell width and cell height
		uniform vec4 u_view_rect; // left, bot, right, top
		uniform float u_grid_param; // offset multiplier (0.0 - no offset, 1.0 - full offset)

		out vec2 quad_uv;

		vec2 get_pos(vec4 rect) {
			switch (gl_VertexID % 4) {
				case 0:
					return u_view_rect.zw;
				case 1:
					return u_view_rect.zy;
				case 2:
					return u_view_rect.xy;
				default:
					return u_view_rect.xw;
			}
		}

		void main() {
			vec2 v = get_pos(u_view_rect);
			vec2 size = u_grid_exts * u_cell_size;
			
			//v = (v * size + size) / 2.0;
			quad_uv = v;
			gl_Position = vec4((u_mvp * vec3(v, 1.0)).xy, u_z, 1.0);
		}
		)shader_source";

	struct alignas(glm::vec4) cell_data_t {
		glm::vec4 color;
		glm::vec4 offset; // only xy are used due to padding(std430)
	};

	static inline const std::string fragment_src = R"shader_source(
		#version 460 core

		#define INF 1e6

		uniform float u_z;
		uniform mat3 u_mvp;
		uniform vec2 u_grid_exts;
		uniform vec2 u_cell_size;
		uniform vec4 u_view_rect;
		uniform float u_grid_param;

		struct cell_data_t {
			vec4 color;
			vec4 offset; // only xy are used due to padding(std430)
		};

		layout(std430, binding = 0) restrict readonly buffer b_cells_buffer {
			cell_data_t b_cells[];
		};

		in vec2 quad_uv;
		out vec4 color;

		// some common stuff
		float length2(vec2 p) {
			return length(p);
		}

		float length1(vec2 p) {
			p = abs(p);
			return max(p.x, p.y);
		}

		#define maxcomp2(v) max(v.x, v.y)
		#define maxcomp3(v) max(max(v.x, v.y), v.z);
		#define mincomp2(v) min(v.x, v.y)
		#define mincomp3(v) min(min(v.x, v.y), v.z);

		// returns values from -1.0 to +1.0
		vec3 hash23(vec2 h) {
			return vec3(cos(sin((0.5 * h.x + 0.5 * h.y) * 123.456) * 179.123),
						sin(cos((0.5 * h.x - 0.5 * h.y) * 456.789) * 123.172),
						cos(sin((0.25 * h.x * h.y) * 316.654) * 397.637));
		}

		struct cell_t {
			vec2 coord; // coord relative to true(unoffsetted) cell center
			vec2 index; // cell index
			vec2 offset; // offset that is applied to cell center
			float dist_to_border; // distance to cell border
		};

		vec2 offset_function(vec2 index, vec2 cell_size) {
			if (int(index.y) % 2 == 1) {
				return vec2(cell_size.x / 2.0, 0.0);
			} return vec2(0.0);
		}

		vec2 cell_offset_impl(vec2 index, vec2 cell_size, float off_mult) {
			vec2 pert = hash23(index).xy * cell_size * 0.1;
			return offset_function(index, cell_size) * off_mult + pert;
		}

		cell_t grid_to_cell_impl(vec2 grid, vec2 cell_size, float off_mult) {
			vec2 coord = mod(grid, cell_size) - cell_size / 2.0;
			vec2 index = floor(grid / cell_size);
			vec2 offset = cell_offset_impl(index, cell_size, off_mult);
			return cell_t(coord, index, offset, INF);
		}

		cell_t cell_neighbour(cell_t cell, vec2 cell_size, float off_mult, vec2 index_off) {
			vec2 coord = cell.coord - index_off * cell_size;
			vec2 index = cell.index + index_off;
			vec2 offset = cell_offset_impl(index, cell_size, off_mult);
			return cell_t(coord, index, offset, INF);
		}

		cell_t grid_to_cell(vec2 grid, vec2 cell_size, float off_mult) {
			const vec2 ij[8] = {
				{-1,  1}, { 0,  1}, { 1,  1},
				{-1,  0},           { 1,  0},
				{-1, -1}, { 0, -1}, { 1, -1},
			};

			cell_t base = grid_to_cell_impl(grid, cell_size, off_mult);

			cell_t cell = base;
			float dist = length2(base.coord - base.offset);
			for (int k = 0; k < 8; k++) {
				cell_t nb = cell_neighbour(base, cell_size, off_mult, ij[k]);
				float d = length2(nb.coord - nb.offset);
				if (d < dist) {
					cell = nb;
					dist = d;
				}
			} for (int k = 0; k < 8; k++) {
				vec2 x = cell.coord;
				vec2 c1 = cell.offset;
				vec2 c2 = ij[k] * cell_size + cell_offset_impl(cell.index + ij[k], cell_size, off_mult);
				vec2 m = (c1 + c2) / 2.0;
				vec2 l = normalize((c1 - c2) / 2.0);
				cell.dist_to_border = min(cell.dist_to_border, dot(x - m, l));
			} return cell;
		}

		vec3 cell_color(cell_t cell, vec2 grid_exts, vec2 cell_size) {
			vec2 index = cell.index;

			//vec3 col = abs(hash23(fract(hash23(cell.index).xz)));
			vec3 col = vec3(0.0);
			float c = 0.0;
			if (all(greaterThanEqual(index, vec2(0.0))) && all(lessThan(index, grid_exts))) {
				int cell_id = int(index.y * grid_exts.x + index.x);
				c = 1.0;
				col = b_cells[cell_id].color.xyz;
			}

			float nd = maxcomp2(cell_size) / 2.0;
			float d = sqrt((1.0 - cos(3 * 3.141592 * cell.dist_to_border / nd)) / 2.0);
			return c * d * col;
		}

		void main() {
			cell_t cell = grid_to_cell(quad_uv, u_cell_size, u_grid_param);
			color = vec4(cell_color(cell, u_grid_exts, u_cell_size), 1.0);
		}
		)shader_source";

	quad_program_t() try {
		GLuint vertex_shader{};
		if (!create_shader(vertex_shader, GL_VERTEX_SHADER, vertex_src.c_str(), vertex_src.size())) {
			throw std::runtime_error(odd::join("failed to create shader: ", get_shader_info_log(vertex_shader)));
		}

		GLuint fragment_shader{};
		if (!create_shader(fragment_shader, GL_FRAGMENT_SHADER, fragment_src.c_str(), fragment_src.size())) {
			throw std::runtime_error(odd::join("failed to create shader: ", get_shader_info_log(fragment_shader)));
		}

		if (!create_program(program, vertex_shader, fragment_shader)) {
			throw std::runtime_error(odd::join("failed to create program: ", get_program_info_log(program)));
		}

		struct {
			GLint* uniform{};
			const char* name{};
		} uniforms[] = {
			{&u_z, "u_z"},
			{&u_mvp, "u_mvp"},
			{&u_grid_exts, "u_grid_exts"},
			{&u_cell_size, "u_cell_size"},
			{&u_view_rect, "u_view_rect"},
			{&u_grid_param, "u_grid_param"},
		};

		for (auto [uniform, name] : uniforms) {
			*uniform = glGetUniformLocation(program, name);
			if (*uniform == -1) {
				throw std::runtime_error(odd::join(CURR_FUNC_NAME, ": failed to obtain ", std::quoted(name), " uniform location."));	
			}
		}

		glDeleteShader(fragment_shader);
		glDeleteShader(vertex_shader);
	} catch (std::runtime_error& err) {
		std::cerr << err.what() << std::endl;
		throw;
	}

	~quad_program_t() {
		glDeleteProgram(program);
	}

	void use() const {
		glUseProgram(program);
	}

	void draw() const {
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}


	// buffer
	void set_cell_buffer(GLuint buffer) const {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	// uniforms
	void set_z(float value) const {
		glProgramUniform1f(program, u_z, value);
	}

	void set_mvp(const glm::mat3& value) const {
		glProgramUniformMatrix3fv(program, u_mvp, 1, GL_FALSE, glm::value_ptr(value));
	}

	void set_grid_exts(const glm::vec2& value) const {
		glProgramUniform2fv(program, u_grid_exts, 1, glm::value_ptr(value));
	}

	void set_cell_size(const glm::vec2& value) const {
		glProgramUniform2fv(program, u_cell_size, 1, glm::value_ptr(value));
	}

	void set_view_rect(const glm::vec4& value) const {
		glProgramUniform4fv(program, u_view_rect, 1, glm::value_ptr(value));
	}

	void set_grid_param(float value) const {
		glProgramUniform1f(program, u_grid_param, value);
	}

	GLuint program{};

	GLint u_z{-1};
	GLint u_mvp{-1};
	GLint u_grid_exts{-1};
	GLint u_cell_size{-1};
	GLint u_view_rect{-1};
	GLint u_grid_param{-1};
};

template<class __data_t>
struct mapped_buffer_t {
	using data_t = __data_t;

	mapped_buffer_t(GLsizei __cap) : cap(__cap), sz(sizeof(data_t) * cap) {
		GLbitfield flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		glCreateBuffers(1, &buffer);
		glNamedBufferStorage(buffer, sz, NULL, flags);
		data = (data_t*)glMapNamedBufferRange(buffer, 0, sz, flags);
	}

	~mapped_buffer_t() {
		glUnmapNamedBuffer(buffer);
		glDeleteBuffers(1, &buffer);
	}

	GLuint buffer{};
	GLsizei cap{};
	GLsizei sz{};
	data_t* data{};
};

struct dummy_vao_t {
	dummy_vao_t() {
		glCreateVertexArrays(1, &vao);
	}

	~dummy_vao_t() {
		glDeleteVertexArrays(1, &vao);		
	}

	void bind() {
		glBindVertexArray(vao);
	}

	GLuint vao;
};


enum grid_mode_t : int {
	Quad,
	Hex,
	MaxModes,
};

using cell_buffer_t = mapped_buffer_t<quad_program_t::cell_data_t>;

using cell_t = std::uint32_t;
const constexpr cell_t cell_empty_v = 0;
const constexpr cell_t cell_occupied_v = 1;

using point_t = glm::ivec3;
using field_t = field::field_t<point_t, cell_t, cell_empty_v>;

using rot_t = grid::rot_t<point_t>;
using transform_t = grid::transform_t<point_t>;

using data_t = std::vector<cell_t>;
using figure_t = std::vector<point_t>;

figure_t create_test_figure() {
	return figure_t{
		point_t{1, 1, 0},
		point_t{0, 0, 0}, point_t{0, 1, 0}, point_t{0, 2, 0},
	};
}


struct controller_ctx_t {
	grid_mode_t& mode;
	figure_t& figure;
	transform_t& t_prev;
	transform_t& t_curr;
	transform_t& t_next;
	field_t& field;
	bool toggle{};
};

struct test_controller_t : public controller_ctx_t {
	bool can_move(const transform_t& t) {
		switch (mode) {
			case Quad: {
				for (auto& p : figure) {
					if (!field.can_move_point(grid::transform4(t, p))) {
						return false;
					}
				} return true;
			} case Hex: {
				for (auto& p : figure) {
					if (!field.can_move_point(grid::axial_to_quad<point_t>(grid::transform6(t, p)))) {
						return false;
					}
				} return true;
			}
		} return false;
	}

	bool can_place(const transform_t& t) {
		switch (mode) {
			case Quad: {
				for (auto& p : figure) {
					if (!field.can_place_point(grid::transform4(t, p))) {
						return false;
					}
				} return true;
			} case Hex: {
				for (auto& p : figure) {
					if (!field.can_place_point(grid::axial_to_quad<point_t>(grid::transform6(t, p)))) {
						return false;
					}
				} return true;
			}
		} return false;
	}

	void place_figure(const transform_t& t, cell_t value) {
		switch (mode) {
			case Quad: {
				for (auto& p : figure) {
					field.place_point(grid::transform4(t, p), value);
				} break;
			} case Hex: {
				for (auto& p : figure) {
					field.place_point(grid::axial_to_quad<point_t>(grid::transform6(t, p)), value);
				} break;
			}
		}
	}

	void verify_move() {
		if (!can_move(t_next)) {
			return;
		}
		t_prev = t_curr;
		t_curr = t_next;
	}

	point_t get_base_off(int w, int h, const point_t& l, const point_t& b, const point_t& pivot) {
		return point_t{std::max(w / 2 - pivot.x, pivot.x - l.x), h + 1 - (pivot.y - b.y), 0.0};
	}


	void move_up(int value) {
		place_figure(t_curr, cell_empty_v);
		switch (mode) {
			case Quad: {
				t_next = t_curr;
				t_next.t = grid::move_up4(t_next.t, value);
				break;
			} case Hex: {
				t_next = t_curr;
				int way = t_next.t.y % 2;
				if (value < 0) {
					way ^= 1;
				}
				t_next.t = grid::move_up6(t_next.t, value, (grid::updown_way_t)way);
				break;
			}
		}
		verify_move();
		place_figure(t_curr, cell_occupied_v);
	}

	void move_down(int value) {
		move_up(-value);
	}

	void move_left(int value) {
		place_figure(t_curr, cell_empty_v);
		switch (mode) {
			case Quad: {
				t_next = t_curr;
				t_next.t = grid::move_left4(t_next.t, value);
				break;
			} case Hex: {
				t_next = t_curr;
				t_next.t = grid::move_left6(t_next.t, value);
				break;
			}
		}
		verify_move();
		place_figure(t_curr, cell_occupied_v);
	}

	void move_right(int value) {
		move_left(-value);
	}

	void rotate_cw(int value) {
		place_figure(t_curr, cell_empty_v);
		t_next = t_curr;
		t_next.r.rotation -= value;
		verify_move();
		place_figure(t_curr, cell_occupied_v);
	}

	void rotate_ccw(int value) {
		rotate_cw(-value);
	}

	void fix_figure() {
		place_figure(t_curr, cell_empty_v);
		if (can_place(t_curr)) {
			place_figure(t_curr, cell_occupied_v);
			reset_figure(false);
		} else {
			place_figure(t_curr, cell_occupied_v);
		}
	}

	// workaround
	void move_figure() {
		place_figure(t_curr, cell_empty_v);
		place_figure(t_curr, cell_occupied_v);
	}

	void reset_figure(bool reset_curr = true) {
		int w = field.get_width();
		int h = field.get_height();

		if (reset_curr) {
			place_figure(t_curr, cell_empty_v);
		} switch (mode) {
			case Quad: {
				auto [l, r] = std::minmax_element(figure.begin(), figure.end(), [&] (auto& a, auto& b) { return a.x < b.x; });
				auto [b, t] = std::minmax_element(figure.begin(), figure.end(), [&] (auto& a, auto& b) { return a.y < b.y; });
				t_curr.t = get_base_off(w, h, *l, *b, t_curr.r.point);
				t_curr.r.rotation = 0;
				t_prev = t_curr;
				break;
			} case Hex: {
				auto a2q = [] (const auto& p) { return grid::axial_to_quad<point_t>(p); };
				auto q2a = [] (const auto& p) { return grid::quad_to_axial<point_t>(p); };
				auto [l, r] = std::minmax_element(figure.begin(), figure.end(), [&] (auto& a, auto& b) { return a2q(a).x < a2q(b).x; });
				auto [b, t] = std::minmax_element(figure.begin(), figure.end(), [&] (auto& a, auto& b) { return a2q(a).y < a2q(b).y; });
				t_curr.t = q2a(get_base_off(w, h, a2q(*l), a2q(*b), a2q(t_curr.r.point)));
				t_curr.r.rotation = 0;
				t_prev = t_curr;
				break;
			}
		} place_figure(t_curr, cell_occupied_v);
	}

	void reset_field() {
		field.reset();
		place_figure(t_curr, cell_occupied_v);
	}

	void shift_phase() {
		switch (mode) {
			case Quad: {
				figure_t fig_t;
				for (auto& p : figure) {
					fig_t.push_back(grid::quad_to_axial<point_t>(grid::transform4(t_curr, p)));
				} if (!grid::connected6(fig_t)) {
					break;
				}

				point_t pivot = grid::quad_to_axial<point_t>(grid::transform4(t_curr, t_curr.r.point));
				for (auto& p : fig_t) {
					p -= pivot;
				}

				mode = Hex;
				figure = fig_t;
				t_curr = {{point_t{0}, grid::rot4_to_rot6_index<point_t>(t_curr.r), 0}, pivot};
				t_prev = t_curr;
				break;
			} case Hex: {
				figure_t fig_t;
				for (auto& p : figure) {
					fig_t.push_back(grid::axial_to_quad<point_t>(grid::transform6(t_curr, p)));
				} if (!grid::connected4(fig_t)) {
					break;
				}

				point_t pivot = grid::axial_to_quad<point_t>(grid::transform6(t_curr, t_curr.r.point));
				for (auto& p : fig_t) {
					p -= pivot;
				}

				mode = Quad;
				figure = fig_t;
				t_curr = {{point_t{0}, grid::rot6_to_rot4_index<point_t>(t_curr.r), 0}, pivot};
				t_prev = t_curr;
				break;
			}
		}
	}
};

struct game_ctx_t {
	inline static const int WINDOW_WIDTH = 400;
	inline static const int WINDOW_HEIGHT = 400;
	inline static const int FIELD_WIDTH = 10;
	inline static const int FIELD_HEIGHT = 10;

	game_ctx_t() {
		glfw_guard = std::make_unique<glfw::guard_t>();
		window = std::make_unique<glfw::window_t>(WINDOW_WIDTH, WINDOW_HEIGHT);
		input = std::make_unique<glfw::input_t<>>();
	}

	~game_ctx_t() {}

	void attach_ev_handlers() {
		window->set_key_ev_handler([&] (int key, int scancode, int action, int mods) {
			// whatever
		});

		window->set_mouse_button_ev_handler([&] (int button, int action, int mods) {
			// whatever
		});

		window->set_cursor_pos_ev_handler([&] (double x, double y) {
			// whatever;
		});

		window->set_cursor_enter_ev_handler([&] (bool entered) {
			// whatever
		});

		window->set_scroll_ev_handler([&] (double dx, double dy) {
			// whatever
		});

		window->set_framebuffer_size_ev_handler([&] (int width, int height) {
			// whatever
		});

		window->set_window_size_ev_handler([&] (int width, int height) {
			// whatever
		});
	}

	void detach_ev_handlers() {
		window->reset_window_size_ev_handler();
		window->reset_framebuffer_size_ev_handler();
		window->reset_scroll_ev_handler();
		window->reset_cursor_enter_ev_handler();
		window->reset_cursor_pos_ev_handler();
		window->reset_mouse_button_ev_handler();
		window->reset_key_ev_handler();
	}

	void handle_input() {
		if (input->was_key_pressed(glfw::KeyW)) {
			controller->move_up(1);
		} if (input->was_key_pressed(glfw::KeyA)) {
			controller->move_left(1);
		} if (input->was_key_pressed(glfw::KeyS)) {
			controller->move_down(1);
		} if (input->was_key_pressed(glfw::KeyD)) {
			controller->move_right(1);
		} if (input->was_key_pressed(glfw::KeyQ)) {
			controller->rotate_ccw(1);
		} if (input->was_key_pressed(glfw::KeyE)) {
			controller->rotate_cw(1);
		} if (input->was_key_pressed(glfw::KeyR)) {
			controller->reset_field();
		} if (input->was_key_pressed(glfw::KeyF)) {
			controller->fix_figure();
		} if (input->was_key_pressed(glfw::KeySpace)) {
			controller->shift_phase();
		}
	}

	void draw_field() {
		glm::vec4 test_colors[] = {glm::vec4{0.2, 0.2, 0.2, 1.0}, glm::vec4{1.0, 0.0, 0.0, 1.0}};
		int Y = field->get_height();
		int X = field->get_width();
		for (int y = 0; y < Y; y++) {
			for (int x = 0; x < X; x++) {
				glm::vec4 color = test_colors[field->cell({x, y, 0})];
				cell_buffer->data[y * X + x] = {color};
			}
		}

		float z = 1.0;

		float phase = 0.0;
		switch (grid_mode) {
			case Quad: {
				phase = 0.0;
				break;
			} case Hex: {
				phase = 1.0;
				break;
			}
		}

		auto [w, h] = window->get_window_size();
		float q = std::min((float)w / (field_width + 1), (float)h / (field_height + 1));
		float sq3d2 = std::sqrt(3.0) / 2.0;
		glm::vec2 cell = glm::vec2{q,  glm::mix(q, q * sq3d2, phase)};
		glm::vec2 half = cell / 2.0f;
		glm::vec2 size = glm::vec2{field_width * cell.x, field_height * cell.y};
		glm::vec2 c = glm::vec2{field_width * cell.x / 2.0f, field_height * cell.y / 2.0f};
		glm::vec4 view_rect = glm::vec4{-half.x, -half.y, size.x + half.x, size.y + half.y};
		glm::vec2 exts = glm::vec2{field_width, field_height};

		glm::mat3 m = glm::mat3{
			1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			0.0, 0.0, 1.0,
		};
		glm::mat3 v = glm::mat3{
			1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			-c.x, -c.y, 1.0,
		};
		glm::mat3 p = glm::mat3{
			2.0 / w, 0.0, 0.0,
			0.0, 2.0 / h, 0.0,
			0.0, 0.0, 1.0,
		};
		glm::mat3 mvp = p * v * m;

		// glm::vec4 colors[] = {
		// 	glm::vec4(204,  17, 240, 255) / 255.0f,
		// 	glm::vec4( 99,   0, 255, 255) / 255.0f,
		// 	glm::vec4(255,   0, 141, 255) / 255.0f,
		// 	glm::vec4(209,  78, 234, 255) / 255.0f,
		// 	glm::vec4(249,  99,  99, 255) / 255.0f,
		// };
		// for (int i = 0; i < cells_total; i++) {
		// 	cell_buffer->data[i] = {colors[i % std::size(colors)]};
		// }

		dummy_vao->bind();
		quad_program->use();
		quad_program->set_cell_buffer(cell_buffer->buffer);
		quad_program->set_z(z);
		quad_program->set_mvp(mvp);
		quad_program->set_grid_exts(exts);
		quad_program->set_cell_size(cell);
		quad_program->set_view_rect(view_rect);
		quad_program->set_grid_param(phase);
		quad_program->draw();
	}

	void init_shit() {
		field_width = FIELD_WIDTH;
		field_height = FIELD_HEIGHT;
		cells_total = field_width * field_height;
		
		input = std::make_unique<glfw::input_t<>>();
		glew = std::make_unique<glew_t>();
		quad_program = std::make_unique<quad_program_t>();
		dummy_vao = std::make_unique<dummy_vao_t>();
		cell_buffer = std::make_unique<cell_buffer_t>(cells_total);

		field = std::make_unique<field_t>(field_width, field_height);

		grid_mode = Quad;
		
		figure = create_test_figure();

		auto pivot = grid::fig4_pivot(figure);
		std::tie(t_curr.r.point, t_curr.r.index) = pivot;

		controller = odd::make_unique<test_controller_t>(grid_mode, figure, t_prev, t_curr, t_next, *field);
		controller->reset_figure();

		attach_ev_handlers();
	}

	void deinit_shit() {
		detach_ev_handlers();

		controller.reset();
		figure.clear();

		field.reset();

		cell_buffer.reset();
		dummy_vao.reset();
		quad_program.reset();
		glew.reset();
		input.reset();
	}

	void mainloop() {
		window->make_ctx_current();

		init_shit();
		while (!window->should_close()) {
			glfw::poll_events();

			t = glfw::get_time();
			dt = t - t0;

			input->update(*window);

			handle_input();

			// draw figure
			glClearColor(1.0, 1.0, 1.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			auto [w, h] = window->get_framebuffer_size();
			glViewport(0, 0, w, h);

			draw_field();

			window->swap_buffers();

			t0 = t;
		}
		deinit_shit();
	}

	int field_width{};
	int field_height{};
	int cells_total{};

	double t0{};
	double t{};
	double dt{};

	grid_mode_t grid_mode{};
	figure_t figure{};
	transform_t t_prev{}, t_curr{}, t_next{}; 
	std::unique_ptr<field_t> field{};
	std::unique_ptr<test_controller_t> controller{};

	std::unique_ptr<glfw::guard_t> glfw_guard;
	std::unique_ptr<glfw::window_t> window;
	std::unique_ptr<glfw::input_t<>> input;
	std::unique_ptr<glew_t> glew;
	
	std::unique_ptr<dummy_vao_t> dummy_vao;
	std::unique_ptr<quad_program_t> quad_program;
	std::unique_ptr<cell_buffer_t> cell_buffer;
};

int main(int argc, char* argv[]) {
	std::unique_ptr<game_ctx_t> game_ctx = std::make_unique<game_ctx_t>();
	game_ctx->mainloop();
	return 0;
}