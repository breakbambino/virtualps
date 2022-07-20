#include <dirent.h>     /* Defines DT_* constants */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>

#include <memory>
#include <tuple>
#include <regex>
#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <random>
#include <chrono>
#include <csignal>

#include <yaml-cpp/emitterstyle.h>
#include <yaml-cpp/eventhandler.h>
#include <yaml-cpp/yaml.h>  // IWYU pragma: keep

#include <yaml-cpp/parser.h>
#include <yaml-cpp/exceptions.h>

#include <boost/asio/io_service.hpp>
#include <boost/unordered_map.hpp>

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/encoding/block-helpers.hpp>

/*
 * ptree.hpp 또는 json_parser.hpp는
 * ndn-cxx 보다 먼저 include 오류 발생함.
 */
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "ndnutils.h"
#include "stlutils.h"

#include "subtopic.h"
#include "interestdelegator.h"
#include "interestoption.h"
#include "statistics.h"
#include "logging.h"

using YAML::Parser;

namespace PTree = boost::property_tree;
namespace PTreeJson = boost::property_tree::json_parser;

//using YAML::MockEventHandler;
using namespace ndn;
using namespace ndn::encoding;

NDN_LOG_INIT(psperf.subscriber);

std::string opt_confpath;
int opt_argc = 0;
char *opt_argv[1001] = {0, };

long cfg_duration = 0;

long cfg_STInterval = 100;

long cfg_SMInterval = 100;

long cfg_SDRound = 1;
long cfg_SDInterval = 100;
long cfg_SDRequestInterval = 10000000;

std::string cfg_SMRN("/rn-1");

std::string cfg_SDRN("/rn-1");

std::string cfg_logging_target(".");
std::string cfg_logging_prefix;

bool cfg_logging_console = false;
bool cfg_logging_file = true;

using Component = ndn::name::Component;
using TopicPtr = std::shared_ptr<Topic>;
using TopicMap = boost::unordered_map<std::string, TopicPtr>;

using InterestPtr = std::shared_ptr<Interest>;
using FacePtr = std::shared_ptr<Face>;
using SchedulerPtr = std::shared_ptr<Scheduler>;

using InterestDelegatorPtr = std::shared_ptr<InterestDelegator>;

using InterestOptionPtr = std::shared_ptr<InterestOption>;

InterestOptionPtr gInterestOption = nullptr;
InterestOptionPtr gSTInterestOption = nullptr;
InterestOptionPtr gSMInterestOption = nullptr;
InterestOptionPtr gSDInterestOption = nullptr;

std::shared_ptr<boost::asio::io_service> gIOService = nullptr;
FacePtr gFace = nullptr;
SchedulerPtr gScheduler = nullptr;

boost::unordered_map<std::string, TopicPtr> gTopicmap;
boost::unordered_map<std::string, TopicPtr> gSMTopicmap;
boost::unordered_map<std::string, TopicPtr> gSDTopicmap;

boost::unordered_map<std::string, struct timeval> gSTTimeMap;
boost::unordered_map<std::string, struct timeval> gSMTimeMap;
boost::unordered_map<std::string, struct timeval> gSDTimeMap;

bool gTestST = false;
bool gTestSM = false;
bool gTestSD = false;

bool cfg_STPrintDetailTimeData = false;
bool cfg_SMPrintDetailTimeData = false;
bool cfg_SDPrintDetailTimeData = false;

Statistics gStatST;
Statistics gStatSM;
Statistics gStatSD;

static
struct option long_options[] = {
	{"config", required_argument, (int *)'\0', (int)'c'},
	{"help", no_argument, 0, (int)'h'},
	{0, 0, 0, 0}
};

void usage(int argc, char *argv[]) {
	printf("Usage: %s -c <path>\n", argv[0]);
	printf("options:\n");
	printf("  -c|--config <path>     설정 파일 경로.\n");
	printf("  -h|--help              도움말.\n");
}

void signalHandler( int signum ) {
	NDN_LOG_INFO_SS("Receive SIGNAL: " << signum);

	if(gIOService == nullptr) {
		return;
	}
	gIOService->stop();
}


int parseArgs(int argc, char *argv[]) {

	int opt;
	int index;
	long tmp_number;
	int option_output = 0;
	int c;
	int digit_optind = 0;

	optind = 1;
	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;

		c = getopt_long(argc, argv, "c:h", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
			case 0:
				if(strcmp("help", long_options[option_index].name) == 0) {
					usage(argc, argv);
					exit(0);
				} else if(strcmp("config", long_options[option_index].name) == 0) {
					opt_confpath = optarg;
				}
				break;
			case 'c':
				opt_confpath = optarg;
				break;
			case 'h':
			default:
				usage(argc, argv);
				exit(0);
				break;
		}
	}

	for (opt_argc = 0; optind < argc; optind += 1, opt_argc += 1) {
		opt_argv[opt_argc] = argv[optind];
	}

	return 0;
}

