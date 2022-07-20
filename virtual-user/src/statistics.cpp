/*
 * statistics.cpp
 *
 *  Created on: 2020. 9. 6.
 *      Author: ymtech
 */

#include <sys/time.h>
#include "stlutils.h"
#include "statistics.h"

Statistics::Statistics()
	: m_start({0, })
	, m_end({0, })
	, m_interest(0)
	, m_data(0)
	, m_nack(0)
	, m_timeout(0)
	, m_topic(0)
	{
}
Statistics::~Statistics() {
}

void Statistics::checkStartTime() {
	gettimeofday(&m_start, NULL);
}

void Statistics::setStartTime(struct timeval val) {
	m_start = val;
}

const struct timeval & Statistics::getStartTime() {
	return m_start;
}

std::string Statistics::getStartTimeString() {
	return stringf("%ld.%06ld", m_start.tv_sec, m_start.tv_usec);
}

void Statistics::checkEndTime() {
	gettimeofday(&m_end, NULL);
}

void Statistics::setEndTime(struct timeval val) {
	m_end = val;
}

const struct timeval & Statistics::getEndTime() {
	return m_end;
}

std::string Statistics::getEndTimeString() {
	return stringf("%ld.%06ld", m_end.tv_sec, m_end.tv_usec);
}

void Statistics::increaseWaitTime(struct timeval &val) {
	struct timeval dst = {0, };
	timeval_offset(&dst, &m_wait, &val);
	m_wait = dst;
}

void Statistics::saveWaitTime(std::string name, struct timeval &tv1, struct timeval &tv2, struct timeval &wait) {
	m_waitList.push_back(TimeData(name, tv1, tv2, wait));
}

std::string Statistics::getWaitTimeString() {
	return stringf("%ld.%06ld", m_wait.tv_sec, m_wait.tv_usec);
}

void Statistics::increase_interest(uint64_t val) {
	m_interest += val;
}

void Statistics::increase_data(uint64_t val) {
	m_data += val;
}

void Statistics::increase_timeout(uint64_t val) {
	m_timeout += val;
}

void Statistics::increase_nack(uint64_t val) {
	m_nack += val;
}

void Statistics::increase_topic(uint64_t val) {
	m_topic += val;
}
void Statistics:: decrease_topic(uint64_t val) {
	m_topic -= val;
}

bool Statistics::is_complete() {
	if(m_interest == m_data+m_nack+m_timeout) {
		if(0 < m_topic) {
			return false;
		}
		return true;
	}

	return false;
}
