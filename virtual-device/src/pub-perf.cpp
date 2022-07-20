#include <dirent.h>     /* Defines DT_* constants */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>

#include <memory>
#include <regex>
#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
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

#include <boost/log/common.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/attr.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>

#include <boost/log/utility/record_ordering.hpp>
#include <boost/log/support/date_time.hpp>

#include "ndnutils.h"
#include "stlutils.h"

#include "pubtopic.h"
#include "interestdelegator.h"
#include "interestoption.h"
#include "statistics.h"

#include "logging.h"

using YAML::Parser;
//using YAML::MockEventHandler;
using namespace ndn;
using namespace ndn::encoding;

NDN_LOG_INIT(psperf.publisher);

std::string opt_confpath;
int opt_argc = 0;
char *opt_argv[1001] = {0, };

std::string cfg_PDContent;

long cfg_duration = 0;

long cfg_PAinterval = 100;
long cfg_PUinterval = 1000;

long cfg_PDRound = 100;
long cfg_PDinterval = 100;
long cfg_PDGenerateInterval = 10000000;
long cfg_PDContentSize = 100;

std::string cfg_logging_target(".");
std::string cfg_logging_prefix;

bool cfg_logging_console = false;
bool cfg_logging_file = true;

using TopicPtr = std::shared_ptr<Topic>;
using TopicMap = boost::unordered_map<std::string, TopicPtr>;

using InterestPtr = std::shared_ptr<Interest>;
using FacePtr = std::shared_ptr<Face>;
using SchedulerPtr = std::shared_ptr<Scheduler>;

using InterestDelegatorPtr = std::shared_ptr<InterestDelegator>;

using InterestOptionPtr = std::shared_ptr<InterestOption>;

InterestOptionPtr gInterestOption = nullptr;
InterestOptionPtr gPAInterestOption = nullptr;
InterestOptionPtr gPDInterestOption = nullptr;
InterestOptionPtr gPUInterestOption = nullptr;

std::shared_ptr<boost::asio::io_service> gIOService = nullptr;
FacePtr gFace = nullptr;
SchedulerPtr gScheduler = nullptr;

std::vector<TopicPtr> gTopics;
boost::unordered_map<std::string, TopicPtr> gTopicmap;

boost::unordered_map<std::string, struct timeval> gPATimeMap;
boost::unordered_map<std::string, struct timeval> gPUTimeMap;
boost::unordered_map<std::string, struct timeval> gPDTimeMap;

bool gTestPA = false;
bool gTestPU = false;
bool gTestPD = false;

bool cfg_PAPrintDetailTimeData = false;
bool cfg_PUPrintDetailTimeData = false;
bool cfg_PDPrintDetailTimeData = false;

Statistics gStatPA;
Statistics gStatPU;
Statistics gStatPD;

bool is_first_pa = true;
//boost::unordered_map<std::string, Statistics> gStatPD;

static
struct option long_options[] = {
	{"config", required_argument, (int *)'\0', (int)'c'},
	{"help", no_argument, 0, (int)'h'},
	{0, 0, 0, 0}
};

void signalHandler( int signum ) {
	NDN_LOG_INFO_SS("Receive SIGNAL: " << signum);

	if(gIOService == nullptr) {
		return;
	}
	gIOService->stop();
}

