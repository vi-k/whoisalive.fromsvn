#include "my_num.h"
#include "my_qi.h"

using namespace std;

namespace my { namespace num {

template<class Result, class Rule>
Result to_num(const string &str, Rule rule, Result def)
{
	const char *ptr = str.c_str();
	qi::parse(ptr, ptr + strlen(ptr), rule, def);
	return def;
}

template<class Result, class Rule>
Result to_num(const wstring &str, Rule rule, Result def)
{
	const wchar_t *ptr = str.c_str();
	qi::parse(ptr, ptr + wcslen(ptr), rule, def);
	return def;
}

template<class Result, class Rule>
Result to_num(const char *str, Rule rule, Result def)
{
	qi::parse(str, str + strlen(str), rule, def);
	return def;
}

template<class Result, class Rule>
Result to_num(const wchar_t *str, Rule rule, Result def)
{
	qi::parse(str, str + wcslen(str), rule, def);
	return def;
}

#define DEF_TO_NUM_FUNCS(N,T) \
T to_##N(const std::string &str, T def) \
	{ return my::num::to_num(str, qi::N##_, def); } \
T to_##N(const std::wstring &str, T def) \
	{ return my::num::to_num(str, qi::N##_, def); } \
T to_##N(const char *str, T def) \
	{ return my::num::to_num(str, qi::N##_, def); } \
T to_##N(const wchar_t *str, T def) \
	{ return my::num::to_num(str, qi::N##_, def); }

DEF_TO_NUM_FUNCS(int,int)
DEF_TO_NUM_FUNCS(uint,unsigned int)
DEF_TO_NUM_FUNCS(short,short)
DEF_TO_NUM_FUNCS(ushort,unsigned short)
DEF_TO_NUM_FUNCS(float,float)
DEF_TO_NUM_FUNCS(double,double)

} }
