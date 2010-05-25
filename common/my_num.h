#ifndef MY_NUM_H
#define MY_NUM_H

#include <string>

namespace my { namespace num {

int to_int(const std::string &str, int def);
int to_int(const std::wstring &str, int def);
int to_int(const char *str, int def);
int to_int(const wchar_t *str, int def);

unsigned int to_uint(const std::string &str, unsigned int def);
unsigned int to_uint(const std::wstring &str, unsigned int def);
unsigned int to_uint(const char *str, unsigned int def);
unsigned int to_uint(const wchar_t *str, unsigned int def);

short to_short(const std::string &str, short def);
short to_short(const std::wstring &str, short def);
short to_short(const char *str, short def);
short to_short(const wchar_t *str, short def);

unsigned short to_ushort(const std::string &str, unsigned short def);
unsigned short to_ushort(const std::wstring &str, unsigned short def);
unsigned short to_ushort(const char *str, unsigned short def);
unsigned short to_ushort(const wchar_t *str, unsigned short def);

float to_float(const std::string &str, float def);
float to_float(const std::wstring &str, float def);
float to_float(const char *str, float def);
float to_float(const wchar_t *str, float def);

double to_double(const std::string &str, double def);
double to_double(const std::wstring &str, double def);
double to_double(const char *str, double def);
double to_double(const wchar_t *str, double def);

} }

#endif