void usage(int argc, char *argv[]) {
	printf("Usage: %s -c <path>\n", argv[0]);
	printf("options:\n");
	printf("  -c|--config <path>     설정 파일 경로.\n");
	printf("  -h|--help              도움말.\n");
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

	if(options["PA"] != NULL) {
		YAML::Node stOption = options["PA"];
		if(stOption["Interest"] != NULL) {
			gPAInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			load_interest_option(stOption["Interest"], gPAInterestOption);
		}
	}

	if(gPAInterestOption == nullptr) {
		gPAInterestOption = std::make_shared<InterestOption>(*gInterestOption);
	}


	if(options["PD"] != NULL) {
		YAML::Node stOption = options["PD"];
		if(stOption["Interest"] != NULL) {
			gPDInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			load_interest_option(stOption["Interest"], gPDInterestOption);
		}
	}

	if(gPDInterestOption == nullptr) {
		gPDInterestOption = std::make_shared<InterestOption>(*gInterestOption);
	}

	if(options["PU"] != NULL) {
		YAML::Node stOption = options["PU"];
		if(stOption["Interest"] != NULL) {
			gPUInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			load_interest_option(stOption["Interest"], gPUInterestOption);
		}
	}

	if(gPUInterestOption == nullptr) {
		gPUInterestOption = std::make_shared<InterestOption>(*gInterestOption);
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
	if((node = config["Duration"]) == NULL) {
		NDN_LOG_INFO("Missing 'Config.Duration'");
		NDN_LOG_INFO(stringf("default duration: %ld", cfg_duration));
	} else {
		cfg_duration = node.as<uint>();
		NDN_LOG_INFO(stringf("Duration: %ld", cfg_duration));
	}

	if((node = config["PA"]) != NULL) {
		gTestPA = yaml_getAsBool(node, "Enable", false);
		cfg_PAinterval = yaml_getAsLong(node, "Interval", cfg_PAinterval);
		cfg_PAPrintDetailTimeData = yaml_getAsBool(node, "PrintDetailTimeData", cfg_PAPrintDetailTimeData);

		NDN_LOG_INFO(stringf("PA Enabled: %s", gTestPA? "true":"false"));
		NDN_LOG_INFO(stringf("PA Interval: %ld", cfg_PAinterval));
		NDN_LOG_INFO(stringf("PA PrintDetailTimeData: %s", cfg_PAPrintDetailTimeData? "true":"false"));
	}

	if((node = config["PU"]) != NULL) {
		gTestPU = yaml_getAsBool(node, "Enable", false);
		cfg_PUinterval = yaml_getAsLong(node, "Interval", cfg_PUinterval);
		cfg_PUPrintDetailTimeData = yaml_getAsBool(node, "PrintDetailTimeData", cfg_PUPrintDetailTimeData);

		NDN_LOG_INFO(stringf("PU Enabled: %s", gTestPU? "true":"false"));
		NDN_LOG_INFO(stringf("PU Interval: %ld", cfg_PUinterval));
		NDN_LOG_INFO(stringf("PU PrintDetailTimeData: %s", cfg_PUPrintDetailTimeData? "true":"false"));
	}

	if((node = config["PD"]) != NULL) {
		gTestPD = yaml_getAsBool(node, "Enable", false);
		cfg_PDRound = yaml_getAsLong(node, "Round", cfg_PDRound);
		cfg_PDinterval = yaml_getAsLong(node, "Interval", cfg_PDinterval);
		cfg_PDGenerateInterval = yaml_getAsLong(node, "GenerateInterval", cfg_PDGenerateInterval);
		cfg_PDContentSize = yaml_getAsLong(node, "ContentSize", cfg_PDContentSize);
		if(0 < cfg_PDContentSize) {
			cfg_PDContent.resize(cfg_PDContentSize);
			for(int i = 0; i < cfg_PDContentSize; i ++) {
				cfg_PDContent[i] = '0';
			}
		}
		cfg_PDPrintDetailTimeData = yaml_getAsBool(node, "PrintDetailTimeData", cfg_PDPrintDetailTimeData);

		NDN_LOG_INFO(stringf("PD Enabled: %s", gTestPD? "true":"false"));
		NDN_LOG_INFO(stringf("PD Round: %ld", cfg_PDRound));
		NDN_LOG_INFO(stringf("PD Interval: %ld", cfg_PDinterval));
		NDN_LOG_INFO(stringf("PD Generate Interval: %ld", cfg_PDGenerateInterval));
		NDN_LOG_INFO(stringf("PD ContentSize: %ld", cfg_PDContentSize));
		NDN_LOG_INFO(stringf("PD PrintDetailTimeData: %s", cfg_PDPrintDetailTimeData? "true":"false"));
	}

	return 0;
}

int setDataRange(YAML::Node dataRange, TopicPtr &topic) {
	DataType datatype = topic->getType();

	if(datatype == DataType::Int) {
		std::regex pattern("([+-]?\\d+)(\\s*[~\\-]\\s*([+-]?\\d+))?");
		std::smatch m;

		std::string str = dataRange.as<std::string>();
		NDN_LOG_DEBUG(stringf("DataRange: %s", str.c_str()));

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
			max = min;
		}

		ParametizedData *minparam = new TypedParameter<long>(min);
		if(topic->push_back(minparam) == false) {
			NDN_LOG_ERROR(stringf("Not available: %ld", min));
			return -1;
		}

		ParametizedData *maxparam = new TypedParameter<long>(max);
		if(topic->push_back(maxparam) == false) {
			NDN_LOG_ERROR(stringf("Not available: %ld", max));
			return -1;
		}
	} else if(datatype == DataType::Real) {
		std::regex pattern("([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?)(\\s*[~\\-]\\s*([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?))?");
		std::smatch m;

		std::string str = dataRange.as<std::string>();
		NDN_LOG_DEBUG(stringf("DataRange: %s", str.c_str()));

		if (std::regex_match(str, m, pattern) == false) {
			return -1;
		}
		double min = 0;
		double max = 0;

		if(m[1].matched && m[4].matched) {
			min = std::stold(m[1].str());
			max = std::stold(m[4].str());
		} else if(m[1].matched) {
			min = std::stold(m[1].str());
			max = min;
		}

		ParametizedData *minparam = new TypedParameter<double>(min);
		if(topic->push_back(minparam) == false) {
			NDN_LOG_ERROR(stringf("Not available: %lf", min));
			return -1;
		}

		ParametizedData *maxparam = new TypedParameter<double>(max);
		if(topic->push_back(maxparam) == false) {
			NDN_LOG_ERROR(stringf("Not available: %lf", max));
			return -1;
		}
	} else if(datatype == DataType::String) {
		for(int i = 0; i < dataRange.size(); i ++) {
			std::string name = dataRange[i].as<std::string>();
			NDN_LOG_DEBUG(stringf("DataRange: %s", name.c_str()));

			ParametizedData *minparam = new TypedParameter<std::string>(name);
			if(topic->push_back(minparam) == false) {
				NDN_LOG_ERROR(stringf("Not available: %s", name.c_str()));
				return -1;
			}
		}
	}

	return 0;
}