std::string read_stream(std::istream& in) {
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

int load_logging(YAML::Node config) {
	if(config["Target"] == NULL) {
		NDN_LOG_INFO_SS("default logging target: " << cfg_logging_target);
	} else {
		cfg_logging_target = config["Target"].as<std::string>();
		NDN_LOG_INFO("logging target: " << cfg_logging_target);
	}

	if(config["Prefix"] == NULL) {
		NDN_LOG_INFO_SS("default logging prefix: " << cfg_logging_prefix);
	} else {
		cfg_logging_prefix = config["Prefix"].as<std::string>();
		NDN_LOG_INFO("logging prefix: " << cfg_logging_prefix);
	}

	if(config["Console"] == NULL) {
		NDN_LOG_INFO_SS("default print a console: " << cfg_logging_console);
	} else {
		cfg_logging_console = config["Console"].as<bool>();
		NDN_LOG_INFO("print to a console: " << cfg_logging_console);
	}

	if(config["File"] == NULL) {
		NDN_LOG_INFO_SS("default print to a file: " << cfg_logging_file);
	} else {
		cfg_logging_file = config["File"].as<bool>();
		NDN_LOG_INFO("print to a file: " << cfg_logging_file);
	}

	return 0;
}

int load_interest_option(YAML::Node options, InterestOptionPtr interestOption) {
	if(options["Lifetime"] != NULL) {
		interestOption->m_lifetime = options["Lifetime"].as<int64_t>();
	}

	if(options["CanBePrefix"] != NULL) {
		interestOption->m_canBePrefix = options["CanBePrefix"].as<bool>();
	}

	if(options["MustBeFresh"] != NULL) {
		interestOption->m_mustBeFresh = options["MustBeFresh"].as<bool>();
	}

	if(options["HopLimit"] != NULL) {
		interestOption->m_hopLimit = options["HopLimit"].as<uint8_t>();
	}

	return 0;
}

int load_packet_options(YAML::Node options) {
	if(options["DefaultOptions"] != NULL) {
		YAML::Node defaultOption = options["DefaultOptions"];
		if(defaultOption["Interest"] != NULL) {
			gInterestOption = std::make_shared<InterestOption>();
			load_interest_option(defaultOption["Interest"], gInterestOption);
		}
	}

	if(gInterestOption == nullptr) {
		gInterestOption = std::make_shared<InterestOption>();
	}

	if(options["ST"] != NULL) {
		YAML::Node stOption = options["ST"];
		if(stOption["Interest"] != NULL) {
			gSTInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			load_interest_option(stOption["Interest"], gSTInterestOption);
		}
	}

	if(gSTInterestOption == nullptr) {
		gSTInterestOption = std::make_shared<InterestOption>(*gInterestOption);
	}

	if(options["SM"] != NULL) {
		YAML::Node stOption = options["SM"];
		if(stOption["Interest"] != NULL) {
			gSMInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			load_interest_option(stOption["Interest"], gSMInterestOption);
		}
	}

	if(gSMInterestOption == nullptr) {
		gSMInterestOption = std::make_shared<InterestOption>(*gInterestOption);
	}

	if(options["SD"] != NULL) {
		YAML::Node stOption = options["SD"];
		if(stOption["Interest"] != NULL) {
			gSDInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			load_interest_option(stOption["Interest"], gSDInterestOption);
		}
	}

	if(gSDInterestOption == nullptr) {
		gSDInterestOption = std::make_shared<InterestOption>(*gInterestOption);
	}

	return 0;
}

bool yaml_getAsBool(YAML::Node parent, std::string child, bool defaultValue) {
	if(parent[child] == NULL) {
		return defaultValue;
	}

	return parent[child].as<bool>();
}

uint yaml_getAsUInt(YAML::Node parent, std::string child, uint defaultValue) {
	if(parent[child] == NULL) {
		return defaultValue;
	}

	return parent[child].as<uint>();
}

long yaml_getAsLong(YAML::Node parent, std::string child, long defaultValue) {
	if(parent[child] == NULL) {
		return defaultValue;
	}

	return parent[child].as<long>();
}

std::string yaml_getAsString(YAML::Node parent, std::string child, std::string defaultValue) {
	if(parent[child] == NULL) {
		return defaultValue;
	}

	return parent[child].as<std::string>();
}

int load_config(YAML::Node config) {
	YAML::Node node;
	if(config["Duration"] == NULL) {
		NDN_LOG_INFO("Missing 'Duration'");
		NDN_LOG_INFO(stringf("default Duration: %ld", cfg_duration));
	} else {
		cfg_duration = config["Duration"].as<long>();
		NDN_LOG_INFO(stringf("Duration: %ld", cfg_duration));
	}

	if((node = config["ST"]) != NULL) {
		gTestST = yaml_getAsBool(node, "Enable", false);
		cfg_STInterval = yaml_getAsLong(node, "Interval", cfg_STInterval);
		cfg_STPrintDetailTimeData = yaml_getAsBool(node, "PrintDetailTimeData", cfg_STPrintDetailTimeData);

		NDN_LOG_INFO(stringf("ST Enabled: %s", gTestST? "true":"false"));
		NDN_LOG_INFO(stringf("ST Interval: %ld", cfg_STInterval));
		NDN_LOG_INFO(stringf("ST PrintDetailTimeData: %s", cfg_STPrintDetailTimeData? "true":"false"));
	}

	if((node = config["SM"]) != NULL) {
		gTestSM = yaml_getAsBool(node, "Enable", false);
		cfg_SMRN = yaml_getAsString(node, "RN", cfg_SMRN);
		cfg_SMInterval = yaml_getAsLong(node, "Interval", cfg_SMInterval);
		cfg_SMPrintDetailTimeData = yaml_getAsBool(node, "PrintDetailTimeData", cfg_SMPrintDetailTimeData);

		NDN_LOG_INFO(stringf("SM Enabled: %s", gTestSM? "true":"false"));
		NDN_LOG_INFO(stringf("SM Interval: %ld", cfg_SMInterval));
		NDN_LOG_INFO(stringf("SM PrintDetailTimeData: %s", cfg_SMPrintDetailTimeData? "true":"false"));
	}

	if((node = config["SD"]) != NULL) {
		gTestSD = yaml_getAsBool(node, "Enable", false);
		cfg_SDRN = yaml_getAsString(node, "RN", cfg_SDRN);
		cfg_SDRound = yaml_getAsLong(node, "Round", cfg_SDRound);
		cfg_SDInterval = yaml_getAsLong(node, "Interval", cfg_SDInterval);
		cfg_SDRequestInterval = yaml_getAsLong(node, "RequestInterval", cfg_SDRequestInterval);
		cfg_SDPrintDetailTimeData = yaml_getAsBool(node, "PrintDetailTimeData", cfg_SDPrintDetailTimeData);

		NDN_LOG_INFO(stringf("SD Enabled: %s", gTestSD? "true":"false"));
		NDN_LOG_INFO(stringf("SD Round: %ld", cfg_SDRound));
		NDN_LOG_INFO(stringf("SD Interval: %ld", cfg_SDInterval));
		NDN_LOG_INFO(stringf("SD Request Interval: %ld", cfg_SDRequestInterval));
		NDN_LOG_INFO(stringf("SD PrintDetailTimeData: %s", cfg_SDPrintDetailTimeData? "true":"false"));
	}

	return 0;
}

int load_layer(YAML::Node layer, std::vector<TopicPtr> &topics) {
	long sequence = -1;
	// Enable 확인(default: true)
	// Prefix 확인(필수)
	// PrefixRange 확인(Prefix에  ${..} 를 포함한 경우 필수
	// Fetch 확인(leaf인 경우, 없으면 -1, leaf여부는 Layer가 없는 경우)

	// Enable 확인(default: true)
	if(layer["Enable"] == NULL) {
		// default true이므로
		layer["Enable"] = true;
	}

	if(layer["Enable"].as<bool>() == false) {
		// Enable이 있고
		// false면 무시
		return 0;
	}

	// Prefix 확인(필수)
	if(layer["Prefix"] == NULL) {
		NDN_LOG_ERROR("Missing 'Prefix'");
		return -1;
	}

	if(layer["Layer"] == NULL) {
		// Layer가 없는 경우, leaf 노드
		if(layer["Fetch"] == NULL) {
			// Fetch이 없는 경우, default -1
			layer["Fetch"] = -1L;
		}
	}

	// PrefixRange 확인(Prefix에  ${..} 를 포함한 경우 필수
	std::string prefixIndex;

	std::string prefix = layer["Prefix"].as<std::string>();

	// '#' 인 경우 하위는 무시하고 종료한다.
	if(prefix.compare("#") == 0) {
		NDN_LOG_DEBUG(stringf("Prefix: %s", prefix.c_str()));
		if(layer["Fetch"] != NULL) {
			sequence = layer["Fetch"].as<long>();
			NDN_LOG_DEBUG(stringf("Sequence: %ld", sequence));
		}
		// 현재 Layer의 topic
		TopicPtr topic = std::make_shared<Topic>(prefix);
		topic->setFetchSequence(sequence);
		NDN_LOG_DEBUG(stringf("Fetch: %ld", sequence));
		topics.push_back(topic);
		return 0;
	}
	std::size_t found = prefix.find("${");
	if (found == std::string::npos) {
		NDN_LOG_DEBUG(stringf("Prefix: %s", prefix.c_str()));
	} else {
		std::regex pattern("\\$\\{\\s*([^\\}\\s]+)\\s*\\}");
		std::smatch m;

		if (regex_search(prefix, m, pattern)) {
			prefixIndex = m[0];
			NDN_LOG_DEBUG(stringf("Prefix: %s, Variable part: %s", prefix.c_str(), prefixIndex.c_str()));
		}
	}

	if(layer["Fetch"] != NULL) {
		sequence = layer["Fetch"].as<long>();
		NDN_LOG_DEBUG(stringf("Sequence: %ld", sequence));
	}

	// 가변 prefix 설정
	if(0 < prefixIndex.size()) {
		// 반드시 PrefixRange가 설정되어 있어야 한다.
		if(layer["PrefixRange"] == NULL) {
			return -1;
		}

		// PrefixRange 패턴
		// 1
		// 1 ~ 10
		std::regex pattern("(\\d+)(\\s*[~\\-]\\s*(\\d+))?");
		std::smatch m;

		std::string str = layer["PrefixRange"].as<std::string>();
		NDN_LOG_DEBUG(stringf("PrefixRange: %s", str.c_str()));

		if (std::regex_match(str, m, pattern) == false) {
			return -1;
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

		std::string newname = prefix;
		found = newname.find(prefixIndex);
		if (found != std::string::npos) {
			newname.replace(found, prefixIndex.size(), "%ld");
		}

		// Prefix에 PrefixRange를 적용
		for(long i = min; i <= max; i ++) {
			std::string name = stringf(newname, i);
			NDN_LOG_DEBUG(name);

			// 현재 Layer의 topic
			TopicPtr topic = std::make_shared<Topic>(name);

			//load_layer(YAML::Node layer, std::vector<TopicPtr> &topics)

			if(layer["Layer"] == NULL) {
				// Layer가 없는 경우
				// TODO:
				topic->setFetchSequence(sequence);
				NDN_LOG_DEBUG(stringf("Fetch: %ld", sequence));
			} else {
				// Layer 가 있는 경우, 여러개의 Layer
				YAML::Node::iterator iter;
				for(iter = layer.begin(); iter != layer.end(); ++ iter) {
					std::string name = iter->first.as<std::string>();

					if(name.compare("Layer") != 0) {
						continue;
					}

					std::vector<TopicPtr> &children = topic->getTopics();
					if(load_layer(iter->second, children) == -1) {
						return -1;
					}
				}
			}
			topics.push_back(topic);
		}

		return 0;
	}

	TopicPtr topic = std::make_shared<Topic>(prefix);

	// layer["Layer"] == NULL이면, DataRange 확인(leaf인 경우 필수)
	if(layer["Layer"] == NULL) {
		topic->setFetchSequence(sequence);
	} else {
		// Layer 가 있는 경우, 여러개의 Layer
		YAML::Node::iterator iter;
		for(iter = layer.begin(); iter != layer.end(); ++ iter) {
			std::string name = iter->first.as<std::string>();

			if(name.compare("Layer") != 0) {
				continue;
			}

			std::vector<TopicPtr> &children = topic->getTopics();
			if(load_layer(iter->second, children) == -1) {
				return -1;
			}
		}
	}

	topics.push_back(topic);

	return 0;
}

ssize_t findParametersDigestComponent(const Name &name) {
	ssize_t pos = -1;
	for (ssize_t i = 0; i < static_cast<ssize_t>(name.size()); i++) {
		if (name[i].isParametersSha256Digest()) {
			if (pos != -1) {
				return -2;
			}
			pos = i;
		}
	}
	return pos;
}

void makeTopicList(std::vector<TopicPtr> &topics, std::string parent, boost::unordered_map<std::string, TopicPtr> &topicmap) {

	std::vector<TopicPtr>::iterator layer1iter;
	for(layer1iter = topics.begin(); layer1iter != topics.end(); layer1iter++) {
		TopicPtr layer1Topic = *layer1iter;
		std::string layer1name = layer1Topic->getName();

		if(layer1Topic->topicsSize() == 0) {
			std::string name(stringf("%s/%s", parent.c_str(), layer1name.c_str()));
			layer1Topic->setFullName(name);
			topicmap[name] = layer1Topic;
		} else {
			std::string name(stringf("%s/%s", parent.c_str(), layer1name.c_str()));
			makeTopicList(layer1Topic->getTopics(), name, topicmap);
		}
	}
}

void sendSD(std::string rnname, std::string dataname, long round, TopicPtr topic) {

	long sequence = topic->getNextSequence();
	Name name(rnname);
	name.append("SD");
	name.append(dataname);
	name.append(stringf("%ld", sequence));

	NDN_LOG_INFO_SS("SD Interest Name: " << name);

	Interest interest(name);
	interest.setCanBePrefix(gSDInterestOption->m_canBePrefix);
	interest.setMustBeFresh(gSDInterestOption->m_mustBeFresh);
	interest.setInterestLifetime(time::milliseconds(gSDInterestOption->m_lifetime));
	if(gSDInterestOption->m_hopLimit.has_value()) {
		interest.setHopLimit(gSDInterestOption->m_hopLimit);
	}

	std::string appParam("{\"subinfo\": null}");
	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest.setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	dataHandler=[=](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_INFO_SS("SD Data Name: " << data.getName());

		gStatSD.increase_data();

		gStatSD.checkEndTime();

		// Data를 받았으면, SM 목록에 등록 & 업데이트
		const Name &name = data.getName();
		//PartialName rnname = name.getSubName(0, 1);
		//PartialName dataname;
		PartialName seqName;

		std::string qname;

		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			// /rn-3/SD/Gu-1/BDOT-1/temp/100/param....
			//dataname = name.getSubName(2, pos-3);
			seqName = name.getSubName(pos-1, 1);
			qname = name.getPrefix(pos).toUri();
		} else {
			//dataname = name.getSubName(2, name.size()-3);
			seqName = name.getSubName(name.size()-1, 1);
			qname = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gSDTimeMap.find(qname);
		if(iter != gSDTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatSD.increaseWaitTime(diff);
			gStatSD.saveWaitTime(qname, iter->second, tv, diff);
		}

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("SD Data Content: " << value);

#if 0
		long seq = std::stol(seqName.get(0).toUri());

		long topicSeq;
		if((topicSeq = topic->getSequence()) < 0) {
			// config에 -1이 설정 되어 있으면, next sequence
			seq = seq + 1;
		} else {
			seq = topicSeq;
		}
#endif

		time::microseconds interval(0);
		gScheduler->schedule(interval, [rnname, dataname, round, topic] {
			sendSD(rnname, dataname, round+1, topic);
		});
	};
	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_INFO_SS("SD Timeout: " << interest);

		gStatSD.increase_timeout();

		gStatSD.checkEndTime();

		topic->timeout();

#if 0
		// Data를 받았으면, SM 목록에 등록 & 업데이트
		const Name &name = interest.getName();
		//PartialName rnname = name.getSubName(0, 1);
		//PartialName dataname;
		PartialName seqName;

		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			// /rn-3/SD/Gu-1/BDOT-1/temp/100/param....
			//dataname = name.getSubName(2, pos-3);
			seqName = name.getSubName(pos-1, 1);
		} else {
			//dataname = name.getSubName(2, name.size()-3);
			seqName = name.getSubName(name.size()-1, 1);
		}

		long seq = std::stol(seqName.get(0).toUri());
#endif
		time::microseconds interval(0);
		gScheduler->schedule(interval, [rnname, dataname, round, topic] {
			sendSD(rnname, dataname, round, topic);
		});

	};
	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_INFO_SS("SD Nack: " << name << ", Reason: " << nack.getReason());

		gStatSD.increase_nack();

		gStatSD.checkEndTime();
	};

	gFace->expressInterest(interest, dataHandler, nackHandler, timeoutHandler);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gSDTimeMap[name.toUri()] = tv;

	if(gStatSD.m_start.tv_sec == 0) {
		gStatSD.checkStartTime();
	}

	gStatSD.increase_interest();
}

