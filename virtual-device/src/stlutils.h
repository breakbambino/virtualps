/*
 * stlutls.h
 *
 *      Author: kim, seong-jung
 *  Updated on: 2019. 5. 27.
 *     Version: 0.0.1-0001
 */
#ifndef __STLUTILS_H__
#define __STLUTILS_H__
#include <string>
#include <cctype>
#include <cwctype>
#include <random>
#include <chrono>

std::string stringf(const std::string &fmt, ...);

void toUpper(std::string &str);

bool caseInsCompare(const std::string &s1, const std::string &s2);

bool caseInsCompare(const std::wstring &s1, const std::wstring &s2);

std::mt19937_64 getRandomGenerator();

long generateUniformIntData(std::mt19937_64 &gen, long min, long max);

double generateUniformRealData(std::mt19937_64 &gen, double min, double max);

std::string generateUniformStringData(std::mt19937_64 &gen, std::vector<std::string> &strlist);

struct timeval timeval_diff(struct timeval *from, struct timeval *to);

void timeval_offset(struct timeval *dst, struct timeval *src, struct timeval *offset);

bool parseIntRange(std::string src, std::tuple<long, long> &range);

#endif