int load_layer(YAML::Node layer, std::vector<TopicPtr> &topics) {
	DataType datatype = DataType::None;

	// Enable 확인(default: true)
	// Prefix 확인(필수)
	// PrefixRange 확인(Prefix에  ${..} 를 포함한 경우 필수
	// Type 확인(leaf인 경우 필수, 그렇지 않으면 무시, leaf여부는 Layer그 없는 경우)
	// DataRange 확인(leaf인 경우 필수)

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
		// Layer가 없는 경우
		if(layer["Type"] == NULL) {
			// Type이 없는 경우
			NDN_LOG_ERROR("Missing 'Layer' or 'Type'");
			return -1;
		}
	}

	// PrefixRange 확인(Prefix에  ${..} 를 포함한 경우 필수
	std::string prefixIndex;
	std::string prefix = layer["Prefix"].as<std::string>();
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

	if(layer["Type"] != NULL) {
		std::string type = layer["Type"].as<std::string>();
		if(caseInsCompare(type, "Int") || caseInsCompare(type, "Integer") || caseInsCompare(type, "Long")) {
			datatype = DataType::Int;
		} else if(caseInsCompare(type, "Real") || caseInsCompare(type, "Float") || caseInsCompare(type, "Double")) {
			datatype = DataType::Real;
		} else if(caseInsCompare(type, "String") || caseInsCompare(type, "Str")) {
			datatype = DataType::String;
		} else {
			NDN_LOG_ERROR(stringf("Unsupported type: %s", type.c_str()));
			return -1;
		}

		NDN_LOG_DEBUG(stringf("Type: %s", type.c_str()));
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

			//load_layer(YAML::Node layer, std::vector<Topic> &topics)

			if(layer["Layer"] == NULL) {
				// Layer가 없는 경우
				topic->setType(datatype);

				if(layer["Sequence"] != NULL) {
					long sequence = layer["Sequence"].as<long>();
					if(0 < sequence) {
						topic->setNextSequence(sequence);
					} else {
						topic->setNextSequence(1);
					}
				}

				if(layer["DataRange"] == NULL) {
					NDN_LOG_ERROR("Missing 'DataRange'");
					return -1;
				}

				if(setDataRange(layer["DataRange"], topic) == -1) {
					return -1;
				}
			} else {
				// 하위 Layer가 있는 경우, 여러개의 Layer
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
		topic->setType(datatype);

		if(layer["Sequence"] != NULL) {
			long sequence = layer["Sequence"].as<long>();
			if(0 < sequence) {
				topic->setNextSequence(sequence);
			} else {
				topic->setNextSequence(1);
			}
		}

		if(layer["DataRange"] == NULL) {
			NDN_LOG_ERROR("Missing 'DataRange'");
			return -1;
		}

		if(setDataRange(layer["DataRange"], topic) == -1) {
			return -1;
		}
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

void makeTopicList(std::vector<TopicPtr> &topics, std::string parent, boost::unordered_map<std::string, TopicPtr> &topicmap) {

	std::vector<TopicPtr>::iterator layer1iter;
	for(layer1iter = topics.begin(); layer1iter != topics.end(); layer1iter++) {
		TopicPtr layer1Topic = *layer1iter;
		std::string layer1name = layer1Topic->getName();

		if(layer1Topic->topicsSize() == 0) {
			if(layer1Topic->getType() != DataType::None) {
				std::string name(stringf("%s/%s", parent.c_str(), layer1name.c_str()));
				layer1Topic->setFullName(name);
				topicmap[name] = layer1Topic;
			}
		} else {
			std::string name(stringf("%s/%s", parent.c_str(), layer1name.c_str()));
			makeTopicList(layer1Topic->getTopics(), name, topicmap);
		}
	}
}

ssize_t findParametersDigestComponent(const Name &name) {
	ssize_t pos = -1;
	for (ssize_t i = 0; i < static_cast<ssize_t>(name.size()); i++) {
		if (name[i].isParametersSha256Digest()) {
			if (pos != -1)
				return -2;
			pos = i;
		}
	}
	return pos;
}

void sendPA(std::string topicName) {

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		return;
	}

	TopicPtr topic = iter->second;

	Name pa_name("/rn");
	pa_name.append("PA");
	pa_name.append(topic->getFullName());

	InterestPtr interest = nullptr;
	interest = std::make_shared<ndn::Interest>(pa_name);
	interest->setCanBePrefix(gPAInterestOption->m_canBePrefix);
	interest->setMustBeFresh(gPAInterestOption->m_mustBeFresh);
	interest->setInterestLifetime(time::milliseconds(gPAInterestOption->m_lifetime));
	if(gPAInterestOption->m_hopLimit.has_value()) {
		interest->setHopLimit(gPAInterestOption->m_hopLimit);
	}

	std::string appParam("{\"pubinfo\": null, \"irinfo\": null}");
	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest->setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	dataHandler=[topic](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("PA Data Name: " << data);

		gStatPA.increase_data();

		gStatPA.checkEndTime();

		gStatPA.decrease_topic();

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("PA Data Content: " << value);

		std::string uri;
		const Name &name = data.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gPATimeMap.find(uri);
		if(iter != gPATimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatPA.increaseWaitTime(diff);
			gStatPA.saveWaitTime(uri, iter->second, tv, diff);
		}
	};

	// Interest에 대한 Nack 핸들러를 준비한다.
	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_DEBUG_SS("PA Nack: " << name << ", Reason: " << nack.getReason());

		gStatPA.increase_nack();

		gStatPA.checkEndTime();

		gStatPA.decrease_topic();
	};

	// Interest에 대한 Timeout 핸들러를 준비한다.
	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_DEBUG_SS("PA Timeout: " << interest);

		gStatPA.increase_timeout();

		gStatPA.checkEndTime();

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

		NDN_LOG_DEBUG_SS("Retry PA: " << dataname.toUri());
		sendPA(dataname.toUri());
	};

	NDN_LOG_DEBUG_SS("Send PA Interest: " << *interest);
	gFace->expressInterest(*interest, dataHandler, nackHandler, timeoutHandler);

	gStatPA.increase_interest();

	std::string uri = pa_name.toUri();

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gPATimeMap[uri] = tv;
}