void sendSM(std::string rnname, std::string dataname, TopicPtr topic) {

	Name name(rnname);
	name.append("SM");
	name.append(dataname);

	NDN_LOG_INFO_SS("SM Interest Name: " << name);

	Interest interest(name);
	interest.setCanBePrefix(gSMInterestOption->m_canBePrefix);
	interest.setMustBeFresh(gSMInterestOption->m_mustBeFresh);
	interest.setInterestLifetime(time::milliseconds(gSMInterestOption->m_lifetime));
	if(gSMInterestOption->m_hopLimit.has_value()) {
		interest.setHopLimit(gSMInterestOption->m_hopLimit);
	}

	std::string appParam("{\"subinfo\": null}");
	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest.setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	dataHandler=[=](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_INFO_SS("SM Data Name: " << data.getName());

		gStatSM.increase_data();

		gStatSM.checkEndTime();

		// Data를 받았으면, SM 목록에 등록 & 업데이트
		const Name &name = data.getName();
		PartialName rnname = name.getSubName(0, 1);
		PartialName dataname;

		std::string qname;

		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			dataname = name.getSubName(2, pos-2);
			qname = name.getPrefix(pos).toUri();
		} else {
			dataname = name.getSubName(2);
			qname = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gSMTimeMap.find(qname);
		if(iter != gSMTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatSM.increaseWaitTime(diff);
			gStatSM.saveWaitTime(qname, iter->second, tv, diff);
		}

		std::stringstream result;

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_INFO_SS( "SM Data Content: " << value);

		result << value;

		try {
			PTree::ptree pt;
			PTreeJson::read_json(result, pt);

			std::string status = pt.get<std::string>("status");
			long start = pt.get<long>("start");
			long count = pt.get<long>("count");
			long seq;
			NDN_LOG_DEBUG(stringf("status: %s, start: %d, count: %d", status.c_str(), start, count));

			seq = start+count;
			if(1 < seq) {
				seq = seq - 1;
			} else {
				seq = 1;
			}
			topic->setNextSequence(seq);

#if 0
			std::tuple<std::string, std::string> key;
			key = std::make_tuple<std::string, std::string>(rnname.toUri(), dataname.toUri());
			if(topic->hasRepository(key) == false) {
				topic->setRepositorySequence(key, seq);
			}
#endif

			sendSD(rnname.toUri(), dataname.toUri(), 0, topic);

		} catch (std::exception &e) {
			NDN_LOG_WARN(e.what());
		}
	};

	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_INFO_SS("SM Nack: " << name << ", Reason: " << nack.getReason());

		gStatSM.increase_nack();

		gStatSM.checkEndTime();
	};

	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_INFO_SS("SM Timeout: " << interest);

		gStatSM.increase_timeout();

		gStatSM.checkEndTime();

		time::microseconds interval(0);
		gScheduler->schedule(interval, [rnname, dataname, topic] {
			sendSM(rnname, dataname, topic);
		});
	};

	gFace->expressInterest(interest, dataHandler, nackHandler, timeoutHandler);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gSMTimeMap[name.toUri()] = tv;

	if(gStatSM.m_start.tv_sec == 0) {
		gStatSM.checkStartTime();
	}

	gStatSM.increase_interest();
}

