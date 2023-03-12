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

	// dt = 0 => update finished
	timeout_action_t update(value_t& dt) {
		if (fired) {
			dt = value_t(0);
			return {};
		}

		value_t cut = std::min(delay, dt);
		delay -= cut;
		dt -= cut;
		if (delay <= value_t(0)) {
			if (single_shot) {
				fired = true;
			} if (timeout) {
				return timeout();
			} delay = tick;
		} return {};
	}

	void set_tick(value_t value) {
		tick = value;
	}

	void set_delay(value_t value) {
		delay = value;
	}

	void reset(value_t _tick) {
		delay = _tick;
		tick = _tick;
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
	value_t tick{};
	bool single_shot{};
	bool fired{};
};