void sendPU(std::string topicName) {

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		return;
	}

	TopicPtr topic = iter->second;

	Name pu_name("/rn");
	pu_name.append("PU");
	pu_name.append(topic->getFullName());

	InterestPtr interest = nullptr;
	interest = std::make_shared<ndn::Interest>(pu_name);
	interest->setCanBePrefix(gPUInterestOption->m_canBePrefix);
	interest->setMustBeFresh(gPUInterestOption->m_mustBeFresh);
	interest->setInterestLifetime(time::milliseconds(gPUInterestOption->m_lifetime));
	if(gPUInterestOption->m_hopLimit.has_value()) {
		interest->setHopLimit(gPUInterestOption->m_hopLimit);
	}

	std::string appParam("{\"pubinfo\": null, \"irinfo\": null}");
	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest->setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	dataHandler=[topic](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("PU Data Name: " << data);

		gStatPU.increase_data();

		gStatPU.checkEndTime();

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("PU Data Content: " << value);

		std::string uri;
		const Name &name = data.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gPUTimeMap.find(uri);
		if(iter != gPUTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatPU.increaseWaitTime(diff);
			gStatPU.saveWaitTime(uri, iter->second, tv, diff);
		}
	};

	// Interest에 대한 Nack 핸들러를 준비한다.
	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_DEBUG_SS("PU Nack: " << name << ", Reason: " << nack.getReason());

		gStatPU.increase_nack();

		gStatPU.checkEndTime();
	};

	// Interest에 대한 Timeout 핸들러를 준비한다.
	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_DEBUG_SS("PU Timeout: " << interest);

		gStatPU.increase_timeout();

		gStatPU.checkEndTime();

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

		sendPU(dataname.toUri());
	};


	NDN_LOG_DEBUG_SS("Send PU Interest: " << *interest);
	gFace->expressInterest(*interest, dataHandler, nackHandler, timeoutHandler);

	gStatPU.increase_interest();

	std::string uri = pu_name.toUri();

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gPUTimeMap[uri] = tv;
}