/**
 * dataname에 대한 ST Interest을 전송한다.
 */
void sendST(std::string dataname, TopicPtr topic) {

	Name name("rn");
	name.append("ST");
	name.append(dataname);

	NDN_LOG_DEBUG_SS("ST Interest Name: " << name);

	// Topic별로 Interest를 작성한다.
	Interest interest(name);
	interest.setCanBePrefix(gSTInterestOption->m_canBePrefix);
	interest.setMustBeFresh(gSTInterestOption->m_mustBeFresh);
	interest.setInterestLifetime(time::milliseconds(gSTInterestOption->m_lifetime));
	if(gSTInterestOption->m_hopLimit.has_value()) {
		interest.setHopLimit(gSTInterestOption->m_hopLimit);
	}

	std::string appParam("{\"subinfo\": null}");
	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest.setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	// Interest에 대한 Data를 수신 핸들러를 준비한다.
	dataHandler=[topic](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("ST Data Name: " << data.getName());

		gStatST.increase_data();

		gStatST.checkEndTime();

		// Data를 받았으면, SM 목록에 등록 & 업데이트
		std::string uri;
		const Name &name = data.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gSTTimeMap.find(uri);
		if(iter != gSTTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatST.increaseWaitTime(diff);
			gStatST.saveWaitTime(uri, iter->second, tv, diff);
		}

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("ST Data Content: " << value);

		try {
			PTree::ptree pt;

			std::stringstream result;
			result << value;
			PTreeJson::read_json(result, pt);
			// PTreeJson::write_json(std::cout, pt);

			PTree::ptree status = pt.get_child("status");
			PTree::ptree values = pt.get_child("values");

			// { status: "OK", values: [["rn-3", "/Gu-1/BDOT-1/temp"]]}
			PTree::ptree::assoc_iterator valueIter;
			for(valueIter = values.ordered_begin(); valueIter != values.not_found(); valueIter++) {
				//
				PTree::ptree::assoc_iterator itemIter;

				// 첫번째는 rnname
				itemIter = valueIter->second.ordered_begin();

				// rn name이 없다면
				if(itemIter == valueIter->second.not_found()) {
					continue;
				}
				std::string rnname = itemIter->second.get<std::string>("");

				// 두번째는 dataname
				itemIter++;

				// data name이 없다면(없어도 되지 않나?)
				if(itemIter == valueIter->second.not_found()) {
					continue;
				}

				std::string dataname = itemIter->second.get<std::string>("");

				//NDN_LOG_DEBUG(stringf("%s: %s", rnname.c_str(), dataname.c_str()));

				// 여기에 실제 데이터를 가지고 있는 rn과 data name을 등록하면
				// scheduleSendSM() 에서 목록을 조회하여 SM Interest를 전송한다.
				//topic->addRepos(rnname, dataname);

				sendSM(rnname, dataname, topic);
			}

		} catch (std::exception &e) {
			NDN_LOG_WARN(e.what());
		}
	};

	// Interest에 대한 Nack 핸들러를 준비한다.
	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_DEBUG_SS("ST Nack: " << name << ", Reason: " << nack.getReason());

		gStatST.increase_nack();

		gStatST.checkEndTime();
	};

	// Interest에 대한 Timeout 핸들러를 준비한다.
	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_DEBUG_SS("ST Timeout: " << interest);

		gStatST.increase_timeout();

		gStatST.checkEndTime();

		time::microseconds interval(0);
		gScheduler->schedule(interval, [dataname, topic] {
			sendST(dataname, topic);
		});
	};

	gFace->expressInterest(interest, dataHandler, nackHandler, timeoutHandler);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gSTTimeMap[name.toUri()] = tv;

	if(gStatST.m_start.tv_sec == 0) {
		gStatST.checkStartTime();
	}

	gStatST.increase_interest();
}

