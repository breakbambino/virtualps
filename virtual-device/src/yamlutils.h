/*
 * yamlutils.h
 *
 *  Created on: 2021. 8. 6.
 *      Author: ymtech
 */

#ifndef SRC_YAMLUTILS_H_
#define SRC_YAMLUTILS_H_

#include <string>
#include <tuple>
#include <yaml-cpp/yaml.h>  // IWYU pragma: keep

namespace yaml {
/**
 * parent에서 name노드를 찾아 boolean으로 반환
 */
bool getAsBool(YAML::Node parent, std::string name, bool defaultValue = false);

uint getAsUInt(YAML::Node parent, std::string name, uint defaultValue = 0);

long getAsLong(YAML::Node parent, std::string name, long defaultValue = false);

std::string getAsString(YAML::Node parent, std::string name, std::string defaultValue = "");

bool getNode(YAML::Node &parent, std::string name, std::tuple<std::string, YAML::Node> &dest, int index = -1);

bool getNode(YAML::Node &parent, std::string name, YAML::Node &dest, int index = -1);

}



#endif /* SRC_YAMLUTILS_H_ */