void sendPD(std::string topicName, long round) {

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		return;
	}

	TopicPtr topic = iter->second;
	long sequence = topic->getNextSequence();

	Name pd_name("/rn");
	pd_name.append("PD");
	pd_name.append(topic->getFullName());
	pd_name.append(std::to_string(sequence));
	//name.appendVersion();

	InterestPtr interest = nullptr;
	interest = std::make_shared<ndn::Interest>(pd_name);
	interest->setCanBePrefix(gPDInterestOption->m_canBePrefix);
	interest->setMustBeFresh(gPDInterestOption->m_mustBeFresh);
	interest->setInterestLifetime(time::milliseconds(gPDInterestOption->m_lifetime));
	if(gPDInterestOption->m_hopLimit.has_value()) {
		interest->setHopLimit(gPDInterestOption->m_hopLimit);
	}

	std::string appParam;
	if(0 <= cfg_PDContentSize) {
		std::stringstream ss;
		ss << "{\"pubinfo\": { \"data\": \"";
		ss << cfg_PDContent;
		ss << "\"}, \"irinfo\": null}";
		appParam = ss.str();
	} else {
		if(topic->getType() == DataType::Int) {
			long data;
			if(topic->generateData(data)) {
				appParam = stringf("{\"pubinfo\": { \"data\": %ld }, \"irinfo\": null}", data);
			}

		} else if(topic->getType() == DataType::Real) {
			double data;
			if(topic->generateData(data)) {
				appParam = stringf("{\"pubinfo\": { \"data\": %lf }, \"irinfo\": null}", data);
			}
		} else if(topic->getType() == DataType::String) {
			std::string data;
			if(topic->generateData(data)) {
				appParam = stringf("{\"pubinfo\": { \"data\": \"%s\" }, \"irinfo\": null}", data.c_str());
			}
		}
	}

	Block parameters = makeStringBlock(tlv::ApplicationParameters, appParam);
	interest->setApplicationParameters(parameters);

	std::function<void(const Interest &, const Data &)> dataHandler;
	std::function<void(const Interest &, const lp::Nack &)> nackHandler;
	std::function<void(const Interest &)> timeoutHandler;

	dataHandler=[=](const Interest &interest, const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("PD Data Name: " << data);

		gStatPD.increase_data();

		gStatPD.checkEndTime();

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("PD Data Content: " << value);

		std::string uri;
		const Name &name = data.getName();
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gPDTimeMap.find(uri);
		if(iter != gPDTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatPD.increaseWaitTime(diff);
			gStatPD.saveWaitTime(uri, iter->second, tv, diff);
		}

		// round 수 만큼 일정 간격으로 반복해서 보낸다.
		if(round+1 < cfg_PDRound) {
			time::microseconds interval(cfg_PDGenerateInterval);
			gScheduler->schedule(interval, [=] {
				sendPD(topicName, round+1);
			});
		} else {
			gStatPD.decrease_topic();
		}
	};

	// Interest에 대한 Nack 핸들러를 준비한다.
	nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();
		NDN_LOG_DEBUG_SS("PD Nack: " << name << ", Reason: " << nack.getReason());

		gStatPD.increase_nack();

		gStatPD.checkEndTime();
	};

	// Interest에 대한 Timeout 핸들러를 준비한다.
	timeoutHandler=[=](const Interest &interest) {
		NDN_LOG_DEBUG_SS("PD Timeout: " << interest);

		gStatPD.increase_timeout();

		gStatPD.checkEndTime();

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
		sendPD(topicName, round);
#endif
	};

	NDN_LOG_DEBUG_SS("Send PD Interest: " << *interest);
	gFace->expressInterest(*interest, dataHandler, nackHandler, timeoutHandler);

	gStatPD.increase_interest();

	std::string uri = pd_name.toUri();

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gPDTimeMap[uri] = tv;
}

