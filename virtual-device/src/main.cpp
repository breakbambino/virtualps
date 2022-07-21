#include <dirent.h>     /* Defines DT_* constants */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>

#include <memory>
#include <regex>
#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <csignal>
#include <ctime>

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

//2021 add start
#include "yamlutils.h"
#include "pubsub.hpp"

//2021 add end

using YAML::Parser;
//using YAML::MockEventHandler;
using namespace ndn;
using namespace ndn::encoding;

NDN_LOG_INIT(psperf.virtual-device);

std::string opt_confpath;
int opt_argc = 0;
char *opt_argv[1001] = {0, };

uint64_t cfg_duration = 0;
uint64_t cfg_round = 100;
uint64_t cfg_PAinterval = 100;
uint64_t cfg_PUinterval = 100;
uint64_t cfg_PDinterval = 10000000;

std::string cfg_deviceFile = "busan.txt";
std::string cfg_sensorFile = "sensor.txt";
std::string cfg_serviceName = "/rn";

bool cfg_PAPrintDetailTimeData = false;
bool cfg_PDPrintDetailTimeData = false;
bool cfg_PUPrintDetailTimeData = false;

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

boost::unordered_map<std::string, InterestDelegatorPtr> gPAMap;
boost::unordered_map<std::string, InterestDelegatorPtr> gPDMap;
boost::unordered_map<std::string, InterestDelegatorPtr> gPUMap;

using InterestOptionPtr = std::shared_ptr<InterestOption>;

InterestOptionPtr gInterestOption = nullptr;
InterestOptionPtr gPAInterestOption = nullptr;
InterestOptionPtr gPDInterestOption = nullptr;
InterestOptionPtr gPUInterestOption = nullptr;

std::shared_ptr<boost::asio::io_service> gIOService = nullptr;
FacePtr gFace = nullptr;
SchedulerPtr gScheduler = nullptr;

std::vector<TopicPtr> gTopics;

YAML::Node doc;

boost::unordered_map<std::string, TopicPtr> gTopicmap;

boost::unordered_map<std::string, struct timeval> gPATimeMap;
boost::unordered_map<std::string, struct timeval> gPDTimeMap;
boost::unordered_map<std::string, struct timeval> gPUTimeMap;


Statistics gStatPA;
Statistics gStatPD;
Statistics gStatPU;
Statistics gStatALL;
bool is_first_pa = true;
bool is_first_pd = true;
bool is_first_pu = true;
//boost::unordered_map<std::string, Statistics> gStatPD;

rapidjson::Document makeSendPUData(TopicPtr topic);

static
struct option long_options[] = {
	{"config", required_argument, (int *)'\0', (int)'c'},
	{"help", no_argument, 0, (int)'h'},
	{0, 0, 0, 0}
};

