/******************************************************************************
*   - host_state - класс для использования в std:map - состояние хоста
*     (состояние ping'а, состояние квитирования, кол-во пользователей, время
*     последнего изменения состояния (ping'а));
*   - ipgroup_state_t - состояние группы.
******************************************************************************/
#ifndef IPADDR_H
#define IPADDR_H

#include "../common/my_time.h"

#include <string>
#include <istream>
#include <ostream>

namespace ipstate { enum t {unknown=0, ok=1, warn=2, fail=3}; }

/* Хранитель текущего состояния ip-адреса для привязки в std::map */
class ipaddr_state_t {
	private:
		ipstate::t state_; /* Состояние ping'а */
		boost::posix_time::ptime state_changed_; /* Время изменения состояния */
		bool acknowledged_; /* Квитирован? */
		int users_; /* Кол-во наблюдателей за состоянием */

	public:
		/* Значения по умолчанию установлены потому,
			что std::map требует конструктор по умолчанию */
		explicit ipaddr_state_t(ipstate::t state = ipstate::unknown,
				bool acknowledged = true)
			: state_(state)
			, state_changed_( boost::posix_time::microsec_clock::local_time() )
			, acknowledged_(acknowledged)
			, users_(0) {}

		inline ipstate::t state(void) const { return state_; }
		
		/* Изменение состояния. Может быть сразу заквитировано */
		bool set_state(ipstate::t new_state, bool acknowledge = false);

		inline boost::posix_time::ptime state_changed(void) const {
			return state_changed_;
		}

		/* Квитирование */
		inline bool acknowledged(void) const { return acknowledged_; }
		inline void acknowledge(void) { acknowledged_ = true; }
		inline void unacknowledge(void) { acknowledged_ = false; }

		/* Регистрация наблюдателей */
		inline int users(void) const { return users_; }
		inline int add_user(void) { return ++users_; }
		inline int del_user(void) { return users_ ? 0 : --users_; }
};

class ipgroup_state_t {
	private:
		int counters_[4]; /* Счётчик объектов для каждого состояния */
		bool ack_by_state_[4]; /* Квитирование по состояниям */

	public:
		ipgroup_state_t() {
			counters_[ipstate::unknown] = 0;
			counters_[ipstate::ok] = 0;
			counters_[ipstate::warn] = 0;
			counters_[ipstate::fail] = 0;

			ack_by_state_[ipstate::unknown] = true;
			ack_by_state_[ipstate::ok] = true;
			ack_by_state_[ipstate::warn] = true;
			ack_by_state_[ipstate::fail] = true;
		}

		inline bool operator ==(const ipgroup_state_t &rhs) const {
			return state() == rhs.state() && acknowledged() == rhs.acknowledged();
		}
		inline bool operator !=(const ipgroup_state_t &rhs) const {
			return state() != rhs.state() || acknowledged() != rhs.acknowledged();
		}

		ipgroup_state_t& operator<<(const ipaddr_state_t &st) {
			counters_[ st.state() ]++;
			if (!st.acknowledged()) ack_by_state_[ st.state() ] = false;
			return *this;
		}

		inline ipstate::t state(void) const {
			ipstate::t _state = ipstate::unknown;
			for (int i = 1; i < 4; i++) {
				if (counters_[i]) {
					if (_state == ipstate::unknown) 
						_state = (ipstate::t)i;
					else
						return ipstate::unknown;
				}
			}
			return _state;
		}
		
		inline bool acknowledged(void) const {
			bool _acknowledged = true;
			for (int i = 0; i < 4; i++) {
				if (!ack_by_state_[i])
					_acknowledged = false;
			}
			return _acknowledged;
		}
		
		inline int count(ipstate::t state) const {
			return counters_[state];
		}
		inline bool acknowledged(ipstate::t state) const {
			return ack_by_state_[state];
		}
};

#endif
