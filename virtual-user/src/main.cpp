#include <dirent.h>     /* Defines DT_* constants */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>

#include <memory>
#include <vector>
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

//2021 START
#include "yamlutils.h"
//2021 END

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
long cfg_startDelay = 100;

long cfg_STInterval = 100;

std::string cfg_deviceFile = "area.yaml";
std::string cfg_serviceName = "/rn";

bool cfg_STPrintDetailTimeData = false;
bool cfg_SMPrintDetailTimeData = false;
bool cfg_SDPrintDetailTimeData = false;

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

Statistics gStatALL;
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
		exit(1);
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

int load_config(YAML::Node config) {

	if(config["Duration"] == NULL) {
		NDN_LOG_INFO("Missing 'Duration'");
		NDN_LOG_INFO(stringf("default Duration: %ld", cfg_duration));
	} else {
		cfg_duration = config["Duration"].as<long>();
		NDN_LOG_INFO(stringf("Duration: %ld", cfg_duration));
	}

	if(config["StartDelay"] == NULL) {
		NDN_LOG_INFO("Missing 'StartDelay'");
		NDN_LOG_INFO(stringf("default StartDelay: %ld", cfg_startDelay));
	} else {
		cfg_startDelay = config["StartDelay"].as<long>();
		NDN_LOG_INFO(stringf("StartDelay: %ld", cfg_startDelay));
	}

	if(config["STInterval"] == NULL) {
		NDN_LOG_INFO("Missing 'STInterval'");
		NDN_LOG_INFO(stringf("default STInterval: %ld", cfg_STInterval));
	} else {
		cfg_STInterval = config["STInterval"].as<long>();
		NDN_LOG_INFO(stringf("STInterval: %ld", cfg_STInterval));
	}

	if(config["DeviceFile"] == NULL) {
		NDN_LOG_INFO("Missing 'DeviceFile'");
		NDN_LOG_INFO(stringf("default DeviceFile: %ld", cfg_deviceFile.c_str()));
	} else {
		cfg_deviceFile = config["DeviceFile"].as<std::string>();
		NDN_LOG_INFO(stringf("DeviceFile: %ld", cfg_deviceFile));
	}

	if(config["ServiceName"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.ServiceName'");
		NDN_LOG_INFO(stringf("default ServiceName: %s", cfg_serviceName.c_str()));
	} else {
		cfg_serviceName = config["ServiceName"].as<std::string>();
		NDN_LOG_INFO(stringf("ServiceName: %s", cfg_serviceName.c_str()));
	}

	cfg_STPrintDetailTimeData = yaml::getAsBool(config, "STPrintDetailTimeData", cfg_STPrintDetailTimeData);
	cfg_SMPrintDetailTimeData = yaml::getAsBool(config, "SMPrintDetailTimeData", cfg_SMPrintDetailTimeData);
	cfg_SDPrintDetailTimeData = yaml::getAsBool(config, "SDPrintDetailTimeData", cfg_SDPrintDetailTimeData);

	NDN_LOG_INFO(stringf("ST PrintDetailTimeData: %s", cfg_STPrintDetailTimeData? "true":"false"));
	NDN_LOG_INFO(stringf("SM PrintDetailTimeData: %s", cfg_SMPrintDetailTimeData? "true":"false"));
	NDN_LOG_INFO(stringf("SD PrintDetailTimeData: %s", cfg_SDPrintDetailTimeData? "true":"false"));

	return 0;
}

std::vector<std::string> split(std::string input, char delimiter) {
	 std::vector<std::string> answer;
	 std::stringstream ss(input);
	 std::string temp;

	 while(getline(ss, temp, delimiter)) {
		 answer.push_back(temp);
	 }

	 return answer;
}

int load_layer(YAML::Node doc, YAML::Node layer, YAML::Node doc_device, std::vector<TopicPtr> &topics, std::vector<std::string> tokens) {
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

	if(layer["Fetch"] != NULL) {
		sequence = layer["Fetch"].as<long>();
	}

	// PrefixRange 확인(Prefix에  ${..} 를 포함한 경우 필수
	std::string prefixIndex;
	std::string variableName;
	std::string prefix = layer["Prefix"].as<std::string>();

	// '#' 인 경우 하위는 무시하고 종료한다.
	if(prefix.compare("#") == 0) {

		NDN_LOG_DEBUG(stringf("Prefix: %s", prefix.c_str()));
		TopicPtr topic = std::make_shared<Topic>(prefix);

		if(layer["Layer"] == NULL) {
			topic->setFetchSequence(sequence);
			NDN_LOG_DEBUG(stringf("FETCH: %s", sequence));
		} else {
			YAML::Node::iterator iter;
			for(iter = layer.begin(); iter != layer.end(); ++ iter) {
				std::string name = iter->first.as<std::string>();
				if(name.compare("Layer") != 0) {
					continue;
				}
				std::vector<TopicPtr> &children = topic->getTopics();

				std::vector<std::string> addtokens = tokens;
				addtokens.push_back(prefix);

				if(iter->second.IsMap()) {
					if(load_layer(doc, iter->second, doc_device, children, addtokens) == -1) {
						return -1;
					}
				}else if (iter->second.IsSequence()) {
					for(int li = 0; li < iter->second.size(); li ++) {
						if(load_layer(doc, iter->second[li], doc_device, children, addtokens) == -1) {
							return -1;
						}
					}
				}
			}
		}
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
			variableName = m[1];
			NDN_LOG_DEBUG(stringf("Prefix: %s, Variable part: %s, Name: %s", prefix.c_str(), prefixIndex.c_str(), variableName.c_str()));
		}
	}

	// 가변 prefix 설정
	if(0 < prefixIndex.size()) {
		// 반드시 PrefixRange가 설정되어 있어야 한다.
		if(layer["PrefixRange"] == NULL) {
			return -1;
		}

		// PrefixRange 패턴, 1, 1 ~ 10, 1- 10
		std::string str = layer["PrefixRange"].as<std::string>();
		NDN_LOG_DEBUG(stringf("PrefixRange: %s", str.c_str()));

		std::tuple<long, long> range;
		if (parseIntRange(str, range) == false) {
			return -1;
		}

		long min = std::get<0>(range);
		long max = std::get<1>(range);

		NDN_LOG_DEBUG(stringf("min: %ld, max: %ld",min,max));


		if(prefix.compare(prefixIndex) == 0) { //${} 이러한 형태
			TopicPtr topic = nullptr;
			std::string wildOptionPrefixIndex;
			std::string wildOptionPrefix = variableName.c_str();
			std::size_t found = wildOptionPrefix.find("$");
			if (found == std::string::npos) {
				NDN_LOG_DEBUG(stringf("Prefix: %s", prefix.c_str()));
			} else {
				std::regex wildPattern("\\$\\d");
				std::smatch m2;
				if (regex_search(wildOptionPrefix, m2, wildPattern)) {
					wildOptionPrefixIndex = m2[0];
					NDN_LOG_DEBUG(stringf("wildOptionPrefix: %s, wildOptionPrefixIndex: %s", wildOptionPrefix.c_str(), wildOptionPrefixIndex.c_str()));
				}
			}
			if(0 < wildOptionPrefixIndex.size()) {
				std::string name;
				std::size_t tempFound = wildOptionPrefix.find(wildOptionPrefixIndex);

				wildOptionPrefix.replace(tempFound, wildOptionPrefixIndex.size(), "%s"); // ${$1-vDBT} => ${%s-vBDT}, ${asdasd-$1-vBDT} => ${asdasd-%s-vBDT} 로 변경
				std::string wildNumber;
				std::regex numberPattern("\\d");
				std::smatch m3;
				long wildOption;
				if (regex_search(wildOptionPrefixIndex, m3, numberPattern)) {
					wildNumber = m3[0];
					wildOption = atol(wildNumber.c_str()); //$0 자기자신 - 구현 안함, $1 = 첫번째 레이어의 prefix, $2 = 두번째 레이어의 prefix ...
//					NDN_LOG_DEBUG_SS("number: " << wildOption);
				}
				if(tokens.size() > wildOption - 1){
					name = stringf(wildOptionPrefix, tokens[wildOption - 1].c_str()); //${%s-vBDT} => ${Gangseo-gu-vBDT}로 형태 변경
					NDN_LOG_DEBUG(stringf("name: %s",name.c_str()));
				} else {
					NDN_LOG_DEBUG("null error");
					return -1;
				}

				for(long i = min; i <= max; i ++) {
					std::tuple<std::string, YAML::Node> dest;
					if(yaml::getNode(doc_device, name.c_str(), dest, i)) {
						std::string topicName;
						YAML::Node elmt;

						elmt = get<1>(dest);


						topicName = stringf(elmt.as<std::string>());
						NDN_LOG_DEBUG_SS(stringf("prefix: %s", topicName.c_str()));

						topic = std::make_shared<Topic>(topicName);


						if(layer["Layer"] == NULL) {
							topic->setFetchSequence(sequence);
							NDN_LOG_DEBUG(stringf("Fetch: %ld", sequence));
						} else {
							std::vector<std::string> addtokens = tokens;
							addtokens.push_back(topicName);
							YAML::Node::iterator iter;
							for(iter = layer.begin(); iter != layer.end(); ++ iter) {
								std::string name = iter->first.as<std::string>();
								if(name.compare("Layer") != 0) {
									continue;
								}
								std::vector<TopicPtr> &children = topic->getTopics();
								if(iter->second.IsMap()) {
									if(load_layer(doc, iter->second, doc_device, children, addtokens) == -1) {
										return -1;
									}
								}else if (iter->second.IsSequence()) {
									for(int li = 0; li < iter->second.size(); li ++) {
										if(load_layer(doc, iter->second[li], doc_device, children, addtokens) == -1) {
											return -1;
										}
									}
								}
							}
						}
						topics.push_back(topic);
					}
				}
			} else { //${Area} 이런형태
				for(long i = min; i <= max; i ++) {
					std::tuple<std::string, YAML::Node> dest;
					if(yaml::getNode(doc_device, variableName.c_str(), dest, i)) {
						std::string topicName;
						YAML::Node elmt;
						elmt = get<1>(dest);

						topicName = stringf(elmt.as<std::string>());
						NDN_LOG_DEBUG_SS(stringf("prefix: %s", topicName.c_str()));

						topic = std::make_shared<Topic>(topicName);
						if(layer["Layer"] == NULL) {
							topic->setFetchSequence(sequence);
							NDN_LOG_DEBUG(stringf("Fetch: %ld", sequence));
						} else {
							std::vector<std::string> addtokens = tokens;
							addtokens.push_back(topicName);
							YAML::Node::iterator iter;
							for(iter = layer.begin(); iter != layer.end(); ++ iter) {
								std::string name = iter->first.as<std::string>();
								if(name.compare("Layer") != 0) {
									continue;
								}
								std::vector<TopicPtr> &children = topic->getTopics();
								if(iter->second.IsMap()) {
									if(load_layer(doc, iter->second, doc_device, children, addtokens) == -1) {
										return -1;
									}
								}else if (iter->second.IsSequence()) {
									for(int li = 0; li < iter->second.size(); li ++) {
										if(load_layer(doc, iter->second[li], doc_device, children, addtokens) == -1) {
											return -1;
										}
									}
								}
							}
						}
						topics.push_back(topic);
					}
				}
			}
		} else { //x-${} 같은 형태
			std::string newname = prefix;
			found = newname.find(prefixIndex);
			if (found != std::string::npos) {
				newname.replace(found, prefixIndex.size(), "%ld");
			}
			for(long i = min; i <= max; i ++) {
				std::string topicName = stringf(newname, i);
				NDN_LOG_DEBUG_SS(stringf("prefix: %s", topicName));

				TopicPtr topic = std::make_shared<Topic>(topicName);

				if(layer["Layer"] == NULL) {
					topic->setFetchSequence(sequence);
					NDN_LOG_DEBUG(stringf("Fetch: %ld", sequence));
				} else {
					std::vector<std::string> addtokens = tokens;
					addtokens.push_back(topicName);
					YAML::Node::iterator iter;
					for(iter = layer.begin(); iter != layer.end(); ++ iter) {
						std::string name = iter->first.as<std::string>();
						if(name.compare("Layer") != 0) {
							continue;
						}
						std::vector<TopicPtr> &children = topic->getTopics();
						if(iter->second.IsMap()) {
							if(load_layer(doc, iter->second, doc_device, children, addtokens) == -1) {
								return -1;
							}
						}else if (iter->second.IsSequence()) {
							for(int li = 0; li < iter->second.size(); li ++) {
								if(load_layer(doc, iter->second[li], doc_device, children, addtokens) == -1) {
									return -1;
								}
							}
						}
					}
				}
				topics.push_back(topic);
			}
		}
	} else { // 가변이 아닐 때
		TopicPtr topic = std::make_shared<Topic>(prefix.c_str());
		if(layer["Layer"] == NULL) {
			topic->setFetchSequence(sequence);
			NDN_LOG_DEBUG(stringf("Fetch: %ld", sequence));
		} else {
			std::vector<std::string> addtokens = tokens;
			addtokens.push_back(prefix);
			YAML::Node::iterator iter;
			for(iter = layer.begin(); iter != layer.end(); ++ iter) {
				std::string name = iter->first.as<std::string>();
				if(name.compare("Layer") != 0) {
					continue;
				}
				std::vector<TopicPtr> &children = topic->getTopics();
				if(iter->second.IsMap()) {
					if(load_layer(doc, iter->second, doc_device, children, addtokens) == -1) {
						return -1;
					}
				}else if (iter->second.IsSequence()) {
					for(int li = 0; li < iter->second.size(); li ++) {
						if(load_layer(doc, iter->second[li], doc_device, children, addtokens) == -1) {
							return -1;
						}
					}
				}
			}
		}
		topics.push_back(topic);
	}
	return 0;
}

