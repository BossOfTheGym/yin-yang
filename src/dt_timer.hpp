#pragma once

#include <utility>
#include <functional>

template<class __value_t>
struct dt_timer_t {
	using value_t = __value_t;

	using timeout_ev_handler_t = std::function<void()>;

	dt_timer_t(value_t tick_value = value_t(1)) : tick(tick_value) {}

	void update(value_t dt) {
		if (delay > value_t(0)) {
			value_t cut = std::min(delay, dt);
			delay -= cut;
			dt -= cut;
		}

		t += dt;
		while (t >= tick) {
			t -= tick;
			if (timeout) {
				timeout();
			}
		}
	}

	void set_tick(value_t value) {
		tick = value;
	}

	void set_delay(value_t value) {
		delay = value;
	}

	void set_time(value_t value) {
		t = value;
	}

	void reset(value_t tick_value) {
		t = value_t(0);
		delay = value_t(0);
		tick = tick_value;
	}


	template<class handler_t>
	void set_timeout_handler(handler_t&& handler) {
		timeout = std::forward<handler_t>(handler);
	}

	void reset_timeout_handler() {
		timeout = timeout_ev_handler_t();
	}

	timeout_ev_handler_t timeout;


	value_t delay{};
	value_t t{};
	value_t tick{};
};