void sendST(std::string topicName) {

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		return;
	}

	TopicPtr topic = iter->second;

	Name st_name("/rn/ST");
	st_name.append(topic->getFullName());

	InterestPtr interest = nullptr;
	interest = std::make_shared<ndn::Interest>(st_name);
	interest->setCanBePrefix(gSTInterestOption->m_canBePrefix);
	interest->setMustBeFresh(gSTInterestOption->m_mustBeFresh);
	interest->setInterestLifetime(time::milliseconds(gSTInterestOption->m_lifetime));
	if(gSTInterestOption->m_hopLimit.has_value()) {
		interest->setHopLimit(gSTInterestOption->m_hopLimit);
	}

	std::string appParam("{\"subinfo\": null}");
	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest->setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	dataHandler=[=](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("ST Data Name: " << data);

		gStatST.increase_data();

		gStatST.checkEndTime();

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("ST Data Content: " << value);

		std::string uri;
		const Name &name = data.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gSTTimeMap.find(uri);
		if(iter != gSTTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatST.increaseWaitTime(diff);
			gStatST.saveWaitTime(uri, iter->second, tv, diff);
		}

		gStatST.decrease_topic();
	};

	// Interest에 대한 Nack 핸들러를 준비한다.
	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_DEBUG_SS("ST Nack: " << name << ", Reason: " << nack.getReason());

		gStatST.increase_nack();

		gStatST.checkEndTime();

		gStatST.decrease_topic();
	};

	// Interest에 대한 Timeout 핸들러를 준비한다.
	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_DEBUG_SS("ST Timeout: " << interest);

		gStatST.increase_timeout();

		gStatST.checkEndTime();

		std::string uri;
		PartialName dataname;
		const Name &name = interest.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			dataname = name.getSubName(2, pos-2);
			uri = name.getPrefix(pos).toUri();
		} else {
			dataname = name.getSubName(2);
			uri = name.toUri();
		}

		sendST(dataname.toUri());
	};

	NDN_LOG_DEBUG_SS("Send ST Interest: " << *interest);
	gFace->expressInterest(*interest, dataHandler, nackHandler, timeoutHandler);

	gStatST.increase_interest();

	std::string uri = st_name.toUri();

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gSTTimeMap[uri] = tv;
}