#if 0
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
					if(load_layer(doc, iter->second, children) == -1) {
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
			if(load_layer(doc, iter->second, children) == -1) {
				return -1;
			}
		}
	}

	topics.push_back(topic);

	return 0;
}
#endif

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
			if(cfg_SDPrintDetailTimeData) {
				gStatSD.saveWaitTime(qname, iter->second, tv, diff);
			}
		}

		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("SD Data Content: " << value);

#if 0
		long seq = std::stol(seqName.get(0).toUri());

		long topicSeq;
		if((topicSeq = topic->getNextSequence()) < 0) {
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

		topic->timeout();
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
#if 0
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
			if(cfg_SMPrintDetailTimeData) {
				gStatSM.saveWaitTime(qname, iter->second, tv, diff);
			}
		}
#endif

		boost::unordered_map<std::string, struct timeval>::iterator iter;
		iter = gSMTimeMap.find(name.toUri());
		if(iter != gSMTimeMap.end()) {
			struct timeval diff = timeval_diff(&iter->second, &tv);
			gStatSM.increaseWaitTime(diff);
			gStatSM.saveWaitTime(name.toUri(), iter->second, tv, diff);
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
			if (status.compare("NOT_FOUND") == 0) {
				time::microseconds interval(0);
				gScheduler->schedule(interval, [rnname, dataname, topic] {
					sendSM(rnname, dataname, topic);
				});
			} else if(status.compare("OK") == 0) {
				long fst = pt.get<long>("fst");
				long lst = pt.get<long>("lst");
				long seq;
				NDN_LOG_DEBUG(stringf("status: %s, fst: %d, lst: %d", status.c_str(), fst, lst));

				seq = lst;
				if(seq < 1) {
					seq = 1;
				}
				topic->setNextSequence(seq);
			}
#if 0
			std::tuple<std::string, std::string> key;
			key = std::make_tuple<std::string, std::string>(rnname.toUri(), dataname.toUri());
			if(topic->hasRepository(key) == false) {
				topic->setRepositorySequence(key, seq);
			}
#endif

			sendSD(rnname, dataname, 0, topic);

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

	Name name(cfg_serviceName);
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
			if (cfg_STPrintDetailTimeData) {
				gStatST.saveWaitTime(uri, iter->second, tv, diff);
			}
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
			PTree::ptree values = pt.get_child("value");

			//기존 :  { status: "OK", values: [["rn-3", "/Gu-1/BDOT-1/temp"]]}
			//현재 : { status: "OK", value: [["rn-3", "/Gu-1/BDOT-1/temp"]]}
			//차이점 : values -> value
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

				NDN_LOG_DEBUG_SS(stringf("rnname: %s, dataname: %s", rnname.c_str(), dataname.c_str()));
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
		gStatALL.checkStartTime();
	}

	gStatST.increase_interest();
}