void sendPA(std::shared_ptr<std::vector<std::string>> topics) {

	std::string topicName = topics->back();
	topics->pop_back();

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		time::microseconds interval(0);
		gScheduler->schedule(interval, [=] {
			sendPA(topics);
		});

		return;
	}

	sendPA(topicName);

	if(topics->empty()) {
		return;
	}

	time::microseconds interval(cfg_PAinterval);
	gScheduler->schedule(interval, [=] {
		sendPA(topics);
	});
}

/*void sendPU(std::shared_ptr<std::vector<std::string>> topics) {

	std::string topicName = topics->back();
	topics->pop_back();

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		time::microseconds interval(0);
		gScheduler->schedule(interval, [=] {
			sendPU(topics);
		});

		return;
	}

	sendPU(topicName);

	if(topics->empty()) {
		return;
	}

	time::microseconds interval(cfg_PUinterval);
	gScheduler->schedule(interval, [=] {
		sendPU(topics);
	});
}*/

void sendPD(std::shared_ptr<std::vector<std::string>> topics) {

	std::string topicName = topics->back();
	topics->pop_back();

	TopicMap::iterator iter = gTopicmap.find(topicName);
	if(iter == gTopicmap.end()) {
		time::microseconds interval(0);
		gScheduler->schedule(interval, [=] {
			sendPD(topics);
		});

		return;
	}

	sendPD(topicName, 0);

	if(topics->empty()) {
		return;
	}

	time::microseconds interval(cfg_PDinterval);
	gScheduler->schedule(interval, [=] {
		sendPD(topics);
	});
}

void scheduleSendPA(TopicMap &topicmap) {

	std::shared_ptr<std::vector<std::string>> topics = std::make_shared<std::vector<std::string>>();
	boost::int_least64_t offset = 0;
	// Topic을 조회한다.
	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		topics->push_back(iter->first);
	}

	gStatPA.checkStartTime();

	time::microseconds interval(0);
	gScheduler->schedule(interval, [=] {
		sendPA(topics);
	});
}

void scheduleSendPU(TopicMap &topicmap) {

	std::shared_ptr<std::vector<std::string>> topics = std::make_shared<std::vector<std::string>>();
	boost::int_least64_t offset = 0;
	// Topic을 조회한다.
	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		topics->push_back(iter->first);
	}

	gStatPU.checkStartTime();

	time::microseconds interval(0);
	gScheduler->schedule(interval, [=] {
		sendPU(topics);
	});
}

void scheduleSendPD(TopicMap &topicmap) {

	std::shared_ptr<std::vector<std::string>> topics = std::make_shared<std::vector<std::string>>();
	boost::int_least64_t offset = 0;
	// Topic을 조회한다.
	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		topics->push_back(iter->first);
	}

	gStatPD.checkStartTime();

	time::microseconds interval(0);
	gScheduler->schedule(interval, [=] {
		sendPD(topics);
	});
}