void sendSM(std::string topicName) {

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		return;
	}

	TopicPtr topic = iter->second;

	Name sm_name(cfg_SMRN);
	sm_name.append("SM");
	sm_name.append(topic->getFullName());

	InterestPtr interest = nullptr;
	interest = std::make_shared<ndn::Interest>(sm_name);
	interest->setCanBePrefix(gSMInterestOption->m_canBePrefix);
	interest->setMustBeFresh(gSMInterestOption->m_mustBeFresh);
	interest->setInterestLifetime(time::milliseconds(gSMInterestOption->m_lifetime));
	if(gSMInterestOption->m_hopLimit.has_value()) {
		interest->setHopLimit(gSMInterestOption->m_hopLimit);
	}

	std::string appParam("{\"subinfo\": null}");
	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest->setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	dataHandler=[=](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("SM Data Name: " << data);

		gStatSM.increase_data();

		gStatSM.checkEndTime();

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("SM Data Content: " << value);

		std::string uri;
		const Name &name = data.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gSMTimeMap.find(uri);
		if(iter != gSMTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatSM.increaseWaitTime(diff);
			gStatSM.saveWaitTime(uri, iter->second, tv, diff);
		}

		gStatSM.decrease_topic();
	};

	// Interest에 대한 Nack 핸들러를 준비한다.
	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_DEBUG_SS("SM Nack: " << name << ", Reason: " << nack.getReason());

		gStatSM.increase_nack();

		gStatSM.checkEndTime();

		gStatSM.decrease_topic();
	};

	// Interest에 대한 Timeout 핸들러를 준비한다.
	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_DEBUG_SS("SM Timeout: " << interest);

		gStatSM.increase_timeout();

		gStatSM.checkEndTime();

		std::string uri;
		PartialName dataname;
		const Name &name = interest.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			dataname = name.getSubName(2, pos-2);
			uri = name.getPrefix(pos).toUri();
		} else {
			dataname = name.getSubName(2);
			uri = name.toUri();
		}

		sendSM(dataname.toUri());
	};

	NDN_LOG_DEBUG_SS("Send SM Interest: " << *interest);
	gFace->expressInterest(*interest, dataHandler, nackHandler, timeoutHandler);

	gStatSM.increase_interest();

	std::string uri = sm_name.toUri();

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gSMTimeMap[uri] = tv;
}

void sendSD(std::string topicName, long round) {

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		return;
	}

	TopicPtr topic = iter->second;

	long sequence = topic->getNextSequence();
	Name sd_name(cfg_SDRN);
	sd_name.append("SD");
	sd_name.append(topic->getFullName());
	sd_name.append(std::to_string(sequence));

	InterestPtr interest = nullptr;
	interest = std::make_shared<ndn::Interest>(sd_name);
	interest->setCanBePrefix(gSDInterestOption->m_canBePrefix);
	interest->setMustBeFresh(gSDInterestOption->m_mustBeFresh);
	interest->setInterestLifetime(time::milliseconds(gSDInterestOption->m_lifetime));
	if(gSDInterestOption->m_hopLimit.has_value()) {
		interest->setHopLimit(gSDInterestOption->m_hopLimit);
	}

	std::string appParam("{\"subinfo\": null}");
	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest->setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	dataHandler=[=](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("SD Data Name: " << data);

		gStatSD.increase_data();

		gStatSD.checkEndTime();

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("SD Data Content: " << value);

		std::string uri;
		const Name &name = data.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gSDTimeMap.find(uri);
		if(iter != gSDTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatSD.increaseWaitTime(diff);
			gStatSD.saveWaitTime(uri, iter->second, tv, diff);
		}

		// round 수 만큼 일정 간격으로 반복해서 보낸다.
		if(round+1 < cfg_SDRound) {
			time::microseconds interval(cfg_SDRequestInterval);
			gScheduler->schedule(interval, [=] {
				sendSD(topicName, round+1);
			});
		} else {
			gStatSD.decrease_topic();
		}
	};

	// Interest에 대한 Nack 핸들러를 준비한다.
	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_DEBUG_SS("SD Nack: " << name << ", Reason: " << nack.getReason());

		gStatSD.increase_nack();

		gStatSD.checkEndTime();

		topic->timeout();

		gStatSD.decrease_topic();
	};

	// Interest에 대한 Timeout 핸들러를 준비한다.
	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_DEBUG_SS("SD Timeout: " << interest);

		gStatSD.increase_timeout();

		gStatSD.checkEndTime();

		topic->timeout();

		std::string uri;
		PartialName dataname;
		const Name &name = interest.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			dataname = name.getSubName(2, pos-2);
			uri = name.getPrefix(pos).toUri();
		} else {
			dataname = name.getSubName(2);
			uri = name.toUri();
		}

#if 0
		// round 수 만큼 일정 간격으로 반복해서 보낸다.
		if(round+1 < cfg_PDRound) {
			sendPD(topicName, round+1);
		} else {
			gStatPD.decrease_topic();
		}
#else
		sendSD(topicName, round);
#endif
	};

	NDN_LOG_DEBUG_SS("Send SD Interest: " << *interest);
	gFace->expressInterest(*interest, dataHandler, nackHandler, timeoutHandler);

	gStatSD.increase_interest();

	std::string uri = sd_name.toUri();

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gSDTimeMap[uri] = tv;
}

void sendST(std::shared_ptr<std::vector<std::string>> topics) {

	std::string topicName = topics->back();
	topics->pop_back();

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		time::microseconds interval(0);
		gScheduler->schedule(interval, [=] {
			sendST(topics);
		});

		return;
	}

	sendST(topicName);

	if(topics->empty()) {
		return;
	}

	time::microseconds interval(cfg_STInterval);
	gScheduler->schedule(interval, [=] {
		sendST(topics);
	});
}