/**
 * Topic을 조회하여, ST interest를 전송한다.
 * 수신된 Data 들의 content를 확인하여
 * SM Interest를 전송한다.
 */
void scheduleSendST(TopicMap &topicmap) {
	// Topic을 조회한다.
	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		std::string topicName = iter->first;
		TopicPtr topic = iter->second;
		time::microseconds interval(cfg_STInterval);
		gScheduler->schedule(interval, [topicName, topic] {
			sendST(topicName, topic);
		});
	}
}

void scheduleSendST(std::vector<TopicPtr> &topics, int index) {
	// Topic을 조회한다.
	if(topics.size() <= index) {
		return;
	}

	std::string topicName = topics[index]->getFullName();
	sendST(topicName, topics[index]);

	time::microseconds interval(cfg_STInterval);
	gScheduler->schedule(interval, [&topics, index] {
		scheduleSendST(topics, index + 1);
	});
}

int main(int argc, char *argv[]) {
	char path[PATH_MAX];
	char buffer[4096];
	int opt;
	int index;
	char *pos;
	int retval;
	std::string confpath_dir;

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

	pos = realpath(opt_confpath.c_str(), NULL);
	if (pos != NULL) {
		confpath_dir = dirname(pos);
		free(pos);
	}

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
			NDN_LOG_INFO(stringf("default Duration: %ld", cfg_duration));
			NDN_LOG_INFO(stringf("default StartDelay: %ld", cfg_startDelay));
			NDN_LOG_INFO(stringf("default STInterval: %ld", cfg_STInterval));
			NDN_LOG_INFO(stringf("default DeviceFile: %s", cfg_deviceFile.c_str()));
			NDN_LOG_INFO(stringf("default ServiceName: %s", cfg_serviceName.c_str()));
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

		if (cfg_deviceFile.rfind("/", 0) != 0) {
			std::string fullpath = confpath_dir + "/" + cfg_deviceFile;
			cfg_deviceFile = fullpath;
		}

		NDN_LOG_DEBUG_SS(stringf("Device Configure FIle: %s", cfg_deviceFile.c_str()));

		std::ifstream device(cfg_deviceFile);
		YAML::Node doc_device = YAML::Load(device);

		if(doc_device.IsNull()) {
			NDN_LOG_ERROR("Not Found Device File");
			return -1;
		}

		// Layer 1 topics
		std::vector<TopicPtr> topics;
		std::vector<std::string> tokens;
		YAML::Node::iterator iter;
		for(iter = subscriber.begin(); iter != subscriber.end(); ++ iter) {
			std::string name = iter->first.as<std::string>();

			if(name.compare("Layer") != 0) {
				continue;
			}

			if(iter->second.IsMap()) {
				if(load_layer(doc, iter->second, doc_device, topics, tokens) == -1) {
					return -1;
				}
			} else if(iter->second.IsSequence()) {
				for(int li = 0; li < iter->second.size(); li ++) {
					if(load_layer(doc, iter->second[li], doc_device, topics, tokens) == -1) {
						return -1;
					}
				}
			}
		}

		// publisher가 시작하기를 기다린다.
		::usleep(cfg_startDelay);

		// topic을 순회하여 map(topic name, topic)을 생성한다.
		makeTopicList(topics, "", gTopicmap);

		gIOService = std::make_shared<boost::asio::io_service>();
		gFace = std::make_shared<Face>(*gIOService);
		gScheduler = std::make_shared<Scheduler>(*gIOService);

		std::vector<TopicPtr> topicList;

		// topicmap을 순회하여 ST Integerst를 전송한다.
		time::microseconds st_interval(0);
		gScheduler->schedule(st_interval, [&topicList] {
			// check_st(gTopicmap, st_delegators);
#if 0
			scheduleSendST(gTopicmap);
#else
			TopicMap::iterator iter;
			for(iter = gTopicmap.begin(); iter != gTopicmap.end(); iter ++) {
				TopicPtr topic = iter->second;
				topicList.push_back(topic);
			}
			scheduleSendST(topicList, 0);
#endif
		});

		if(0 < cfg_duration) {
			time::seconds duration(cfg_duration);
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

		gStatALL.checkEndTime();

		NDN_LOG_INFO(stringf("   Start ST: %s", gStatST.getStartTimeString().c_str()));
		NDN_LOG_INFO(stringf("     End ST: %s", gStatST.getEndTimeString().c_str()));
		struct timeval diff_st = timeval_diff(&gStatST.m_start, &gStatST.m_end);
		NDN_LOG_INFO(stringf("    Time ST: %ld.%06ld", diff_st.tv_sec, diff_st.tv_usec));
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

		NDN_LOG_INFO(stringf("   Start SM: %s", gStatSM.getStartTimeString().c_str()));
		NDN_LOG_INFO(stringf("     End SM: %s", gStatSM.getEndTimeString().c_str()));
		struct timeval diff_sm = timeval_diff(&gStatSM.m_start, &gStatSM.m_end);
		NDN_LOG_INFO(stringf("    Time SM: %ld.%06ld", diff_sm.tv_sec, diff_sm.tv_usec));
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

		NDN_LOG_INFO(stringf("   Start SD: %s", gStatSD.getStartTimeString().c_str()));
		NDN_LOG_INFO(stringf("     End SD: %s", gStatSD.getEndTimeString().c_str()));
		struct timeval diff_sd = timeval_diff(&gStatSD.m_start, &gStatSD.m_end);
		NDN_LOG_INFO(stringf("    Time SD: %ld.%06ld", diff_sd.tv_sec, diff_sd.tv_usec));
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
		NDN_LOG_INFO(stringf("      Start: %s", gStatALL.getStartTimeString().c_str()));
		NDN_LOG_INFO(stringf("        End: %s", gStatALL.getEndTimeString().c_str()));
		struct timeval diff_all = timeval_diff(&gStatALL.m_start, &gStatALL.m_end);
		NDN_LOG_INFO(stringf("       Time: %ld.%06ld", diff_all.tv_sec, diff_all.tv_usec));

	} catch (const YAML::Exception &e) {
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}
