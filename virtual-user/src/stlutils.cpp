#include <stdio.h>
#include <stdarg.h>
#include <algorithm>
#include <regex>

#include "stlutils.h"

std::string stringf(const std::string &fmt, ...) {
	// use a rubric appropriate for your code
	int n, size;
	std::string str;
	va_list ap;

	va_start(ap, fmt);
	n = vsnprintf((char *)'\0', 0, fmt.c_str(), ap);
	va_end(ap);

	size = n + 1;
	// maximum 2 passes on a POSIX system...
	while (1) {
		str.resize(size);
		va_start(ap, fmt);
		int n = vsnprintf((char *) str.data(), size, fmt.c_str(), ap);
		va_end(ap);
		// everything worked
		if (n > -1 && n < size) {
			str.resize(n);
			return str;
		}
		// needed size returned
		if (n > -1) {
			// for null char
			size = n + 1;
		} else {
			// guess at a larger size (o/s specific)
			size *= 2;
		}
	}
	return str;
}

void toUpper(std::string &str) {
	std::transform(str.begin(), str.end(), str.begin(), ::toupper);
}

inline bool caseInsCharCompareN(char a, char b) {
   return(toupper(a) == toupper(b));
}

inline bool caseInsCharCompareW(wchar_t a, wchar_t b) {
   return(towupper(a) == towupper(b));
}

bool caseInsCompare(const std::string &s1, const std::string &s2) {
   return((s1.size() == s2.size()) &&
          equal(s1.begin(), s1.end(), s2.begin(), caseInsCharCompareN));
}

bool caseInsCompare(const std::wstring &s1, const std::wstring &s2) {
   return((s1.size() == s2.size()) &&
          equal(s1.begin(), s1.end(), s2.begin(), caseInsCharCompareW));
}


std::mt19937_64 getRandomGenerator() {
	auto current = std::chrono::system_clock::now();
	auto duration = current.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

	return std::mt19937_64(millis);
}

long generateUniformIntData(std::mt19937_64 &gen, long min, long max) {
	std::uniform_int_distribution<long> uniformDist(min, max);
	return uniformDist(gen);
}

double generateUniformRealData(std::mt19937_64 &gen, double min, double max) {
	std::uniform_real_distribution<double> uniformDist(min, max);
	return uniformDist(gen);
}

std::string generateUniformStringData(std::mt19937_64 &gen, std::vector<std::string> strlist) {
	std::uniform_int_distribution<uint> uniformDist(0, strlist.size()-1);
	uint index = uniformDist(gen);

	return strlist.at(index);
}

struct timeval timeval_diff(struct timeval *from, struct timeval *to) {
	int64_t from_usec = (int64_t)(from->tv_sec * 1000000) + from->tv_usec;
	int64_t to_usec = (int64_t)(to->tv_sec * 1000000) + to->tv_usec;

	int64_t diff = to_usec - from_usec;

	struct timeval tv;
	tv.tv_sec = diff / 1000000;
	tv.tv_usec = diff % 1000000;
	return tv;
}

void timeval_offset(struct timeval *dst, struct timeval *src, struct timeval *offset) {
	int64_t from = src->tv_sec * 1000000 + src->tv_usec;
	int64_t diff = offset->tv_sec * 1000000 + offset->tv_usec;
	int64_t to = from + diff;

	dst->tv_sec = to / 1000000;
	dst->tv_usec = to % 1000000;
}

bool parseIntRange(std::string src, std::tuple<long, long> &range) {

	size_t pos = 0;
	// PrefixRange 패턴
	// 1
	// 1 ~ 10
	std::regex pattern("([+-]?\\d+)(\\s*[~\\-]\\s*([+-]?\\d+))?");
	std::smatch m;

	if (std::regex_match(src, m, pattern) == false) {
		return false;
	}
	long min = 0;
	long max = 0;

	if(m[1].matched && m[3].matched) {
		min = std::stol(m[1].str());
		max = std::stol(m[3].str());
	} else if(m[1].matched) {
		min = std::stol(m[1].str());
		max = std::stol(m[1].str());
	}

	std::get<0>(range) = min;
	std::get<1>(range) = max;

	return true;
}

bool parseRealRange(std::string src, std::tuple<double, double> &range) {

	size_t pos = 0;
	// PrefixRange 패턴
	// 1
	// 1 ~ 10
	std::regex pattern("([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?)(\\s*[~\\-]\\s*([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?))?");
	std::smatch m;

	if (std::regex_match(src, m, pattern) == false) {
		return false;
	}
	double min = 0;
	double max = 0;

	if(m[1].matched && m[3].matched) {
		min = std::stold(m[1].str());
		max = std::stold(m[4].str());
	} else if(m[1].matched) {
		min = std::stold(m[1].str());
		max = std::stold(m[1].str());
	}

	std::get<0>(range) = min;
	std::get<1>(range) = max;

	return true;
}