void sendSM(std::shared_ptr<std::vector<std::string>> topics) {

	std::string topicName = topics->back();
	topics->pop_back();

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		time::microseconds interval(0);
		gScheduler->schedule(interval, [=] {
			sendSM(topics);
		});

		return;
	}

	sendSM(topicName);

	if(topics->empty()) {
		return;
	}

	time::microseconds interval(cfg_SMInterval);
	gScheduler->schedule(interval, [=] {
		sendSM(topics);
	});
}

void sendSD(std::shared_ptr<std::vector<std::string>> topics) {

	std::string topicName = topics->back();
	topics->pop_back();

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		time::microseconds interval(0);
		gScheduler->schedule(interval, [=] {
			sendSD(topics);
		});

		return;
	}

	sendSD(topicName, 0);

	if(topics->empty()) {
		return;
	}

	time::microseconds interval(cfg_SDInterval);
	gScheduler->schedule(interval, [=] {
		sendSD(topics);
	});
}

/**
 * Topic을 조회하여, ST interest를 전송한다.
 * 수신된 Data 들의 content를 확인하여
 * SM Interest를 전송한다.
 */
void scheduleSendST(TopicMap &topicmap) {

	std::shared_ptr<std::vector<std::string>> topics = std::make_shared<std::vector<std::string>>();
	// Topic을 조회한다.
	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		topics->push_back(iter->first);
	}

	gStatST.checkStartTime();

	time::microseconds interval(0);
	gScheduler->schedule(interval, [=] {
		sendST(topics);
	});
}

void scheduleSendSM(TopicMap &topicmap) {

	std::shared_ptr<std::vector<std::string>> topics = std::make_shared<std::vector<std::string>>();
	// Topic을 조회한다.
	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		topics->push_back(iter->first);
	}

	gStatSM.checkStartTime();

	time::microseconds interval(0);
	gScheduler->schedule(interval, [=] {
		sendSM(topics);
	});
}

void scheduleSendSD(TopicMap &topicmap) {

	std::shared_ptr<std::vector<std::string>> topics = std::make_shared<std::vector<std::string>>();
	// Topic을 조회한다.
	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		topics->push_back(iter->first);
	}

	gStatSD.checkStartTime();

	time::microseconds interval(0);
	gScheduler->schedule(interval, [=] {
		sendSD(topics);
	});
}

void check_complete() {
	time::seconds wait(1);
	gScheduler->schedule(wait, [=] {
		check_complete();
	});

	if(gStatST.is_complete() == false) {
		return;
	}

	if(gStatSM.is_complete() == false) {
		return;
	}

	if(gStatSD.is_complete() == false) {
		return;
	}

	gIOService->stop();
}