void signalHandler( int signum ) {
	NDN_LOG_INFO_SS("Receive SIGNAL: " << signum);

	if(gIOService == nullptr) {
		exit(1);
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

int load_config(YAML::Node config) {

	if(config["Duration"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.Duration'");
		NDN_LOG_INFO(stringf("default duration: %u", cfg_duration));
	} else {
		cfg_duration = config["Duration"].as<uint64_t>();
		NDN_LOG_INFO(stringf("Duration: %u", cfg_duration));
	}

	if(config["Round"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.Round'");
		NDN_LOG_INFO(stringf("default round: %u", cfg_round));
	} else {
		cfg_round = config["Round"].as<uint64_t>();
		NDN_LOG_INFO(stringf("Round: %u", cfg_round));
	}

	if(config["PAInterval"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.PAInterval'");
		NDN_LOG_INFO(stringf("default PAInterval: %u", cfg_PAinterval));
	} else {
		cfg_PAinterval = config["PAInterval"].as<uint64_t>();
		NDN_LOG_INFO(stringf("PAInterval: %u", cfg_PAinterval));
	}
	if(config["PUInterval"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.PUInterval'");
		NDN_LOG_INFO(stringf("default PUInterval: %u", cfg_PUinterval));
	} else {
		cfg_PUinterval = config["PUInterval"].as<uint64_t>();
		NDN_LOG_INFO(stringf("PUInterval: %u", cfg_PUinterval));
	}
	if(config["PDInterval"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.PDInterval'");
		NDN_LOG_INFO(stringf("default PDInterval: %u", cfg_PDinterval));
	} else {
		cfg_PDinterval = config["PDInterval"].as<uint64_t>();
		NDN_LOG_INFO(stringf("PDInterval: %u", cfg_PDinterval));
	}

	if(config["DeviceFile"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.DeviceFile'");
		NDN_LOG_INFO(stringf("default DeviceFile: %s", cfg_deviceFile.c_str()));
	} else {
		cfg_deviceFile = config["DeviceFile"].as<std::string>();
		NDN_LOG_INFO(stringf("DeviceFile: %s", cfg_deviceFile.c_str()));
	}

	if(config["SensorFile"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.SensorFile'");
		NDN_LOG_INFO(stringf("default SensorFile: %s", cfg_sensorFile.c_str()));
	} else {
		cfg_sensorFile = config["SensorFile"].as<std::string>();
		NDN_LOG_INFO(stringf("DeviceFile: %s", cfg_sensorFile.c_str()));
	}

	if(config["ServiceName"] == NULL) {
		NDN_LOG_INFO("Missing 'Config.ServiceName'");
		NDN_LOG_INFO(stringf("default ServiceName: %s", cfg_serviceName.c_str()));
	} else {
		cfg_serviceName = config["ServiceName"].as<std::string>();
		NDN_LOG_INFO(stringf("ServiceName: %s", cfg_serviceName.c_str()));
	}

	cfg_PAPrintDetailTimeData = yaml::getAsBool(config, "PAPrintDetailTimeData", cfg_PAPrintDetailTimeData);
	cfg_PDPrintDetailTimeData = yaml::getAsBool(config, "PDPrintDetailTimeData", cfg_PDPrintDetailTimeData);
	cfg_PUPrintDetailTimeData = yaml::getAsBool(config, "PUPrintDetailTimeData", cfg_PUPrintDetailTimeData);

	NDN_LOG_INFO(stringf("PA PrintDetailTimeData: %s", cfg_PAPrintDetailTimeData? "true":"false"));
	NDN_LOG_INFO(stringf("PU PrintDetailTimeData: %s", cfg_PUPrintDetailTimeData? "true":"false"));
	NDN_LOG_INFO(stringf("PD PrintDetailTimeData: %s", cfg_PDPrintDetailTimeData? "true":"false"));

	return 0;
}

int setDataRange(YAML::Node dataRange, SensorPtr &sensor) {
	DataType datatype = sensor->getType();
	std::string name = sensor->getName();

	if(datatype == DataType::Int) {
		std::regex pattern("([+-]?\\d+)(\\s*[~\\-]\\s*([+-]?\\d+))?");
		std::smatch m;

		std::string str = dataRange.as<std::string>();
		NDN_LOG_DEBUG(stringf("Sensor: %s, Type: Int, DataRange: %s", name.c_str(), str.c_str()));

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
		if(sensor->push_back(minparam) == false) {
			NDN_LOG_ERROR(stringf("Not available: %ld", min));
			return -1;
		}

		ParametizedData *maxparam = new TypedParameter<long>(max);
		if(sensor->push_back(maxparam) == false) {
			NDN_LOG_ERROR(stringf("Not available: %ld", max));
			return -1;
		}
	} else if(datatype == DataType::Real) {
		std::regex pattern("([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?)(\\s*[~\\-]\\s*([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?))?");
		std::smatch m;

		std::string str = dataRange.as<std::string>();
		NDN_LOG_DEBUG(stringf("Sensor: %s, Type: Real, DataRange: %s", name.c_str(), str.c_str()));

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
		if(sensor->push_back(minparam) == false) {
			NDN_LOG_ERROR(stringf("Not available: %lf", min));
			return -1;
		}

		ParametizedData *maxparam = new TypedParameter<double>(max);
		if(sensor->push_back(maxparam) == false) {
			NDN_LOG_ERROR(stringf("Not available: %lf", max));
			return -1;
		}
	} else if(datatype == DataType::String) {
		NDN_LOG_DEBUG(stringf("Sensor: %s, Type: String, DataRange: [", name.c_str()));
		for(int i = 0; i < dataRange.size(); i ++) {
			std::string name = dataRange[i].as<std::string>();
			NDN_LOG_DEBUG(stringf("DataRange: %s", name.c_str()));

			ParametizedData *minparam = new TypedParameter<std::string>(name);
			if(sensor->push_back(minparam) == false) {
				NDN_LOG_ERROR(stringf("Not available: %s", name.c_str()));
				return -1;
			}
		}
		NDN_LOG_DEBUG("]");
	}

	return 0;
}

bool getAsString(YAML::Node n1, YAML::Node n2, std::string &value) {
	std::string ret;
	if (n1 == NULL) {
		if (n2 == NULL) {
			return false;
		}
		ret = n2.as<std::string>();
	} else {
		ret = n1.as<std::string>();
		if (n2 != NULL) {
			ret = n2.as<std::string>();
		}
	}
	value = ret;

	return true;
}

bool getAsDouble(YAML::Node n1, YAML::Node n2, double &value) {
	double ret;
	if (n1 == NULL) {
		if (n2 == NULL) {
			return false;
		}
		ret = n2.as<double>();
	} else {
		ret = n1.as<double>();
		if (n2 != NULL) {
			ret = n2.as<double>();
		}
	}
	value = ret;

	return true;
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

int load_layer(YAML::Node doc, YAML::Node layer, YAML::Node doc_device, std::vector<TopicPtr> &Topics, TopicPtr preTopic, std::vector<std::string> tokens) {

	DataType datatype = DataType::None;
//	std::vector<TopicPtr> &topics = Topics->getTopics();


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

	// PrefixRange 확인(Prefix에  ${..} 를 포함한 경우 필수
	std::string prefixIndex;
	std::string prefix = layer["Prefix"].as<std::string>();
	std::string variableName;

	std::size_t found = prefix.find("${");
	if (found == std::string::npos) {
		NDN_LOG_DEBUG(stringf("Prefix: %s", prefix.c_str()));
	} else {
		std::regex pattern("\\$\\{\\s*([^\\}\\s]+)\\s*\\}");
		std::smatch m;
		if (regex_search(prefix, m, pattern)) {
			prefixIndex = m[0];
			variableName = m[1];
			NDN_LOG_DEBUG(stringf("YAML Prefix: %s, Variable part: %s, Name: %s", prefix.c_str(), prefixIndex.c_str(), variableName.c_str()));
		}
	}


	// 가변 prefix 설정
	if(0 < prefixIndex.size()) {
		// 반드시 PrefixRange가 설정되어 있어야 한다.
		if(layer["PrefixRange"] == NULL) {
			return -1;
		}

		// PrefixRange 패턴, 1, 1 ~ 10, 1 - 10
		std::string str = layer["PrefixRange"].as<std::string>();
		NDN_LOG_DEBUG(stringf("PrefixRange: %s", str.c_str()));

		std::tuple<long, long> range;
		if(parseIntRange(str, range) == false){
			return -1;
		}

		long min = std::get<0>(range);
		long max = std::get<1>(range);

		// $1찾기
		if(prefix.compare(prefixIndex) == 0) {
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
					NDN_LOG_DEBUG_SS("number: " << wildOption);
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

						TopicPtr topic = nullptr;

						if(elmt.IsScalar()) {
							topicName = stringf(elmt.as<std::string>());
							NDN_LOG_DEBUG_SS(stringf("prefix: %s", topicName));

							// 현재 Layer의 topic
							topic = std::make_shared<Topic>(topicName);

							std::vector<std::string> addtokens = tokens;
							addtokens.push_back(topicName);

							if(layer["Sequence"]!= NULL){
								long sequence = layer["Sequence"].as<long>();
								if(0 < sequence) {
									topic->setNextSequence(sequence);
								} else {
									topic->setNextSequence(1);
								}
								NDN_LOG_DEBUG_SS("Sequence: " << sequence);
							}

							if(preTopic != nullptr){
								if(preTopic->getModelName().size() > 0){
									topic->setModelName(preTopic->getModelName());
								}
								if(preTopic->getSerial().size() > 0){
									topic->setSerial(preTopic->getSerial());
								}
								if(preTopic->getLat() != 0){
									topic->setLat(preTopic->getLat());
								}
								if(preTopic->getLon() != 0){
									topic->setLon(preTopic->getLon());
								}
							}

							if(layer["Layer"] == NULL){
								bool lifeDevice;
								bool envDevice;
								bool disasterDevice;

								if(layer["LifeDevice"] != NULL) {
									lifeDevice = layer["LifeDevice"].as<bool>();
									NDN_LOG_DEBUG_SS("LifeDevice: " << lifeDevice);
									if( lifeDevice == false ){
										NDN_LOG_DEBUG_SS("LifeDevice Required item");
										return -1;
									}
								} else {
									NDN_LOG_DEBUG_SS("LifeDevice Required item");
									return -1;
								}

								if(layer["EnvDevice"] != NULL) {
									envDevice = layer["EnvDevice"].as<bool>();
								} else {
									envDevice = false;
								}

								if(layer["DisasterDevice"] != NULL) {
									disasterDevice = layer["DisasterDevice"].as<bool>();
								} else {
									disasterDevice = false;
								}
								topic->setLifeDevice(lifeDevice);
								topic->setEnvDevice(envDevice);
								topic->setDisasterDevice(disasterDevice);

								NDN_LOG_DEBUG_SS("EnvDevice: " << envDevice);
								NDN_LOG_DEBUG_SS("DisasterDevice: " << disasterDevice);

							} else {
								YAML::Node::iterator iter;
								for(iter = layer.begin(); iter != layer.end(); ++ iter) {
									std::string name = iter->first.as<std::string>();

									if(name.compare("Layer") != 0) {
										continue;
									}

									std::vector<TopicPtr> &children = topic->getTopics();
									if(iter->second.IsMap()) {
										if(load_layer(doc, iter->second, doc_device, children, topic, addtokens) == -1) {
											return -1;
										}
									} else if(iter->second.IsSequence()) {
										for(int li = 0; li < iter->second.size(); li ++) {
											if(load_layer(doc, iter->second[li], doc_device, children, topic, addtokens) == -1) {
												return -1;
											}
										}
									}
								}
							}

							Topics.push_back(topic);
						} else if(elmt.IsMap()) {
							YAML::Node::iterator iter;
							int j;

							std::vector<std::string> addtokens = tokens;
							for(iter = elmt.begin(), j = 0; iter != elmt.end(); ++ iter, ++ j) {
								topicName = stringf(iter->first.Scalar());

								NDN_LOG_DEBUG_SS(stringf("prefix: %s", topicName.c_str()));

								topic = std::make_shared<Topic>(topicName);
								addtokens.push_back(topicName);

								YAML::Node value = iter->second;

								std::string VirtualmodelName;
								std::string VirtualSerial;
								double VirtualLat;
								double VirtualLon;

								if(getAsString(value["modelName"], layer["modelName"], VirtualmodelName) == false){
									VirtualmodelName = stringf("vBDT");
								} else {
									topic->setModelName(VirtualmodelName);
								}
								NDN_LOG_DEBUG_SS("modelName: " << VirtualmodelName);

								if(getAsString(value["serial"], layer["serial"], VirtualSerial) == false){
									VirtualSerial = stringf(topicName);
								} else {
									topic->setSerial(VirtualSerial);
								}
								NDN_LOG_DEBUG_SS("serial: " << VirtualSerial);

								if(getAsDouble(value["Lat"], layer["Lat"], VirtualLat) == false) {
								} else {
									topic->setLat(VirtualLat);
									NDN_LOG_DEBUG_SS("Lat: " << VirtualLat);
								}

								if(getAsDouble(value["Lon"], layer["Lon"], VirtualLon) == false) {
								} else {
									topic->setLon(VirtualLon);
									NDN_LOG_DEBUG_SS("Lon: " << VirtualLon);
								}

								if(layer["Sequence"] != NULL) {
									long sequence = layer["Sequence"].as<long>();
									if(0 < sequence) {
										topic->setNextSequence(sequence);
									} else {
										topic->setNextSequence(1);
									}
									NDN_LOG_DEBUG_SS("Sequence: " << sequence);
								}

								if(preTopic != nullptr){
									if(preTopic->getModelName().size() > 0){
										topic->setModelName(preTopic->getModelName());
									}
									if(preTopic->getSerial().size() > 0){
										topic->setSerial(preTopic->getSerial());
									}
									if(preTopic->getLat() != 0){
										topic->setLat(preTopic->getLat());
									}
									if(preTopic->getLon() != 0){
										topic->setLon(preTopic->getLon());
									}
								}

								if(layer["Layer"] == NULL){
									bool lifeDevice;
									bool envDevice;
									bool disasterDevice;
									if(layer["LifeDevice"] != NULL) {
										lifeDevice = layer["LifeDevice"].as<bool>();
										NDN_LOG_DEBUG_SS("LifeDevice: " << lifeDevice);
										if( lifeDevice == false ){
											NDN_LOG_DEBUG_SS("LifeDevice Required item");
											return -1;
										}
									} else {
										NDN_LOG_DEBUG_SS("LifeDevice Required item");
										return -1;
									}
									if(layer["EnvDevice"] != NULL) {
										envDevice = layer["EnvDevice"].as<bool>();
									} else {
										envDevice = false;
									}
									if(layer["DisasterDevice"] != NULL) {
										disasterDevice = layer["DisasterDevice"].as<bool>();
									} else {
										disasterDevice = false;
									}

									topic->setLifeDevice(lifeDevice);
									topic->setEnvDevice(envDevice);
									topic->setDisasterDevice(disasterDevice);

									NDN_LOG_DEBUG_SS("EnvDevice: " << envDevice);
									NDN_LOG_DEBUG_SS("DisasterDevice: " << disasterDevice);
								} else {
									YAML::Node::iterator iter2;
									for(iter2 = layer.begin(); iter2 != layer.end(); ++ iter2) {
										std::string name = iter2->first.as<std::string>();

										if(name.compare("Layer") != 0) {
											continue;
										}

										std::vector<TopicPtr> &children = topic->getTopics();
										if(iter2->second.IsMap()) {
											if(load_layer(doc, iter2->second, doc_device, children, topic, addtokens) == -1) {
												return -1;
											}
										} else if(iter2->second.IsSequence()) {
											for(int li = 0; li < iter2->second.size(); li ++) {
												if(load_layer(doc, iter2->second[li], doc_device, children, topic, addtokens) == -1) {
													return -1;
												}
											}
										}
									}
								}
								Topics.push_back(topic);
							}
					    }
				    }
				}
			} else { //${Area} 같은 형태
				for(long i = min; i <= max; i ++) {
					std::tuple<std::string, YAML::Node> dest;

					if(yaml::getNode(doc_device, variableName.c_str(), dest, i)) {
						std::string topicName;
						YAML::Node elmt;
						elmt = get<1>(dest);

						TopicPtr topic = nullptr;

						if(elmt.IsScalar()) {
							topicName = stringf(elmt.as<std::string>());
							NDN_LOG_DEBUG_SS(stringf("prefix: %s",topicName.c_str()));

							// 현재 Layer의 topic
							topic = std::make_shared<Topic>(topicName);

							std::vector<std::string> addtokens = tokens;
							addtokens.push_back(topicName);
							if(layer["Sequence"] != NULL) {
								long sequence = layer["Sequence"].as<long>();
								if(0 < sequence) {
									topic->setNextSequence(sequence);
								} else {
									topic->setNextSequence(1);
								}
								NDN_LOG_DEBUG_SS("Sequence: " << sequence);
							}
							if(preTopic != nullptr){
								if(preTopic->getModelName().size() > 0){
									topic->setModelName(preTopic->getModelName());
								}
								if(preTopic->getSerial().size() > 0){
									topic->setSerial(preTopic->getSerial());
								}
								if(preTopic->getLat() != 0){
									topic->setLat(preTopic->getLat());
								}
								if(preTopic->getLon() != 0){
									topic->setLon(preTopic->getLon());
								}
							}


							if(layer["Layer"] == NULL){
								bool lifeDevice;
								bool envDevice;
								bool disasterDevice;
								if(layer["LifeDevice"] != NULL) {
									lifeDevice = layer["LifeDevice"].as<bool>();
									NDN_LOG_DEBUG_SS("LifeDevice: " << lifeDevice);
									if( lifeDevice == false ){
										NDN_LOG_DEBUG_SS("LifeDevice Required item");
										return -1;
									}
								} else {
									NDN_LOG_DEBUG_SS("LifeDevice Required item");
									return -1;
								}
								if(layer["EnvDevice"] != NULL) {
									envDevice = layer["EnvDevice"].as<bool>();
								} else {
									envDevice = false;
								}
								if(layer["DisasterDevice"] != NULL) {
									disasterDevice = layer["DisasterDevice"].as<bool>();
								} else {
									disasterDevice = false;
								}

								topic->setLifeDevice(lifeDevice);
								topic->setEnvDevice(envDevice);
								topic->setDisasterDevice(disasterDevice);

								NDN_LOG_DEBUG_SS("EnvDevice: " << envDevice);
								NDN_LOG_DEBUG_SS("DisasterDevice: " << disasterDevice);

							} else {
								YAML::Node::iterator iter;
								for(iter = layer.begin(); iter != layer.end(); ++ iter) {
									std::string name = iter->first.as<std::string>();

									if(name.compare("Layer") != 0) {
										continue;
									}

									std::vector<TopicPtr> &children = topic->getTopics();
									if(iter->second.IsMap()) {
										if(load_layer(doc, iter->second, doc_device, children, topic, addtokens) == -1) {
											return -1;
										}
									} else if(iter->second.IsSequence()) {
										for(int li = 0; li < iter->second.size(); li ++) {
											if(load_layer(doc, iter->second[li], doc_device, children, topic, addtokens) == -1) {
												return -1;
											}
										}
									}
								}
							}
							Topics.push_back(topic);
						}
					}
				}
			}
		} else { // x-${} 같은 형태
			std::string newname = prefix;
			found = newname.find(prefixIndex);
			if (found != std::string::npos) {
				newname.replace(found, prefixIndex.size(), "%ld");
			}

			std::vector<std::string> addtokens = tokens;


			for(long i = min; i <= max; i ++) {
				std::string name = stringf(newname, i);
				NDN_LOG_DEBUG_SS(stringf("prefix: %s",name));

				// 현재 Layer의 topic
				TopicPtr topic = std::make_shared<Topic>(name);
				addtokens.push_back(name);
				if(layer["Sequence"] != NULL) {
					long sequence = layer["Sequence"].as<long>();
					if(0 < sequence) {
						topic->setNextSequence(sequence);
					} else {
						topic->setNextSequence(1);
					}
					NDN_LOG_DEBUG_SS("Sequence: " << sequence);
				}

				if(preTopic != nullptr){
					if(preTopic->getModelName().size() > 0){
						topic->setModelName(preTopic->getModelName());
					}
					if(preTopic->getSerial().size() > 0){
						topic->setSerial(preTopic->getSerial());
					}
					if(preTopic->getLat() != 0){
						topic->setLat(preTopic->getLat());
					}
					if(preTopic->getLon() != 0){
						topic->setLon(preTopic->getLon());
					}
				}

				if(layer["Layer"] == NULL) {
					bool lifeDevice;
					bool envDevice;
					bool disasterDevice;

					if(layer["LifeDevice"] != NULL) {
						lifeDevice = layer["LifeDevice"].as<bool>();
						NDN_LOG_DEBUG_SS("LifeDevice: " << lifeDevice);
						if( lifeDevice == false ){
							NDN_LOG_DEBUG_SS("LifeDevice Required item");
							return -1;
						}
					} else {
						NDN_LOG_DEBUG_SS("LifeDevice Required item");
						return -1;
					}
					if(layer["EnvDevice"] != NULL) {
						envDevice = layer["EnvDevice"].as<bool>();
					} else {
						envDevice = false;
					}
					if(layer["DisasterDevice"] != NULL) {
						disasterDevice = layer["DisasterDevice"].as<bool>();
					} else {
						disasterDevice = false;
					}

					topic->setLifeDevice(lifeDevice);
					topic->setEnvDevice(envDevice);
					topic->setDisasterDevice(disasterDevice);

					NDN_LOG_DEBUG_SS("EnvDevice: " << envDevice);
					NDN_LOG_DEBUG_SS("DisasterDevice: " << disasterDevice);

				} else {
					YAML::Node::iterator iter;
					for(iter = layer.begin(); iter != layer.end(); ++ iter) {
						std::string name = iter->first.as<std::string>();

						if(name.compare("Layer") != 0) {
							continue;
						}

						std::vector<TopicPtr> &children = topic->getTopics();
						if(iter->second.IsMap()) {
							if(load_layer(doc, iter->second, doc_device, children, topic, addtokens) == -1) {
								return -1;
							}
						} else if(iter->second.IsSequence()) {
							for(int li = 0; li < iter->second.size(); li ++) {
								if(load_layer(doc, iter->second[li], doc_device, children, topic, addtokens) == -1) {
									return -1;
								}
							}
						}
					}
				}
				Topics.push_back(topic);
			}
		}
	} else { //가변이 아닐때
		TopicPtr topic = std::make_shared<Topic>(prefix.c_str());
//		NDN_LOG_DEBUG_SS(stringf("Prefix: %s", prefix.c_str()));
		std::vector<std::string> addtokens = tokens;
		addtokens.push_back(prefix);

		bool lifeDevice;
		bool envDevice;
		bool disasterDevice;
		if(layer["Sequence"] != NULL) {
			long sequence = layer["Sequence"].as<long>();
			if(0 < sequence) {
				topic->setNextSequence(sequence);
			} else {
				topic->setNextSequence(1);
			}
			NDN_LOG_DEBUG_SS("Sequence: " << sequence);
		}

		if(layer["Layer"] == NULL) {
			if(layer["LifeDevice"] != NULL) {
				lifeDevice = layer["LifeDevice"].as<bool>();
				NDN_LOG_DEBUG_SS("LifeDevice: " << lifeDevice);
				if( lifeDevice == false ){
					NDN_LOG_DEBUG_SS("LifeDevice Required item");
					return -1;
				}
			} else {
				NDN_LOG_DEBUG_SS("LifeDevice Required item");
				return -1;
			}
			if(layer["EnvDevice"] != NULL) {
				envDevice = layer["EnvDevice"].as<bool>();
			} else {
				envDevice = false;
			}
			if(layer["DisasterDevice"] != NULL) {
				disasterDevice = layer["DisasterDevice"].as<bool>();
			} else {
				disasterDevice = false;
			}

			topic->setLifeDevice(lifeDevice);
			topic->setEnvDevice(envDevice);
			topic->setDisasterDevice(disasterDevice);

			NDN_LOG_DEBUG_SS("EnvDevice: " << envDevice);
			NDN_LOG_DEBUG_SS("DisasterDevice: " << disasterDevice);

			if(preTopic != nullptr){
				if(preTopic->getModelName().size() > 0){
					topic->setModelName(preTopic->getModelName());
				}
				if(preTopic->getSerial().size() > 0){
					topic->setSerial(preTopic->getSerial());
				}
				if(preTopic->getLat() != 0){
					topic->setLat(preTopic->getLat());
				}
				if(preTopic->getLon() != 0){
					topic->setLon(preTopic->getLon());
				}
			}
		} else {
			YAML::Node::iterator iter;
			for(iter = layer.begin(); iter != layer.end(); ++ iter) {
				std::string name = iter->first.as<std::string>();

				if(name.compare("Layer") != 0) {
					continue;
				}

				std::vector<TopicPtr> &children = topic->getTopics();
				if(iter->second.IsMap()) {
					if(load_layer(doc, iter->second, doc_device, children, topic, addtokens) == -1) {
						return -1;
					}
				} else if(iter->second.IsSequence()) {
					for(int li = 0; li < iter->second.size(); li ++) {
						if(load_layer(doc, iter->second[li], doc_device, children, topic, addtokens)== -1) {
							return -1;
						}
					}
				}
			}
		}
		Topics.push_back(topic);
	}

	return 0;
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

			NDN_LOG_DEBUG_SS("Topic Name: " << name);
		} else {
			std::string name(stringf("%s/%s", parent.c_str(), layer1name.c_str()));
//			NDN_LOG_DEBUG_SS("Topic Name: " << name);
			makeTopicList(layer1Topic->getTopics(), name, topicmap);
		}
	}
}

#if 0
void makeTopicList(std::vector<TopicPtr> &topics, boost::unordered_map<std::string, TopicPtr> &topicmap) {

	std::vector<TopicPtr>::iterator layer1iter;
	for(layer1iter = topics.begin(); layer1iter != topics.end(); layer1iter++) {
		TopicPtr layer1Topic = *layer1iter;
		std::string layer1name = layer1Topic->getName();

		if(layer1Topic->topicsSize() == 0) {
			if(layer1Topic->getType() != DataType::None) {
				std::string name(stringf("/%s", layer1name.c_str()));
				layer1Topic->setFullName(name);
				topicmap[name] = layer1Topic;
			}
		} else {
			std::vector<TopicPtr>::iterator layer2iter;
			for(layer2iter = layer1Topic->beginTopic(); layer2iter != layer1Topic->endTopic(); layer2iter++) {
				TopicPtr layer2Topic = *layer2iter;
				std::string layer2name = layer2Topic->getName();

				if(layer2Topic->topicsSize() == 0) {
					if(layer2Topic->getType() != DataType::None) {
						std::string name(stringf("/%s/%s", layer1name.c_str(), layer2name.c_str()));
						layer2Topic->setFullName(name);
						topicmap[name] = layer2Topic;
					}
				} else {
					std::vector<TopicPtr>::iterator layer3iter;
					for(layer3iter = layer2Topic->beginTopic(); layer3iter != layer2Topic->endTopic(); layer3iter++) {
						TopicPtr layer3Topic = *layer3iter;
						std::string layer3name = layer3Topic->getName();

						if(layer3Topic->getType() != DataType::None) {
							std::string name(stringf("/%s/%s/%s", layer1name.c_str(), layer2name.c_str(), layer3name.c_str()));
							layer3Topic->setFullName(name);
							topicmap[name] = layer3Topic;
						}
					}
				}
			}
		}
	}
}
#endif

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

enum DataType getDataType(std::string typeName) {
	if(caseInsCompare(typeName, "Int") || caseInsCompare(typeName, "Integer") || caseInsCompare(typeName, "Long")) {
		return DataType::Int;
	} else if(caseInsCompare(typeName, "Real") || caseInsCompare(typeName, "Float") || caseInsCompare(typeName, "Double")) {
		return DataType::Real;
	} else if(caseInsCompare(typeName, "String") || caseInsCompare(typeName, "Str")) {
		return DataType::String;
	}

	return DataType::None;
}

void makeSensorDataToJson(rapidjson::Document &document, SensorPtr sensor) {
	std::string strName = sensor->getName();

	enum DataType datatype = sensor->getType();
	if(datatype == DataType::Int) {
		long data = 0;
		if(sensor->generateData(data)) {
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(data);
			document.AddMember(name, v, document.GetAllocator());
		}
	} else if(datatype == DataType::Real) {
		double data = 0.0;
		if (sensor->generateData(data)) {
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(data);
			document.AddMember(name, v, document.GetAllocator());
		}
	} else if(datatype == DataType::String) {
		std::string data;
		if (sensor->generateData(data)) {
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(data.c_str(), document.GetAllocator());
			document.AddMember(name, v, document.GetAllocator());
		}
	}
}

rapidjson::Document makeSendPDData(long sequence, TopicPtr topic) {

	rapidjson::Document document;
	document.SetObject();

	if(topic->getLifeDevice() == true){
		std::vector<SensorPtr>::iterator iter;
		for(iter = topic->m_lifeDeviceList.begin();
			iter != topic->m_lifeDeviceList.end();
			iter ++) {
			makeSensorDataToJson(document, *iter);
		}
	}

	if(topic->getEnvDevice() == true){
		std::vector<SensorPtr>::iterator iter;
		for(iter = topic->m_envDeviceList.begin();
			iter != topic->m_envDeviceList.end();
			iter ++) {
			makeSensorDataToJson(document, *iter);
		}
	}

	if(topic->getDisasterDevice() == true){
		std::vector<SensorPtr>::iterator iter;
		for(iter = topic->m_disasterDeviceList.begin();
			iter != topic->m_disasterDeviceList.end();
			iter ++) {
			makeSensorDataToJson(document, *iter);
		}
	}

	std::string modelName = topic->getModelName();
	rapidjson::Value j_modelName(modelName.c_str(), document.GetAllocator());
	document.AddMember("modelName", j_modelName, document.GetAllocator());

	std::string serial = topic->getSerial();
	rapidjson::Value j_serial(serial.c_str(), document.GetAllocator());
	document.AddMember("serial", j_serial, document.GetAllocator());

	double lat = topic->getLat();
	rapidjson::Value j_lat(lat);
	document.AddMember("lat", j_lat, document.GetAllocator());

	double lon = topic->getLon();
	rapidjson::Value j_lon(lon);
	document.AddMember("lon", j_lon, document.GetAllocator());

//	clock_t realTime = clock();
//	rapidjson::Value j_time(realTime);

	struct tm curr_tm;
	time_t curr_time = std::time(nullptr);
	localtime_r(&curr_time, &curr_tm);

	int curr_year = curr_tm.tm_year + 1900;
	int curr_month = curr_tm.tm_mon + 1;
	int curr_day = curr_tm.tm_mday;
	int curr_hour = curr_tm.tm_hour;
	int curr_minute = curr_tm.tm_min;
	int curr_second = curr_tm.tm_sec;

	std::string realtime = stringf("%d-%d-%d %d:%d:%d", curr_year, curr_month, curr_day, curr_hour, curr_minute, curr_second);
	rapidjson::Value j_time(realtime.c_str(), document.GetAllocator());
	document.AddMember("time", j_time, document.GetAllocator());

	return document;
}

void sendPU(TopicPtr topic) {
	long sequence = topic->getNextSequence();
	ndn::Name pu_name(cfg_serviceName);
	pu_name.append("PU");
	pu_name.append(topic->getFullName());
	//name.appendVersion();

	InterestPtr pu_interest = std::make_shared<ndn::Interest>(pu_name);
	pu_interest->setCanBePrefix(gPUInterestOption->m_canBePrefix);
	pu_interest->setMustBeFresh(gPUInterestOption->m_mustBeFresh);
	pu_interest->setInterestLifetime(time::milliseconds(gPUInterestOption->m_lifetime));
	if(gPUInterestOption->m_hopLimit.has_value()) {
		pu_interest->setHopLimit(gPUInterestOption->m_hopLimit);
	}

	std::string appParam;

	rapidjson::Document document = 	makeSendPUData(topic);

#if 0
	DocumentPtr params = std::make_shared<rapidjson::Document>(rapidjson::kObjectType);
	params->SetObject();

	rapidjson::Value nullnode(rapidjson::kObjectType);


//		params->AddMember("storagetype", 0, params->GetAllocator());
	params->AddMember("pubadvinfo", document, params->GetAllocator());
	params->AddMember("irinfo", nullnode, params->GetAllocator());
	std::stringstream ssRapidJsonTest;
	if(params != nullptr) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		params->Accept(writer);

		ssRapidJsonTest << buffer.GetString();
	}
	std::string json = ssRapidJsonTest.str();
	NDN_LOG_DEBUG_SS("PU JSON Data: " << json);
#endif

	std::stringstream ssRapidJsonTest;
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	ssRapidJsonTest << buffer.GetString();
	std::string json = ssRapidJsonTest.str();
	NDN_LOG_DEBUG_SS("PU JSON Data: " << json);

	Block parameters = makeStringBlock(tlv::ApplicationParameters, json);
	pu_interest->setApplicationParameters(parameters);

	NDN_LOG_DEBUG_SS("PU Interest: " << *pu_interest);
	NDN_LOG_DEBUG_SS("PU AppParam: " << json);


	InterestDelegatorPtr pu_delegator = nullptr;
	pu_delegator = std::make_shared<InterestDelegator>(gFace, gScheduler, pu_interest, false);
	pu_delegator->dataHandler=[topic](const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("PU Data Name: " << data.getName());

		gStatPU.increase_data();

		gStatPU.checkEndTime();

		const Name &name = data.getName();
		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("PU Data Content: " << value);


		// 데이터를 받았으니 저장해 놓은 객체는 버린다
		std::string uri;
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator timeiter;
		timeiter = gPUTimeMap.find(uri);
		if(timeiter != gPUTimeMap.end()) {
			struct timeval diff = timeval_diff(&timeiter->second, &tv);
			gStatPU.increaseWaitTime(diff);
			if(cfg_PUPrintDetailTimeData) {
				gStatPU.saveWaitTime(uri, timeiter->second, tv, diff);
			}
		}

		// round 수 만큼 일정 간격으로 반복해서 보낸다.

		gStatPU.decrease_topic();

		boost::unordered_map<std::string, InterestDelegatorPtr>::iterator iter;
		iter = gPUMap.find(uri);
		if(iter != gPUMap.end()) {
			gPUMap.erase(iter);
		}
	};
	pu_delegator->timeoutHandler=[=](const Interest &interest) {

		NDN_LOG_DEBUG_SS("Timeout: " << interest);

		gStatPU.increase_timeout();

		gStatPU.checkEndTime();

		// Timeout 되면 다시 보내므로, 저장해 놓은 객체를 버리면 안된다.

		// pd_delegator가 다시 보냈다.
		gStatPU.increase_interest();
	};
	pu_delegator->nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();

		NDN_LOG_DEBUG_SS("Nack: " << name << ", Reason: " << nack.getReason());

		gStatPU.increase_nack();

		gStatPU.checkEndTime();

		// NACK는 Interest를 보낼 수 없는 경우, 저장해 놓은 객체는 버린다
	};

	pu_delegator->send();

	gPUMap[pu_name.toUri()] = pu_delegator;

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gPUTimeMap[pu_name.toUri()] = tv;

	gStatPU.increase_interest();
}

void sendPD(TopicPtr topic, long round) {
	long sequence = topic->getNextSequence();
	ndn::Name pd_name(cfg_serviceName);
	pd_name.append("PD");
	pd_name.append(topic->getFullName());
	pd_name.append(std::to_string(sequence));
	//name.appendVersion();

	InterestPtr pd_interest = std::make_shared<ndn::Interest>(pd_name);
	pd_interest->setCanBePrefix(gPDInterestOption->m_canBePrefix);
	pd_interest->setMustBeFresh(gPDInterestOption->m_mustBeFresh);
	pd_interest->setInterestLifetime(time::milliseconds(gPDInterestOption->m_lifetime));
	if(gPDInterestOption->m_hopLimit.has_value()) {
		pd_interest->setHopLimit(gPDInterestOption->m_hopLimit);
	}

	std::string appParam;


	rapidjson::Document document = 	makeSendPDData(sequence, topic);

	DocumentPtr data = std::make_shared<rapidjson::Document>(rapidjson::kObjectType);
	data->SetObject();

	DocumentPtr params = std::make_shared<rapidjson::Document>(rapidjson::kObjectType);
	params->SetObject();

	data->AddMember("data", document, data->GetAllocator());
	data->AddMember("data_sseq", sequence, data->GetAllocator());
	data->AddMember("data_eseq", sequence, data->GetAllocator());

	rapidjson::Value nullnode(rapidjson::kObjectType);

	params->AddMember("pubdatainfo", *data, params->GetAllocator());
//	params->AddMember("irinfo", nullnode, params->GetAllocator());
	std::stringstream ssRapidJsonTest;
	if(params != nullptr) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		params->Accept(writer);
//		document.Accept(writer);

		ssRapidJsonTest << buffer.GetString();
	}
	std::string json = ssRapidJsonTest.str();

	Block parameters = makeStringBlock(tlv::ApplicationParameters, json);
	pd_interest->setApplicationParameters(parameters);

	NDN_LOG_DEBUG_SS("PD Interest: " << *pd_interest);
	NDN_LOG_DEBUG_SS("PD AppParam: " << json);

	round += 1;

	InterestDelegatorPtr pd_delegator = nullptr;
	pd_delegator = std::make_shared<InterestDelegator>(gFace, gScheduler, pd_interest, false);
	pd_delegator->dataHandler=[topic, round](const Data &data) {
		struct timeval tv;
		gettimeofday(&tv, NULL);

		NDN_LOG_DEBUG_SS("PD Data Name: " << data.getName());

		gStatPD.increase_data();

		gStatPD.checkEndTime();

		const Name &name = data.getName();
		const Block &content = data.getContent();
		std::string value((const char *)content.value(), (size_t)content.value_size());
		NDN_LOG_DEBUG_SS("PD Data Content: " << value);


		// 데이터를 받았으니 저장해 놓은 객체는 버린다
		std::string uri;
		ssize_t pos = findParametersDigestComponent(name);
		if(0 < pos) {
			uri = name.getPrefix(pos).toUri();
		} else {
			uri = name.toUri();
		}

		boost::unordered_map<std::string, struct timeval>::iterator timeiter;
		timeiter = gPDTimeMap.find(uri);
		if(timeiter != gPDTimeMap.end()) {
			struct timeval diff = timeval_diff(&timeiter->second, &tv);
			gStatPD.increaseWaitTime(diff);
			if(cfg_PDPrintDetailTimeData) {
				gStatPD.saveWaitTime(uri, timeiter->second, tv, diff);
			}
		}

		// round 수 만큼 일정 간격으로 반복해서 보낸다.
		if(round < cfg_round) {
			time::microseconds interval(cfg_PDinterval);
			topic->m_schedulePD = gScheduler->schedule(interval, [topic, round] {
				sendPD(topic, round);
			});
		} else {
			gStatPD.decrease_topic();
			time::microseconds interval(0);
			gScheduler->schedule(interval, [topic] {
				if(is_first_pd) {
					gStatPU.checkStartTime();
					is_first_pd = false;
				}
				sendPU(topic);
			});
			gStatPU.increase_topic();
		}

		boost::unordered_map<std::string, InterestDelegatorPtr>::iterator iter;
		iter = gPDMap.find(uri);
		if(iter != gPDMap.end()) {
			gPDMap.erase(iter);
		}
	};
	pd_delegator->timeoutHandler=[=](const Interest &interest) {

		NDN_LOG_DEBUG_SS("Timeout: " << interest);

		gStatPD.increase_timeout();

		gStatPD.checkEndTime();

		// Timeout 되면 다시 보내므로, 저장해 놓은 객체를 버리면 안된다.

		// pd_delegator가 다시 보냈다.
		gStatPD.increase_interest();
	};
	pd_delegator->nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
		const Name &name = nack.getInterest().getName();

		NDN_LOG_DEBUG_SS("Nack: " << name << ", Reason: " << nack.getReason());

		gStatPD.increase_nack();

		gStatPD.checkEndTime();

		// NACK는 Interest를 보낼 수 없는 경우, 저장해 놓은 객체는 버린다
	};

	pd_delegator->send();

	gPDMap[pd_name.toUri()] = pd_delegator;

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gPDTimeMap[pd_name.toUri()] = tv;

	gStatPD.increase_interest();
}

void sendPA(std::vector<InterestDelegatorPtr> &delegators, int index) {

	if(delegators.size() <= index) {
		return;
	}

	NDN_LOG_DEBUG(stringf("Send[%d]: ", index) << *(delegators[index]->m_interest));

	delegators[index]->send();

	gStatPA.increase_interest();

	const Name &name = delegators[index]->m_interest->getName();
	std::string uri;
	ssize_t pos = findParametersDigestComponent(name);
	if(0 < pos) {
		uri = name.getPrefix(pos).toUri();
	} else {
		uri = name.toUri();
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gPATimeMap[uri] = tv;

	time::microseconds interval(cfg_PAinterval);
	gScheduler->schedule(interval, [&delegators, index] {
		sendPA(delegators, index + 1);
	});
}


void sendPU(std::vector<InterestDelegatorPtr> &delegators, int index){

	if(delegators.size() <= index) {
		return;
	}

	NDN_LOG_DEBUG(stringf("Send[%d]: ", index) << *(delegators[index]->m_interest));

	delegators[index]->send();

	gStatPU.increase_interest();

	const Name &name = delegators[index]->m_interest->getName();
	std::string uri;
	ssize_t pos = findParametersDigestComponent(name);
	if(0 < pos) {
		uri = name.getPrefix(pos).toUri();
	} else {
		uri = name.toUri();
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	gPUTimeMap[uri] = tv;

	time::microseconds interval(cfg_PUinterval);
	gScheduler->schedule(interval, [&delegators, index] {
		sendPU(delegators, index + 1);
	});
}

std::string JsonDocToString(rapidjson::Document &doc, bool isPretty = false)
{
	rapidjson::StringBuffer buffer;

	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

    return buffer.GetString();
}


rapidjson::Document makeSendPAData(TopicPtr topic) {

	rapidjson::Document document;
	document.SetObject();
	rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

	YAML::Node y_lifeDevice= doc["LifeDevice"];
	YAML::Node y_envDevice= doc["EnvDevice"];
	YAML::Node y_disasterDevice= doc["DisasterDevice"];

	document.AddMember("storagetype", 0, document.GetAllocator());
	document.AddMember("bundle", false, document.GetAllocator());

	if(topic->getLifeDevice() == true){
		for(int li = 0; li < y_lifeDevice.size(); li ++) {
			YAML::Node::iterator iter = y_lifeDevice[li].begin();
			if( iter ==  y_lifeDevice[li].end()  ) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(0);
			document.AddMember(name, v, document.GetAllocator());
		}
	}

	if(topic->getEnvDevice() == true){
		for(int li = 0; li < y_envDevice.size(); li ++) {
			YAML::Node::iterator iter = y_envDevice[li].begin();
			if( iter ==  y_envDevice[li].end()  ) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(0);
			document.AddMember(name, v, document.GetAllocator());
		}
	}

	if(topic->getDisasterDevice() == true){
		for(int li = 0; li < y_disasterDevice.size(); li ++) {
			YAML::Node::iterator iter = y_disasterDevice[li].begin();
			if( iter ==  y_disasterDevice[li].end()  ) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(0);
			document.AddMember(name, v, document.GetAllocator());
		}
	}

	std::string modelName = topic->getModelName();
	rapidjson::Value j_modelName(modelName.c_str(), document.GetAllocator());
	document.AddMember("modelName", j_modelName, document.GetAllocator());

	std::string serial = topic->getSerial();
	rapidjson::Value j_serial(serial.c_str(), document.GetAllocator());
	document.AddMember("serial", j_serial, document.GetAllocator());

	double lat = topic->getLat();
	rapidjson::Value j_lat(lat);
	document.AddMember("lat", j_lat, document.GetAllocator());

	double lon = topic->getLon();
	rapidjson::Value j_lon(lon);
	document.AddMember("lon", j_lon, document.GetAllocator());

	return document;
}

rapidjson::Document makeSendPUData(TopicPtr topic) {

	rapidjson::Document document;
	document.SetObject();
	rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

	YAML::Node y_lifeDevice= doc["LifeDevice"];
	YAML::Node y_envDevice= doc["EnvDevice"];
	YAML::Node y_disasterDevice= doc["DisasterDevice"];

	document.AddMember("storagetype", 0, document.GetAllocator());
	document.AddMember("bundle", false, document.GetAllocator());

	if(topic->getLifeDevice() == true){
		for(int li = 0; li < y_lifeDevice.size(); li ++) {
			YAML::Node::iterator iter = y_lifeDevice[li].begin();
			if( iter ==  y_lifeDevice[li].end()  ) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(0);
			document.AddMember(name, v, document.GetAllocator());
		}
	}

	if(topic->getEnvDevice() == true){
		for(int li = 0; li < y_envDevice.size(); li ++) {
			YAML::Node::iterator iter = y_envDevice[li].begin();
			if( iter ==  y_envDevice[li].end()  ) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(0);
			document.AddMember(name, v, document.GetAllocator());
		}
	}

	if(topic->getDisasterDevice() == true){
		for(int li = 0; li < y_disasterDevice.size(); li ++) {
			YAML::Node::iterator iter = y_disasterDevice[li].begin();
			if( iter ==  y_disasterDevice[li].end()  ) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			rapidjson::Value name(strName.c_str(), document.GetAllocator());
			rapidjson::Value v(0);
			document.AddMember(name, v, document.GetAllocator());
		}
	}

	std::string modelName = topic->getModelName();
	rapidjson::Value j_modelName(modelName.c_str(), document.GetAllocator());
	document.AddMember("modelName", j_modelName, document.GetAllocator());

	std::string serial = topic->getSerial();
	rapidjson::Value j_serial(serial.c_str(), document.GetAllocator());
	document.AddMember("serial", j_serial, document.GetAllocator());

	double lat = topic->getLat();
	rapidjson::Value j_lat(lat);
	document.AddMember("lat", j_lat, document.GetAllocator());

	double lon = topic->getLon();
	rapidjson::Value j_lon(lon);
	document.AddMember("lon", j_lon, document.GetAllocator());

	return document;
}

void configureSensorDevice(TopicMap &topicmap, YAML::Node doc_sensor) {



	YAML::Node y_lifeDevice= doc_sensor["LifeDevice"];
	YAML::Node y_envDevice= doc_sensor["EnvDevice"];
	YAML::Node y_disasterDevice= doc_sensor["DisasterDevice"];

	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		std::string topicName = iter->first;
		TopicPtr topic = iter->second; // TopicPtr

		for(int li = 0; li < y_lifeDevice.size(); li ++) {
			YAML::Node::iterator iter = y_lifeDevice[li].begin();
			if( iter ==  y_lifeDevice[li].end()  ) {
				continue;
			}

			enum DataType datatype = DataType::None;
			YAML::Node detail = iter->second;
			if(detail["Type"] == NULL) {
				continue;
			}
			std::string pubType = detail["Type"].as<std::string>();
			datatype = getDataType(pubType);
			if(datatype == DataType::None) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			SensorPtr sensor = std::make_shared<SensorDevice>(strName, datatype);
			if ( setDataRange(detail["DataRange"], sensor) == 0) {
				topic->appendLifeDevice(sensor);
			}
		}
		for(int li = 0; li < y_envDevice.size(); li ++) {
			YAML::Node::iterator iter = y_envDevice[li].begin();
			if( iter ==  y_envDevice[li].end()  ) {
				continue;
			}

			enum DataType datatype = DataType::None;
			YAML::Node detail = iter->second;
			if(detail["Type"] == NULL) {
				continue;
			}
			std::string pubType = detail["Type"].as<std::string>();
			datatype = getDataType(pubType);
			if(datatype == DataType::None) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			SensorPtr sensor = std::make_shared<SensorDevice>(strName, datatype);
			if ( setDataRange(detail["DataRange"], sensor) == 0) {
				topic->appendEnvDevice(sensor);
			}
		}
		for(int li = 0; li < y_disasterDevice.size(); li ++) {
			YAML::Node::iterator iter = y_disasterDevice[li].begin();
			if( iter ==  y_disasterDevice[li].end()  ) {
				continue;
			}

			enum DataType datatype = DataType::None;
			YAML::Node detail = iter->second;
			if(detail["Type"] == NULL) {
				continue;
			}
			std::string pubType = detail["Type"].as<std::string>();
			datatype = getDataType(pubType);
			if(datatype == DataType::None) {
				continue;
			}

			std::string strName = iter->first.as<std::string>();
			SensorPtr sensor = std::make_shared<SensorDevice>(strName, datatype);
			if ( setDataRange(detail["DataRange"], sensor) == 0) {
				topic->appendDisasterDevice(sensor);
			}
		}
	}
}

void scheduleSendPA(TopicMap &topicmap, std::vector<InterestDelegatorPtr> &delegators) {

	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		std::string topicName = iter->first;
		TopicPtr topic = iter->second; // TopicPtr

		Name pa_name(cfg_serviceName);
		pa_name.append("PA");
		pa_name.append(topicName);
		//std::cout << "Interest: " << pa_name << std::endl;

		InterestPtr pa_interest = nullptr;
		pa_interest = std::make_shared<ndn::Interest>(pa_name);
		pa_interest->setCanBePrefix(gPAInterestOption->m_canBePrefix);
		pa_interest->setMustBeFresh(gPAInterestOption->m_mustBeFresh);
		pa_interest->setInterestLifetime(time::milliseconds(gPAInterestOption->m_lifetime));
		if(gPAInterestOption->m_hopLimit.has_value()) {
			pa_interest->setHopLimit(gPAInterestOption->m_hopLimit);
		}

		rapidjson::Document document = makeSendPAData(topic);

/*
		std::string modelName = topic->getModelName();
		rapidjson::Value j_modelName(modelName.c_str(), document.GetAllocator());
		document.AddMember("modelName", j_modelName, document.GetAllocator());

		std::string serial = topic->getSerial();
		rapidjson::Value j_serial(serial.c_str(), document.GetAllocator());
		document.AddMember("serial", j_serial, document.GetAllocator());

		double lat = topic->getLat();
		rapidjson::Value j_lat(lat);
		document.AddMember("lat", j_lat, document.GetAllocator());

		double lon = topic->getLon();
		rapidjson::Value j_lon(lon);
		document.AddMember("lon", j_lon, document.GetAllocator());
*/


		DocumentPtr params = std::make_shared<rapidjson::Document>(rapidjson::kObjectType);
		params->SetObject();


		rapidjson::Value nullnode(rapidjson::kObjectType);


//		params->AddMember("storagetype", 0, params->GetAllocator());
		params->AddMember("pubadvinfo", document, params->GetAllocator());
		params->AddMember("irinfo", nullnode, params->GetAllocator());
		std::stringstream ssRapidJsonTest;
		if(params != nullptr) {
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

			params->Accept(writer);

			ssRapidJsonTest << buffer.GetString();
		}
		std::string json = ssRapidJsonTest.str();
		NDN_LOG_DEBUG_SS("PA JSON Data: " << json);

//		std::string appParam("{\"pubinfo\": null, \"irinfo\": null}");
		Block parameters = makeStringBlock(tlv::ApplicationParameters, json);
		pa_interest->setApplicationParameters(parameters);

		InterestDelegatorPtr pa_delegator = nullptr;
		pa_delegator = std::make_shared<InterestDelegator>(gFace, gScheduler, pa_interest, false);
		delegators.push_back(pa_delegator);

		pa_delegator->dataHandler=[topic](const Data &data) {
			struct timeval tv;
			gettimeofday(&tv, NULL);

			NDN_LOG_DEBUG_SS("PA Data Name: " << data);

			gStatPA.increase_data();

			gStatPA.checkEndTime();


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
				if(cfg_PAPrintDetailTimeData) {
					gStatPA.saveWaitTime(uri, iter->second, tv, diff);
				}
			}

			// Data를 받았으면, PD 를 보내야지
			time::microseconds interval(0);
			gScheduler->schedule(interval, [topic] {
				if(is_first_pa) {
					gStatPD.checkStartTime();
					is_first_pa = false;
				}
				sendPD(topic, 0L);
			});
			gStatPD.increase_topic();
		};
		pa_delegator->timeoutHandler=[=](const Interest &interest) {
			NDN_LOG_DEBUG_SS("Timeout: " << interest);

			gStatPA.increase_timeout();

			gStatPA.checkEndTime();

			// pa_delegator가 다시 보냈다.
			gStatPA.increase_interest();
		};
		pa_delegator->nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
			const Name &name = nack.getInterest().getName();

			NDN_LOG_DEBUG_SS("Nack: " << name << ", Reason: " << nack.getReason());

			gStatPA.increase_nack();

			gStatPA.checkEndTime();
		};
	}

	// 0 부터 delegators.size() 만큼 비동기로 전송한다.
	time::microseconds interval(0);
	gScheduler->schedule(interval, [&delegators] {

		gStatPA.checkStartTime();
		gStatALL.checkStartTime();

		if(0 < delegators.size()) {
			sendPA(delegators, 0);
		} else {
			gStatPA.checkEndTime();
		}
	});
}


