#ifndef __SUBTOPIC_H__
#define __SUBTOPIC_H__

#include <string>
#include <tuple>
#include <vector>
#include <type_traits>

#include "datatype.h"

class Topic;
using TopicPtr = std::shared_ptr<Topic>;

class ParametizedData {
public:
	ParametizedData() {
		m_type = DataType::None;
	}
	~ParametizedData() {}
public:
	enum DataType m_type;
};

class Topic {
public:
	Topic()
	: m_fetch(-1)
	, m_sequence(0)
	, m_type(DataType::None)
	{
	}

	Topic(std::string name)
	: m_fetch(-1)
	, m_sequence(0)
	, m_type(DataType::None)
	{
		m_name = name;
	}

	Topic(std::string name, DataType type)
	: m_fetch(-1)
	, m_sequence(0)
	, m_type(type)
	{
		m_name = name;
	}

	virtual
	~Topic() { }

	void setType(DataType type) {
		m_type = type;
	}

	enum DataType getType() {
		return m_type;
	}

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

	std::vector<TopicPtr>::iterator beginTopic() {
		return m_topics.begin();
	}

	std::vector<TopicPtr>::iterator endTopic() {
		return m_topics.end();
	}

	void push_topic(TopicPtr &topic) {
		m_topics.push_back(topic);
	}

	size_t topicsSize() {
		return m_topics.size();
	}

	std::vector<TopicPtr> & getTopics() {
		return m_topics;
	}

	void setFetchSequence(long sequence) {
		m_fetch = sequence;
	}

	long getFetchSequence() {
		return m_fetch;
	}

	void setNextSequence(long sequence) {
		m_sequence = sequence;
	}

	long getNextSequence() {
		if(0 < m_fetch) {
			return m_fetch;
		}
		int sequence = m_sequence;
		m_sequence ++;
		return sequence;
	}

	long timeout() {
		if(0 < m_fetch) {
			return m_fetch;
		}
		m_sequence--;
		return m_sequence;
	}

	void addRepos(std::string rnname, std::string dataname) {
		m_repos.push_back(std::make_tuple(rnname, dataname));
	}

	std::vector<std::tuple<std::string, std::string>> & getRepos() {
		return m_repos;
	}

	void clearRepos() {
		m_repos.clear();
	}

	bool hasRepository(std::tuple<std::string, std::string> key) {
		return m_repoorySequence.find(key) != m_repoorySequence.end();
	}

	void setRepositorySequence(std::tuple<std::string, std::string> key, long sequence) {
		m_repoorySequence[key] = sequence;
	}

	long getRepositorySequence(std::tuple<std::string, std::string> key) {
		boost::unordered_map<std::tuple<std::string, std::string>, long>::iterator iter;
		iter = m_repoorySequence.find(key);
		if(iter == m_repoorySequence.end()) {
			return -1;
		}

		return iter->second;
	}

public:
	long m_fetch;
	long m_sequence;
	std::string m_name;
	std::string m_fullname;
	enum DataType m_type;

	std::vector<TopicPtr> m_topics;

	std::vector<std::tuple<std::string, std::string>> m_repos;
	boost::unordered_map<std::tuple<std::string, std::string>, long> m_repoorySequence;
};

#endif