int main(int argc, char *argv[]) {
	char path[PATH_MAX];
	char buffer[4096];
	int opt;
	int index;
	char *pos;
	int retval;

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	if((retval = parseArgs(argc, argv)) != 0) {
		usage(argc, argv);
		return retval;
	}

	if(opt_confpath.empty()) {
		std::cerr << "Missing config file" << std::endl;
		usage(argc, argv);
		return -1;
	}

	std::ifstream input(opt_confpath);

	try {
		YAML::Node doc = YAML::Load(input);
		std::cout << doc << std::endl;

		cfg_logging_prefix = argv[0];
		if(doc["Logging"] != NULL) {
			load_logging(doc["Logging"]);
		}

		if(cfg_logging_console == false) {
			remove_all_sinks();
		}

		// 로그를 파일에도 저장하도록 함.
		if(cfg_logging_file) {
			add_text_file_sink(cfg_logging_target, cfg_logging_prefix);
		}

		if(doc["Config"] == NULL) {
			NDN_LOG_INFO("Missing 'Config' section");
			return -1;
		} else {
			load_config(doc["Config"]);
		}

		if(doc["PacketOptions"] == NULL) {
			gInterestOption = std::make_shared<InterestOption>();
			gSTInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			gSMInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			gSDInterestOption = std::make_shared<InterestOption>(*gInterestOption);
		} else {
			load_packet_options(doc["PacketOptions"]);
		}

		YAML::Node subscriber = doc["Subscriber"];
		if(subscriber == NULL) {
			NDN_LOG_ERROR("Missing 'Subscriber' section");
			return -1;
		}

		// Layer 1 topics
		std::vector<TopicPtr> topics;
		YAML::Node::iterator iter;
		for(iter = subscriber.begin(); iter != subscriber.end(); ++ iter) {
			std::string name = iter->first.as<std::string>();

			if(name.compare("Layer") != 0) {
				continue;
			}

			if(load_layer(iter->second, topics) == -1) {
				return -1;
			}
		}

		gIOService = std::make_shared<boost::asio::io_service>();
		gFace = std::make_shared<Face>(*gIOService);
		gScheduler = std::make_shared<Scheduler>(*gIOService);

		// topic을 순회하여 map(topic name, topic)을 생성한다.
		makeTopicList(topics, "", gTopicmap);

		if(gTestST) {
			gStatST.increase_topic(gTopicmap.size());

			scheduleSendST(gTopicmap);
		}

		if(gTestSM) {
			gStatSM.increase_topic(gTopicmap.size());

			scheduleSendSM(gTopicmap);
		}

		if(gTestSD) {
			gStatSD.increase_topic(gTopicmap.size());

			scheduleSendSD(gTopicmap);
		}

		time::seconds wait(1);
		gScheduler->schedule(wait, [=] {
			check_complete();
		});

		if(0 < cfg_duration) {
			time::microseconds duration(cfg_duration);
			gScheduler->schedule(duration, [=] {
				gIOService->stop();
			});
		}

		bool is_stop = false;
		while(is_stop == false) {
			try {
				boost::asio::io_service::work work(*gIOService);
				gIOService->run();
				is_stop = true;
			} catch (std::exception &e) {
				NDN_LOG_ERROR(e.what());
			}
		}

		NDN_LOG_INFO(stringf("Topic Count: %d", gTopicmap.size()));

		if(gTestST) {

			NDN_LOG_INFO(stringf("   Start ST: %s", gStatST.getStartTimeString().c_str()));
			NDN_LOG_INFO(stringf("     End ST: %s", gStatST.getEndTimeString().c_str()));
			struct timeval diff = timeval_diff(&gStatST.m_start, &gStatST.m_end);
			NDN_LOG_INFO(stringf("    Time ST: %ld.%06ld", diff.tv_sec, diff.tv_usec));
			if(0 < gStatST.m_data) {
				int64_t avg = (gStatST.m_wait.tv_sec*1000000 + gStatST.m_wait.tv_usec)/gStatST.m_data;
				NDN_LOG_INFO(stringf("     AVG ST: %ld.%06ld", avg/1000000, avg%1000000));
			} else {
				NDN_LOG_INFO(stringf("     AVG ST: %s", gStatST.getWaitTimeString().c_str()));
			}
			NDN_LOG_INFO(stringf("Interest ST: %d", gStatST.m_interest));
			NDN_LOG_INFO(stringf("    Data ST: %d", gStatST.m_data));
			NDN_LOG_INFO(stringf("    NACK ST: %d", gStatST.m_nack));
			NDN_LOG_INFO(stringf(" Timeout ST: %d", gStatST.m_timeout));
			if(cfg_STPrintDetailTimeData) {
				NDN_LOG_INFO(stringf(" Each ST List: %d", gStatST.m_waitList.size()));
				std::vector<TimeData>::iterator dataiter = gStatST.m_waitList.begin();
				for(; dataiter != gStatST.m_waitList.end(); dataiter++) {
					TimeData &data = *dataiter;
					NDN_LOG_INFO(stringf(" %s: %d.%06ld - %d.%06ld = %d.%06ld", data.m_name.c_str(),
							data.m_tv1.tv_sec, data.m_tv1.tv_usec,
							data.m_tv2.tv_sec, data.m_tv2.tv_usec,
							data.m_tv3.tv_sec, data.m_tv3.tv_usec));
				}
			}
		}

		if(gTestSM) {
			NDN_LOG_INFO(stringf("   Start SM: %s", gStatSM.getStartTimeString().c_str()));
			NDN_LOG_INFO(stringf("     End SM: %s", gStatSM.getEndTimeString().c_str()));
			struct timeval diff = timeval_diff(&gStatSM.m_start, &gStatSM.m_end);
			NDN_LOG_INFO(stringf("    Time SM: %ld.%06ld", diff.tv_sec, diff.tv_usec));
			if(0 < gStatSM.m_data) {
				int64_t avg = (gStatSM.m_wait.tv_sec*1000000 + gStatSM.m_wait.tv_usec)/gStatSM.m_data;
				NDN_LOG_INFO(stringf("     AVG SM: %ld.%06ld", avg/1000000, avg%1000000));
			} else {
				NDN_LOG_INFO(stringf("     AVG SM: %s", gStatSM.getWaitTimeString().c_str()));
			}
			NDN_LOG_INFO(stringf("Interest SM: %d", gStatSM.m_interest));
			NDN_LOG_INFO(stringf("    Data SM: %d", gStatSM.m_data));
			NDN_LOG_INFO(stringf("    NACK SM: %d", gStatSM.m_nack));
			NDN_LOG_INFO(stringf(" Timeout SM: %d", gStatSM.m_timeout));
			if(cfg_SMPrintDetailTimeData) {
				NDN_LOG_INFO(stringf(" Each SM List: %d", gStatSM.m_waitList.size()));
				std::vector<TimeData>::iterator dataiter = gStatSM.m_waitList.begin();
				for(; dataiter != gStatSM.m_waitList.end(); dataiter++) {
					TimeData &data = *dataiter;
					NDN_LOG_INFO(stringf(" %s: %d.%06ld - %d.%06ld = %d.%06ld", data.m_name.c_str(),
							data.m_tv1.tv_sec, data.m_tv1.tv_usec,
							data.m_tv2.tv_sec, data.m_tv2.tv_usec,
							data.m_tv3.tv_sec, data.m_tv3.tv_usec));
				}
			}
		}

		if(gTestSD) {
			NDN_LOG_INFO(stringf("   Start SD: %s", gStatSD.getStartTimeString().c_str()));
			NDN_LOG_INFO(stringf("     End SD: %s", gStatSD.getEndTimeString().c_str()));
			struct timeval diff = timeval_diff(&gStatSD.m_start, &gStatSD.m_end);
			NDN_LOG_INFO(stringf("    Time SD: %ld.%06ld", diff.tv_sec, diff.tv_usec));
			if(0 < gStatSD.m_data) {
				int64_t avg = (gStatSD.m_wait.tv_sec*1000000 + gStatSD.m_wait.tv_usec)/gStatSD.m_data;
				NDN_LOG_INFO(stringf("     AVG SD: %ld.%06ld", avg/1000000, avg%1000000));
			} else {
				NDN_LOG_INFO(stringf("     AVG SD: %s", gStatSD.getWaitTimeString().c_str()));
			}
			NDN_LOG_INFO(stringf("Interest SD: %d", gStatSD.m_interest));
			NDN_LOG_INFO(stringf("    Data SD: %d", gStatSD.m_data));
			NDN_LOG_INFO(stringf("    NACK SD: %d", gStatSD.m_nack));
			NDN_LOG_INFO(stringf(" Timeout SD: %d", gStatSD.m_timeout));
			if(cfg_SDPrintDetailTimeData) {
				NDN_LOG_INFO(stringf(" Each SD List: %d", gStatSD.m_waitList.size()));
				std::vector<TimeData>::iterator dataiter = gStatSD.m_waitList.begin();
				for(; dataiter != gStatSD.m_waitList.end(); dataiter++) {
					TimeData &data = *dataiter;
					NDN_LOG_INFO(stringf(" %s: %d.%06ld - %d.%06ld = %d.%06ld", data.m_name.c_str(),
							data.m_tv1.tv_sec, data.m_tv1.tv_usec,
							data.m_tv2.tv_sec, data.m_tv2.tv_usec,
							data.m_tv3.tv_sec, data.m_tv3.tv_usec));
				}
			}
		}

	} catch (const YAML::Exception &e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}
