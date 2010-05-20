#include "ipaddr.h"

#include <stdio.h>

using namespace std;

/* boost::posix_time (без подключения *.lib) */
#include <libs/date_time/src/posix_time/posix_time_types.cpp>
#include <libs/date_time/src/gregorian/date_generators.cpp>

unsigned long ipaddr_t::str2ip(const char *str)
{
	unsigned long a, b, c, d;
	unsigned long addr = 0;
	char ch;

	if ( sscanf_s(str, "%u.%u.%u.%u%c", &a, &b, &c, &d, &ch) == 4
			&& a <= 255 && b <= 255 && c <= 255 && d <= 255 ) {
		
		unsigned char *ch_addr = (unsigned char*)&addr;
		ch_addr[0] = (unsigned char)a;
		ch_addr[1] = (unsigned char)b;
		ch_addr[2] = (unsigned char)c;
		ch_addr[3] = (unsigned char)d;
	}

	return addr;
}

unsigned long ipaddr_t::str2ip(const wchar_t *str)
{
	unsigned long a, b, c, d;
	unsigned long addr = 0;
	wchar_t ch;

	if ( swscanf_s(str, L"%u.%u.%u.%u%c", &a, &b, &c, &d, &ch) == 4
			&& a <= 255 && b <= 255 && c <= 255 && d <= 255 ) {
		
		unsigned char *ch_addr = (unsigned char*)&addr;
		ch_addr[0] = (unsigned char)a;
		ch_addr[1] = (unsigned char)b;
		ch_addr[2] = (unsigned char)c;
		ch_addr[3] = (unsigned char)d;
	}

	return addr;
}

void ipaddr_t::to_str(char *buf, size_t max_count) const
{
	sprintf_s(buf, max_count, "%u.%u.%u.%u",
		(unsigned long)((unsigned char*)&addr_)[0],
		(unsigned long)((unsigned char*)&addr_)[1],
		(unsigned long)((unsigned char*)&addr_)[2],
		(unsigned long)((unsigned char*)&addr_)[3]);
}

void ipaddr_t::to_str(wchar_t *buf, size_t max_count) const
{
	swprintf_s(buf, max_count, L"%u.%u.%u.%u",
		(unsigned long)((unsigned char*)&addr_)[0],
		(unsigned long)((unsigned char*)&addr_)[1],
		(unsigned long)((unsigned char*)&addr_)[2],
		(unsigned long)((unsigned char*)&addr_)[3]);
}

bool ipaddr_state_t::set_state(ipstate::t new_state, bool acknowledge)
{
	bool changed = false;

	/* Матрица перехода состояний:
		 1 - квитировать,
		 0 - не менять квитирование,
		-1 - снять квитирование */
	const int matrix[4][4] = {
		/* unknown -> */ { 0,  0,  0,  0},
		/*      ok -> */ { 0,  0,  0, -1},
		/*    warn -> */ { 0,  0,  0, -1},
		/*    fail -> */ { 0,  0,  0,  0}
	};

	switch ( matrix[state_][new_state] ) {
		case 1:  acknowledged_ = true; break;
		case -1: acknowledged_ = false; break;
	}

	if (state_ != new_state) {
		state_ = new_state;
		state_changed_ = boost::posix_time::microsec_clock::local_time();
		changed = true;
	}			

	if (acknowledge && !acknowledged_) {
		acknowledged_ = true;
		changed = true;
	}

	return changed;
}

#if 0
#include <iostream>
int main(void)
{
	ipaddr_t a;
	ipaddr_t b("127.0.0.1");
	ipaddr_t c("");
	ipaddr_t d(L"");

	cout << a << endl;
	cout << "Enter ip: ";
	cin >> b;
	cout << b << endl;

	ipaddr_state_t st;
	ipgroup_state_t grst;

	cout << grst.state() << "." << grst.acknowledged() << endl;	
	grst << ipaddr_state_t(ipstate::ok);
	cout << grst.state() << "." << grst.acknowledged() << endl;	
	grst << ipaddr_state_t(ipstate::warn);
	cout << grst.state() << "." << grst.acknowledged() << endl;	
	grst << ipaddr_state_t(ipstate::fail);
	cout << grst.state() << "." << grst.acknowledged() << endl;	
	grst << ipaddr_state_t(ipstate::unknown);
	cout << grst.state() << "." << grst.acknowledged() << endl;	

	st.set_state(ipstate::fail);
	grst << st;
	cout << grst.state() << "." << grst.acknowledged() << endl;	

	return 1;
}
#endif