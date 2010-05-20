/******************************************************************************
* Классы:
*   - ipaddr_t - замена winsock'овскому in_addr, с преобразованием в строку
*     и обратно, операторами сравнения (==,!=,<), сохранением в поток (<<)
*     и чтением с потока (>>);
*   - ipaddr_state_t - класс для использования в std:map - состояние ip_адреса
*     (состояние ping'а, состояние квитирования, кол-во пользователей, время
*     последнего изменения состояния (ping'а));
*   - ipgroup_state_t - состояние группы.
******************************************************************************/
#ifndef IPADDR_H
#define IPADDR_H

#include <string>
#include <istream>
#include <ostream>

#include <boost/date_time/posix_time/posix_time.hpp>

namespace ipstate { enum t {unknown=0, ok=1, warn=2, fail=3}; }

class ipaddr_t {
	private:
		unsigned long addr_;

	public:
		explicit ipaddr_t() : addr_(0) {}
		explicit ipaddr_t(unsigned long addr) : addr_(addr) {}
		explicit ipaddr_t(const char *str) : addr_( str2ip(str) ) {}
		explicit ipaddr_t(const wchar_t *str) : addr_( str2ip(str) ) {}

		inline unsigned long get(void) const { return addr_; }
		inline bool ok(void) const { return addr_ != 0; }

		static unsigned long str2ip(const char *str);
		static unsigned long str2ip(const wchar_t *str);

		void to_str(char *buf, size_t max_count) const;
		void to_str(wchar_t *buf, size_t max_count) const;

		template<class Ch>
		std::basic_string<Ch> str(void) {
			Ch buf[16];
			to_str( buf, sizeof(buf)/sizeof(Ch) );
			return std::basic_string<Ch>(buf);
		}
		
		inline bool operator ==(const ipaddr_t &rhs) const {
			return addr_ == rhs.addr_;
		}
		inline bool operator !=(const ipaddr_t &rhs) const {
			return addr_ != rhs.addr_;
		}
		inline bool operator <(const ipaddr_t &rhs) const {
			return addr_ < rhs.addr_;
		}
};

template<class Ch>
std::basic_istream<Ch>& operator>>(std::basic_istream<Ch>& s, ipaddr_t &addr)
{
	if (s.fail()) return s;

	std::basic_string<Ch> str;
	int pos = s.tellg();

	s >> str;

	if (!s.fail()) {
		ipaddr_t res(str.c_str());

		if ( res.ok() )
			addr = res;
		else {
			s.seekg(pos); /* На память: если fail() seekg() не работает */
			s.setstate(std::ios::failbit);
		}
	}

	return s;
}

template<class Ch>
std::basic_ostream<Ch>& operator<<(std::basic_ostream<Ch>& s, const ipaddr_t &addr)
{
	Ch buf[16];
	addr.to_str( buf, sizeof(buf)/sizeof(Ch) );
	s << buf;
	return s;
}


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