void check_complete() {
	time::seconds wait(1);
	gScheduler->schedule(wait, [=] {
		check_complete();
	});

	if(gStatPA.is_complete() == false) {
		return;
	}

	if(gStatPD.is_complete() == false) {
		return;
	}

	if(gStatPU.is_complete() == false) {
		return;
	}

	
	NDN_LOG_INFO("Complete");

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
		usage(argc, argv);
		return -1;
	}

	NDN_LOG_INFO(opt_confpath);

	std::ifstream input(opt_confpath);

	try {
		YAML::Node doc = YAML::Load(input);

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

		std::stringstream ss;
		ss << doc;
		NDN_LOG_DEBUG(ss.str());

		if(doc["Config"] == NULL) {
			NDN_LOG_INFO("Missing 'Config' section");
			return -1;
		} else {
			load_config(doc["Config"]);
		}

		if(doc["PacketOptions"] == NULL) {
			gInterestOption = std::make_shared<InterestOption>();
			gPAInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			gPDInterestOption = std::make_shared<InterestOption>(*gInterestOption);
			gPUInterestOption = std::make_shared<InterestOption>(*gInterestOption);
		} else {
			load_packet_options(doc["PacketOptions"]);
		}

		YAML::Node publisher = doc["Publisher"];
		if(publisher == NULL) {
			NDN_LOG_ERROR("Missing 'Publisher' section");
			return -1;
		}

		// Layer 1 topics
		YAML::Node::iterator iter;
		for(iter = publisher.begin(); iter != publisher.end(); ++ iter) {
			std::string name = iter->first.as<std::string>();

			if(name.compare("Layer") != 0) {
				continue;
			}

			if(load_layer(iter->second, gTopics) == -1) {
				return -1;
			}
		}

		gIOService = std::make_shared<boost::asio::io_service>();
		gFace = std::make_shared<Face>(*gIOService);
		gScheduler = std::make_shared<Scheduler>(*gIOService);

		// topic을 순회하여 map(topic name, topic)을 생성한다.
		makeTopicList(gTopics, "", gTopicmap);

		if(gTestPA) {
			gStatPA.increase_topic(gTopicmap.size());

			// topicmap을 순회하여 PA Integerst를 전송한다.
			scheduleSendPA(gTopicmap);
		}

		if(gTestPU) {
			gStatPU.increase_topic(gTopicmap.size());

			// topicmap을 순회하여 PA Integerst를 전송한다.
			scheduleSendPU(gTopicmap);
		}

		if(gTestPD) {
			gStatPD.increase_topic(gTopicmap.size());

			// topicmap을 순회하여 PA Integerst를 전송한다.
			scheduleSendPD(gTopicmap);
		}

		time::seconds wait(1);
		gScheduler->schedule(wait, [=] {
			check_complete();
		});

		if(0 < cfg_duration) {
			time::microseconds duration(cfg_duration);
			gScheduler->schedule(duration, [=] {
				NDN_LOG_INFO("Timeout Duration");
				gIOService->stop();
			});
		}

		bool  is_stop = false;
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

		if(gTestPA) {
			NDN_LOG_INFO(stringf("   Start PA: %s", gStatPA.getStartTimeString().c_str()));
			NDN_LOG_INFO(stringf("     End PA: %s", gStatPA.getEndTimeString().c_str()));
			struct timeval diff = timeval_diff(&gStatPA.m_start, &gStatPA.m_end);
			NDN_LOG_INFO(stringf("    Time PA: %ld.%06ld", diff.tv_sec, diff.tv_usec));
			if(0 < gStatPA.m_data) {
				int64_t avg = (gStatPA.m_wait.tv_sec*1000000 + gStatPA.m_wait.tv_usec)/gStatPA.m_data;
				NDN_LOG_INFO(stringf("     AVG PA: %ld.%06ld", avg/1000000, avg%1000000));
			} else {
				NDN_LOG_INFO(stringf("     AVG PA: %s", gStatPA.getWaitTimeString().c_str()));
			}
			NDN_LOG_INFO(stringf("Interest PA: %d", gStatPA.m_interest));
			NDN_LOG_INFO(stringf("    Data PA: %d", gStatPA.m_data));
			NDN_LOG_INFO(stringf("    NACK PA: %d", gStatPA.m_nack));
			NDN_LOG_INFO(stringf(" Timeout PA: %d", gStatPA.m_timeout));
			NDN_LOG_INFO(stringf(" Each PA List: %d", gStatPA.m_waitList.size()));
			if(cfg_PAPrintDetailTimeData) {
				std::vector<TimeData>::iterator dataiter = gStatPA.m_waitList.begin();
				for(; dataiter != gStatPA.m_waitList.end(); dataiter++) {
					TimeData &data = *dataiter;
					NDN_LOG_INFO(stringf(" %s: %d.%06ld - %d.%06ld = %d.%06ld", data.m_name.c_str(),
							data.m_tv1.tv_sec, data.m_tv1.tv_usec,
							data.m_tv2.tv_sec, data.m_tv2.tv_usec,
							data.m_tv3.tv_sec, data.m_tv3.tv_usec));
				}
			}
		}

		if(gTestPD) {
			NDN_LOG_INFO(stringf("   Start PD: %s", gStatPD.getStartTimeString().c_str()));
			NDN_LOG_INFO(stringf("     End PD: %s", gStatPD.getEndTimeString().c_str()));
			struct timeval diff = timeval_diff(&gStatPD.m_start, &gStatPD.m_end);
			NDN_LOG_INFO(stringf("    Time PD: %ld.%06ld", diff.tv_sec, diff.tv_usec));
			if(0 < gStatPD.m_data) {
				int64_t avg = (gStatPD.m_wait.tv_sec*1000000 + gStatPD.m_wait.tv_usec)/gStatPD.m_data;
				NDN_LOG_INFO(stringf("     AVG PD: %ld.%06ld", avg/1000000, avg%1000000));
			} else {
				NDN_LOG_INFO(stringf("     AVG PD: %s", gStatPD.getWaitTimeString().c_str()));
			}
			NDN_LOG_INFO(stringf("Interest PD: %d", gStatPD.m_interest));
			NDN_LOG_INFO(stringf("    Data PD: %d", gStatPD.m_data));
			NDN_LOG_INFO(stringf("    NACK PD: %d", gStatPD.m_nack));
			NDN_LOG_INFO(stringf(" Timeout PD: %d", gStatPD.m_timeout));
			NDN_LOG_INFO(stringf(" Each PD List: %d", gStatPD.m_waitList.size()));
			if(cfg_PDPrintDetailTimeData) {
				std::vector<TimeData>::iterator dataiter = gStatPD.m_waitList.begin();
				for(; dataiter != gStatPD.m_waitList.end(); dataiter++) {
					TimeData &data = *dataiter;
					NDN_LOG_INFO(stringf(" %s: %d.%06ld - %d.%06ld = %d.%06ld", data.m_name.c_str(),
							data.m_tv1.tv_sec, data.m_tv1.tv_usec,
							data.m_tv2.tv_sec, data.m_tv2.tv_usec,
							data.m_tv3.tv_sec, data.m_tv3.tv_usec));
				}
			}
		}

		if(gTestPU) {
			NDN_LOG_INFO(stringf("   Start PU: %s", gStatPU.getStartTimeString().c_str()));
			NDN_LOG_INFO(stringf("     End PU: %s", gStatPU.getEndTimeString().c_str()));
			struct timeval diff = timeval_diff(&gStatPU.m_start, &gStatPU.m_end);
			NDN_LOG_INFO(stringf("    Time PU: %ld.%06ld", diff.tv_sec, diff.tv_usec));
			if(0 < gStatPU.m_data) {
				int64_t avg = (gStatPU.m_wait.tv_sec*1000000 + gStatPU.m_wait.tv_usec)/gStatPU.m_data;
				NDN_LOG_INFO(stringf("     AVG PU: %ld.%06ld", avg/1000000, avg%1000000));
			} else {
				NDN_LOG_INFO(stringf("     AVG PU: %s", gStatPU.getWaitTimeString().c_str()));
			}
			NDN_LOG_INFO(stringf("Interest PU: %d", gStatPU.m_interest));
			NDN_LOG_INFO(stringf("    Data PU: %d", gStatPU.m_data));
			NDN_LOG_INFO(stringf("    NACK PU: %d", gStatPU.m_nack));
			NDN_LOG_INFO(stringf(" Timeout PU: %d", gStatPU.m_timeout));
			NDN_LOG_INFO(stringf(" Each PU List: %d", gStatPU.m_waitList.size()));
			if(cfg_PUPrintDetailTimeData) {
				std::vector<TimeData>::iterator dataiter = gStatPU.m_waitList.begin();
				for(; dataiter != gStatPU.m_waitList.end(); dataiter++) {
					TimeData &data = *dataiter;
					NDN_LOG_INFO(stringf(" %s: %d.%06ld - %d.%06ld = %d.%06ld", data.m_name.c_str(),
							data.m_tv1.tv_sec, data.m_tv1.tv_usec,
							data.m_tv2.tv_sec, data.m_tv2.tv_usec,
							data.m_tv3.tv_sec, data.m_tv3.tv_usec));
				}
			}
		}

	} catch (const YAML::Exception &e) {
		NDN_LOG_ERROR(e.what());
		return -1;
	}

	return 0;
}
