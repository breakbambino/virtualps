/*
 * statistics.h
 *
 *  Created on: 2020. 9. 6.
 *      Author: ymtech
 */

#include <stdint.h>
#include <time.h>

#include <string>
#include <vector>

#ifndef __STATISTICS_H__
#define __STATISTICS_H__

class TimeData {
public:
	TimeData(std::string name, struct timeval tv1, struct timeval tv2, struct timeval tv3)
	: m_name(name), m_tv1(tv1), m_tv2(tv2), m_tv3(tv3) { }
	std::string m_name;
	struct timeval m_tv1;
	struct timeval m_tv2;
	struct timeval m_tv3;
};

class Statistics {
public:
	// start time
	struct timeval m_start = {0, };
	// end time
	struct timeval m_end = {0, };

	struct timeval m_wait = {0, };
	// Interest
	uint64_t m_interest ;
	// Data
	uint64_t m_data = 0;
	// Nack
	uint64_t m_nack = 0;
	// Timeout
	uint64_t m_timeout = 0;

	uint64_t m_topic = 0;

	std::vector<TimeData> m_waitList;

public:
	Statistics();
	~Statistics();

	void checkStartTime();
	void setStartTime(struct timeval val);
	const struct timeval & getStartTime();
	std::string getStartTimeString();

	void checkEndTime();
	void setEndTime(struct timeval val);
	const struct timeval & getEndTime();
	std::string getEndTimeString();

	void increaseWaitTime(struct timeval &val);
	void saveWaitTime(std::string name, struct timeval &tv1, struct timeval &tv2, struct timeval &wait);
	std::string getWaitTimeString();

	void increase_interest(uint64_t val = 1);
	void increase_data(uint64_t val = 1);
	void increase_nack(uint64_t val = 1);
	void increase_timeout(uint64_t val = 1);

	void increase_topic(uint64_t val = 1);
	void decrease_topic(uint64_t val = 1);

	bool is_complete();
};



#endif /* __STATISTICS_H__ */
