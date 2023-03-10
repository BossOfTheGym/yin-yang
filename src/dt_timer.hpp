#pragma once

#include <utility>
#include <functional>

using timeout_action_t = std::function<void()>;
using timeout_handler_t = std::function<timeout_action_t()>;

// TODO : rework
template<class __value_t>
class dt_timer_t {
public:
	using value_t = __value_t;

	template<class handler_t>
	dt_timer_t(handler_t&& handler, value_t _delay, value_t _tick, bool _single_shot)
		: timeout{std::forward<timeout_t>(handler)}, delay{_delay}, tick{_tick}, single_shot{_single_shot} {}

	timeout_action_t update(value_t dt) {
		if (fired) {
			return {};
		}

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
			} if (single_shot) {
				fired = true;
				break;
			}
		} return {};
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
		fired = false;
	}

	template<class handler_t>
	void set_timeout_handler(handler_t&& handler) {
		timeout = std::forward<handler_t>(handler);
	}

	void reset_timeout_handler() {
		timeout = timeout_handler_t();
	}

private:
	timeout_handler_t timeout;
	value_t delay{};
	value_t t{};
	value_t tick{};
	bool single_shot{};
	bool fired{};
};