void scheduleSendPU(TopicMap topicmap, std::vector<InterestDelegatorPtr> &delegators) {


	TopicMap::iterator iter;
	for(iter = topicmap.begin(); iter != topicmap.end(); iter ++) {
		std::string topicName = iter->first;
		TopicPtr topic = iter->second; // TopicPtr

		Name pu_name(cfg_serviceName);
		pu_name.append("PU");
		pu_name.append(topicName);
		//std::cout << "Interest: " << pa_name << std::endl;

		InterestPtr pu_interest = nullptr;
		pu_interest = std::make_shared<ndn::Interest>(pu_name);
		pu_interest->setCanBePrefix(gPUInterestOption->m_canBePrefix);
		pu_interest->setMustBeFresh(gPUInterestOption->m_mustBeFresh);
		pu_interest->setInterestLifetime(time::milliseconds(gPUInterestOption->m_lifetime));
		if(gPUInterestOption->m_hopLimit.has_value()) {
			pu_interest->setHopLimit(gPUInterestOption->m_hopLimit);
		}

		rapidjson::Document document = makeSendPUData(topic);

		DocumentPtr params = std::make_shared<rapidjson::Document>(rapidjson::kObjectType);
		params->SetObject();


		rapidjson::Value nullnode(rapidjson::kObjectType);


//		params->AddMember("storagetype", 0, params->GetAllocator());
		params->AddMember("pubadvinfo", document, params->GetAllocator());
		params->AddMember("irinfo", nullnode, params->GetAllocator());
		std::stringstream ssRapidJsonTest;
		if(params != nullptr) {
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

			params->Accept(writer);

			ssRapidJsonTest << buffer.GetString();
		}
		std::string json = ssRapidJsonTest.str();
		NDN_LOG_DEBUG_SS("PU JSON Data: " << json);

//		std::string appParam("{\"pubinfo\": null, \"irinfo\": null}");
		Block parameters = makeStringBlock(tlv::ApplicationParameters, json);
		pu_interest->setApplicationParameters(parameters);

		InterestDelegatorPtr pu_delegator = nullptr;
		pu_delegator = std::make_shared<InterestDelegator>(gFace, gScheduler, pu_interest, false);
		delegators.push_back(pu_delegator);

		pu_delegator->dataHandler=[topic](const Data &data) {
			struct timeval tv;
			gettimeofday(&tv, NULL);

			NDN_LOG_DEBUG_SS("PU Data Name: " << data.getName());

			gStatPU.increase_data();

			gStatPU.checkEndTime();

			const Name &name = data.getName();
			const Block &content = data.getContent();
			std::string value((const char *)content.value(), (size_t)content.value_size());
			NDN_LOG_DEBUG_SS("PU Data Content: " << value);


			// 데이터를 받았으니 저장해 놓은 객체는 버린다
			std::string uri;
			ssize_t pos = findParametersDigestComponent(name);
			if(0 < pos) {
				uri = name.getPrefix(pos).toUri();
			} else {
				uri = name.toUri();
			}

			boost::unordered_map<std::string, struct timeval>::iterator timeiter;
			timeiter = gPUTimeMap.find(uri);
			if(timeiter != gPUTimeMap.end()) {
				struct timeval diff = timeval_diff(&timeiter->second, &tv);
				gStatPU.increaseWaitTime(diff);
				if(cfg_PUPrintDetailTimeData) {
					gStatPU.saveWaitTime(uri, timeiter->second, tv, diff);
				}
			}

			// round 수 만큼 일정 간격으로 반복해서 보낸다.

			gStatPU.decrease_topic();

			boost::unordered_map<std::string, InterestDelegatorPtr>::iterator iter;
			iter = gPUMap.find(uri);
			if(iter != gPUMap.end()) {
				gPUMap.erase(iter);
			}
		};

		pu_delegator->timeoutHandler=[=](const Interest &interest) {
			NDN_LOG_DEBUG_SS("Timeout: " << interest);

			gStatPU.increase_timeout();

			gStatPU.checkEndTime();

			// pa_delegator가 다시 보냈다.
			gStatPU.increase_interest();
		};

		pu_delegator->nackHandler=[=](const Interest &interest, const lp::Nack &nack) {
			const Name &name = nack.getInterest().getName();

			NDN_LOG_DEBUG_SS("Nack: " << name << ", Reason: " << nack.getReason());

			gStatPU.increase_nack();

			gStatPU.checkEndTime();
		};

		gStatPU.increase_topic();
	}

	// 0 부터 delegators.size() 만큼 비동기로 전송한다.
	time::microseconds interval(0);
	gScheduler->schedule(interval, [&delegators] {
		gStatPU.checkStartTime();
		if(0 < delegators.size()) {
			sendPU(delegators, 0);
		} else {
			gStatPU.checkEndTime();
		}
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

	gIOService->stop();
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
		usage(argc, argv);
		return -1;
	}

	NDN_LOG_INFO(opt_confpath);

	pos = realpath(opt_confpath.c_str(), NULL);
	if (pos != NULL) {
		confpath_dir = dirname(pos);
		free(pos);
	}

	std::ifstream input(opt_confpath);

	try {
		doc = YAML::Load(input);

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
			NDN_LOG_INFO(stringf("default Duration: %lu", cfg_duration));
			NDN_LOG_INFO(stringf("default round: %lu", cfg_round));
			NDN_LOG_INFO(stringf("default PAinterval: %lu", cfg_PAinterval));
			NDN_LOG_INFO(stringf("default PUinterval: %lu", cfg_PUinterval));
			NDN_LOG_INFO(stringf("default PDinterval: %lu", cfg_PDinterval));
			NDN_LOG_INFO(stringf("default deviceFile: %s", cfg_deviceFile.c_str()));
			NDN_LOG_INFO(stringf("default SeosorFile: %s", cfg_sensorFile.c_str()));
			NDN_LOG_INFO(stringf("default serviceName: %s", cfg_serviceName.c_str()));
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



		std::vector<std::string> tokens;

		// Layer 1 topics
		YAML::Node::iterator iter;
		for(iter = publisher.begin(); iter != publisher.end(); ++ iter) {
			std::string name = iter->first.as<std::string>();

			if(name.compare("Layer") != 0) {
				continue;
			}
			if(iter->second.IsMap()) {
				if(load_layer(doc, iter->second, doc_device, gTopics, nullptr, tokens) == -1) {
					return -1;
				}
			} else if(iter->second.IsSequence()) {
				for(int li = 0; li < iter->second.size(); li ++) {
					if(load_layer(doc, iter->second[li], doc_device, gTopics, nullptr, tokens) == -1) {
						return -1;
					}
				}
			}
		}

		// topic을 순회하여 map(topic name, topic)을 생성한다.
		makeTopicList(gTopics, "", gTopicmap);


		if (cfg_sensorFile.rfind("/", 0) != 0) {
			std::string fullpath = confpath_dir + "/" + cfg_sensorFile;
			cfg_sensorFile = fullpath;
		}

		NDN_LOG_DEBUG_SS(stringf("Sensor Configure FIle: %s", cfg_sensorFile.c_str()));

		std::ifstream sensor(cfg_sensorFile);
		YAML::Node doc_sensor = YAML::Load(sensor);

		if(doc_sensor.IsNull()) {
			NDN_LOG_ERROR("Not Found Sensor File");
			return -1;
		}

		configureSensorDevice(gTopicmap, doc_sensor);

		std::vector<InterestDelegatorPtr> delegators;
		std::vector<InterestDelegatorPtr> delegators2;

		gIOService = std::make_shared<boost::asio::io_service>();
		gFace = std::make_shared<Face>(*gIOService);
		gScheduler = std::make_shared<Scheduler>(*gIOService);

		// topicmap을 순회하여 PA Integerst를 전송한다.
		scheduleSendPA(gTopicmap, delegators);

		time::seconds wait(1);
		gScheduler->schedule(wait, [=] {
			check_complete();
		});

		if(0 < cfg_duration) {
			time::seconds duration(cfg_duration);
			gScheduler->schedule(duration, [&delegators2] {
				TopicMap::iterator iter;
				for(iter = gTopicmap.begin(); iter != gTopicmap.end(); iter ++) {
					std::string topicName = iter->first;
					TopicPtr topic = iter->second; // TopicPtr
					topic->m_schedulePD.cancel();
					gStatPD.decrease_topic();
				}
				scheduleSendPU(gTopicmap, delegators2);
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

		gStatALL.checkEndTime();

		NDN_LOG_INFO(stringf("   Start PA: %s", gStatPA.getStartTimeString().c_str()));
		NDN_LOG_INFO(stringf("     End PA: %s", gStatPA.getEndTimeString().c_str()));
		struct timeval diff_pa = timeval_diff(&gStatPA.m_start, &gStatPA.m_end);
		NDN_LOG_INFO(stringf("    Time PA: %ld.%06ld", diff_pa.tv_sec, diff_pa.tv_usec));
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
		if(cfg_PAPrintDetailTimeData) {
			NDN_LOG_INFO(stringf(" Each PA List: %d", gStatPA.m_waitList.size()));
			std::vector<TimeData>::iterator dataiter = gStatPA.m_waitList.begin();
			for(; dataiter != gStatPA.m_waitList.end(); dataiter++) {
				TimeData &data = *dataiter;
				NDN_LOG_INFO(stringf(" %s: %d.%06ld - %d.%06ld = %d.%06ld", data.m_name.c_str(),
						data.m_tv1.tv_sec, data.m_tv1.tv_usec,
						data.m_tv2.tv_sec, data.m_tv2.tv_usec,
						data.m_tv3.tv_sec, data.m_tv3.tv_usec));
			}
		}
		NDN_LOG_INFO(stringf("   Start PD: %s", gStatPD.getStartTimeString().c_str()));
		NDN_LOG_INFO(stringf("     End PD: %s", gStatPD.getEndTimeString().c_str()));
		struct timeval diff_pd = timeval_diff(&gStatPD.m_start, &gStatPD.m_end);
		NDN_LOG_INFO(stringf("    Time PD: %ld.%06ld", diff_pd.tv_sec, diff_pd.tv_usec));
		if(0 < gStatPD.m_data) {
			//int64_t avg = (gStatPD.m_wait.tv_sec*1000000 + gStatPD.m_wait.tv_usec)/gStatPD.m_data;
			int64_t avg = (diff_pd.tv_sec*1000000 + diff_pd.tv_usec)/gStatPD.m_data;
			NDN_LOG_INFO(stringf("     AVG PD: %ld.%06ld", avg/1000000, avg%1000000));
			//NDN_LOG_INFO(stringf("     AVG PD: %ld.%06ld", avg/1000000, (4*avg%1000000)));
		} else {
			NDN_LOG_INFO(stringf("     AVG PD: %s", gStatPD.getWaitTimeString().c_str()));
		}
		NDN_LOG_INFO(stringf("Interest PD: %d", gStatPD.m_interest));
		NDN_LOG_INFO(stringf("    Data PD: %d", gStatPD.m_data));
		NDN_LOG_INFO(stringf("    NACK PD: %d", gStatPD.m_nack));
		NDN_LOG_INFO(stringf(" Timeout PD: %d", gStatPD.m_timeout));
		if(cfg_PDPrintDetailTimeData) {
			NDN_LOG_INFO(stringf(" Each PD List: %d", gStatPD.m_waitList.size()));
			std::vector<TimeData>::iterator dataiter = gStatPD.m_waitList.begin();
			for(; dataiter != gStatPD.m_waitList.end(); dataiter++) {
				TimeData &data = *dataiter;
				NDN_LOG_INFO(stringf(" %s: %d.%06ld - %d.%06ld = %d.%06ld", data.m_name.c_str(),
						data.m_tv1.tv_sec, data.m_tv1.tv_usec,
						data.m_tv2.tv_sec, data.m_tv2.tv_usec,
						data.m_tv3.tv_sec, data.m_tv3.tv_usec));
			}
		}
		NDN_LOG_INFO(stringf("   Start PU: %s", gStatPU.getStartTimeString().c_str()));
		NDN_LOG_INFO(stringf("     End PU: %s", gStatPU.getEndTimeString().c_str()));
		struct timeval diff_pu = timeval_diff(&gStatPU.m_start, &gStatPU.m_end);
		NDN_LOG_INFO(stringf("    Time PU: %ld.%06ld", diff_pu.tv_sec, diff_pu.tv_usec));
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
		if(cfg_PUPrintDetailTimeData) {
			NDN_LOG_INFO(stringf(" Each PU List: %d", gStatPU.m_waitList.size()));
			std::vector<TimeData>::iterator dataiter = gStatPU.m_waitList.begin();
			for(; dataiter != gStatPU.m_waitList.end(); dataiter++) {
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
		int64_t avg = (diff_pd.tv_sec*1000000 + diff_pd.tv_usec)/gStatPD.m_data;
                NDN_LOG_INFO(stringf("**************************************************************"));
                // NDN_LOG_INFO(stringf(" Average Event Distribution Delay : %ld ms", (4*avg%1000000)/1000));
                NDN_LOG_INFO(stringf(" Average Event Distribution Delay : %ld ms", (avg%1000000)/1000));
                NDN_LOG_INFO(stringf("**************************************************************")); 
	} catch (const YAML::Exception &e) {
		NDN_LOG_ERROR(e.what());
		return -1;
	}

	return 0;
}
