#ifndef __PUBTOPIC_H__
#define __PUBTOPIC_H__

#include <string>
#include <vector>
#include <memory>
#include <ndn-cxx/util/scheduler.hpp>

#include "stlutils.h"
#include "sensordevice.h"

class Topic;
using TopicPtr = std::shared_ptr<Topic>;

class Topic {
public:
	Topic()
	: m_sequence(0)
	, m_modelname("")
	, m_serial("")
	, m_lat(0)
	, m_lon(0)
	, m_lifedevice(false)
	, m_envdevice(false)
	, m_disasterdevice(false)
	{
	}

	Topic(std::string name)
	: m_sequence(0)
	, m_modelname("")
	, m_serial("")
	, m_lat(0)
	, m_lon(0)
	, m_lifedevice(false)
	, m_envdevice(false)
	, m_disasterdevice(false)
	{
		m_name = name;
	}

	Topic(std::string name, std::string modelName, std::string serial, double lat, double lon, bool lifeDevice, bool envDevice, bool disasterDevice)
	: m_sequence(0)
	, m_modelname(modelName)
	, m_serial(serial)
	, m_lat(lat)
	, m_lon(lon)
	, m_lifedevice(lifeDevice)
	, m_envdevice(envDevice)
	, m_disasterdevice(disasterDevice)
	{
		m_name = name;
	}

	virtual
	~Topic() { }

	void setName(std::string name) {
		m_name = name;
	}

	std::string getName() {
		return m_name;
	}

	void setFullName(std::string name) {
		m_fullname = name;
	}

	std::string getFullName() {
		return m_fullname;
	}

	void setModelName(std::string modelName) {
		m_modelname = modelName;
	}

	std::string getModelName() {
		return m_modelname;
	}

	void setSerial(std::string serial) {
		m_serial = serial;
	}

	std::string getSerial() {
		return m_serial;
	}

	void setLat(double lat) {
		m_lat = lat;
	}

	double getLat() {
		return m_lat;
	}

	void setLon(double lon) {
		m_lon = lon;
	}

	double getLon() {
		return m_lon;
	}

	void setLifeDevice(bool lifeDevice) {
		m_lifedevice = lifeDevice;
	}

	bool getLifeDevice() {
		return m_lifedevice;
	}

	void setEnvDevice(bool envDevice) {
		m_envdevice = envDevice;
	}

	bool getEnvDevice() {
		return m_envdevice;
	}

	void setDisasterDevice(bool disasterDevice) {
		m_disasterdevice = disasterDevice;
	}

	bool getDisasterDevice() {
		return m_disasterdevice;
	}

	void appendLifeDevice(SensorPtr sensor) {
		m_lifeDeviceList.push_back(sensor);
	}

	void appendEnvDevice(SensorPtr sensor) {
		m_envDeviceList.push_back(sensor);
	}

	void appendDisasterDevice(SensorPtr sensor) {
		m_disasterDeviceList.push_back(sensor);
	}

	std::vector<TopicPtr>::iterator beginTopic() {
		return m_topics.begin();
	}

	std::vector<TopicPtr>::iterator endTopic() {
		return m_topics.end();
	}

	void push_topic(TopicPtr topic) {
		m_topics.push_back(topic);
	}

	size_t topicsSize() {
		return m_topics.size();
	}

	std::vector<TopicPtr> & getTopics() {
		return m_topics;
	}

	void setNextSequence(long sequence) {
		m_sequence = sequence;
	}

	long getNextSequence() {
		long sequence = m_sequence;
		m_sequence++;
		return sequence;
	}

	long timeout() {
		m_sequence--;
		return m_sequence;
	}

public:
	long m_sequence;
	std::string m_name;
	std::string m_fullname;

	std::string m_modelname;
	std::string m_serial;
	double m_lat;
	double m_lon;

	bool m_lifedevice;
	bool m_envdevice;
	bool m_disasterdevice;

	std::vector<TopicPtr> m_topics;

	std::vector<SensorPtr> m_lifeDeviceList;
	std::vector<SensorPtr> m_envDeviceList;
	std::vector<SensorPtr> m_disasterDeviceList;

	ndn::scheduler::EventId m_schedulePD;
};

#endif
