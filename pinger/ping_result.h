#ifndef PING_RESULT_H
#define PING_RESULT_H

#include "../common/my_time.h"
#include "../common/my_http.h"

#include "icmp_header.hpp"
#include "ipv4_header.hpp"

#include "string.h"

#include <string>
#include <sstream>

#define PING_RESULT_VER 1

namespace pinger {

/* Результат отдельного ping'а */
class ping_result
{
public:
	enum state_t {unknown, ok, timeout};

private:
	state_t state_;
	posix_time::ptime time_;
	posix_time::time_duration duration_;
	ipv4_header ipv4_hdr_;
	icmp_header icmp_hdr_;

public:
	ping_result()
		: state_(unknown) {}

	ping_result(const std::wstring &str)
	{
		std::wstringstream in(str);
		in >> *this;
		if (!in)
			*this = ping_result();
	}

	inline state_t state() const
		{ return state_; }
	inline void set_state(state_t st)
		{ state_ = st; }

	inline posix_time::ptime time() const
		{ return time_; }
	inline void set_time(posix_time::ptime t)
		{ time_ = t; }

	inline posix_time::time_duration duration() const
		{ return duration_; }
	inline void set_duration(posix_time::time_duration d)
		{ duration_ = d; }

	inline unsigned short sequence_number() const
		{ return icmp_hdr_.sequence_number(); }
	inline void set_sequence_number(unsigned short n)
		{ icmp_hdr_.sequence_number(n); }

	inline const ipv4_header& ipv4_hdr() const
		{ return ipv4_hdr_; }
	inline void set_ipv4_hdr(const ipv4_header &ipv4_hdr)
		{ ipv4_hdr_ = ipv4_hdr; }

	inline const icmp_header& icmp_hdr() const
		{ return icmp_hdr_; }
	inline void set_icmp_hdr(const icmp_header &icmp_hdr)
		{ icmp_hdr_ = icmp_hdr; }

	std::wstring to_wstring() const
	{
		std::wstringstream out;
		out << *this;
		return out.str();
	}

	std::wstring winfo() const
	{
		std::wstringstream out;

		out << sequence_number()
			<< L' ' << state_to_wstring()
			<< L' ' << my::time::to_wstring(time_)
			<< L' ' << duration_.total_milliseconds() << L"ms";

		return out.str();
	}

	std::wstring state_to_wstring() const
	{
		return state_ == ok ? L"ok"
			: state_ == timeout ? L"timeout"
			: L"unknown";
	}

	inline bool operator ==(state_t st) const
		{ return state_ == st; }

	inline bool operator !=(state_t st) const
		{ return state_ != st; }

	friend std::wistream& operator>>(std::wistream& in, ping_result& pr)
	{
		int ver = 0;
		in >> ver;
		if (ver != PING_RESULT_VER)
			in.setstate(std::ios::failbit);

		std::wstring state_s;
		in >> state_s;
		if (state_s == L"ok")
			pr.state_ = ok;
		else if (state_s == L"timeout")
			pr.state_ = timeout;
		else
			in.setstate(std::ios::failbit);

		std::wstring time_s, time2_s;
		in >> time_s;
		in >> time2_s;
		time_s += std::wstring(L" ") + time2_s;
		pr.time_ = my::time::to_time(time_s);
		if (pr.time_.is_special())
			in.setstate(std::ios::failbit);

		std::wstring dur_s;
		in >> dur_s;
		pr.duration_ = my::time::to_duration(dur_s);
		if (pr.duration_.is_special())
			in.setstate(std::ios::failbit);

		std::wstring ipv4_s;
		in >> ipv4_s;
		std::string ipv4_s2 = my::str::from_hex(my::str::to_string(ipv4_s));
		if (ipv4_s2.size() != sizeof(pr.ipv4_hdr_.rep_))
			in.setstate(std::ios::failbit);
		else
			memcpy(pr.ipv4_hdr_.rep_, ipv4_s2.c_str(), sizeof(pr.ipv4_hdr_.rep_));

		std::wstring icmp_s;
		in >> icmp_s;
		std::string icmp_s2( my::str::from_hex(my::str::to_string(icmp_s)) );
		if (icmp_s2.size() != sizeof(pr.icmp_hdr_.rep_))
			in.setstate(std::ios::failbit);
		else
			memcpy(pr.icmp_hdr_.rep_, icmp_s2.c_str(), sizeof(pr.icmp_hdr_.rep_));

		return in;
	}

	friend std::wostream& operator<<(std::wostream& out, const ping_result &pr)
	{
		out << PING_RESULT_VER
			<< L' ' << pr.state_to_wstring()
			<< L' ' << my::time::to_wstring(pr.time_)
			<< L' ' << my::time::to_wstring(pr.duration_)
			<< L' ' << my::str::to_wstring( my::str::to_hex(
				(const char*)pr.ipv4_hdr_.rep_, sizeof(pr.ipv4_hdr_.rep_) ) )
			<< L' ' << my::str::to_wstring( my::str::to_hex(
				(const char*)pr.icmp_hdr_.rep_, sizeof(pr.icmp_hdr_.rep_) ) );

		return out;
	}

};

}

#endif
