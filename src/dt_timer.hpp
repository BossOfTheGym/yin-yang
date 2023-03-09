#pragma once

#include <utility>
#include <functional>

template<class __value_t>
class dt_timer_t {
public:
	using value_t = __value_t;

	using timeout_ev_handler_t = std::function<void()>;

	template<class handler_t>
	dt_timer_t(value_t _tick, value_t _delay, handler_t&& handler)
		: tick{_tick}, delay{_delay}, timeout{std::forward<timeout_t>(handler)} {}

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

private:
	timeout_ev_handler_t timeout;

	value_t delay{};
	value_t t{};
	value_t tick{};
};