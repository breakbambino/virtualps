/*
 * yamlutils.cpp
 *
 *  Created on: 2021. 6. 23.
 *      Author: ymtech
 */

#include "yamlutils.h"

namespace yaml {

bool getAsBool(YAML::Node parent, std::string name, bool defaultValue) {
	if(parent[name] == NULL) {
		return defaultValue;
	}

	return parent[name].as<bool>();
}

uint getAsUInt(YAML::Node parent, std::string name, uint defaultValue) {
	if(parent[name] == NULL) {
		return defaultValue;
	}

	return parent[name].as<uint>();
}

long getAsLong(YAML::Node parent, std::string name, long defaultValue) {
	if(parent[name] == NULL) {
		return defaultValue;
	}

	return parent[name].as<long>();
}

std::string getAsString(YAML::Node parent, std::string name, std::string defaultValue) {
	if(parent[name] == NULL) {
		return defaultValue;
	}

	return parent[name].as<std::string>();
}

bool getNode(YAML::Node &parent, std::string name, std::tuple<std::string, YAML::Node> &dest, int index) {
	if(parent[name] == NULL) {
		return false;
	}
	if(index < 0) {
		std::get<1>(dest) = parent[name];
		return true;
	}
	YAML::Node child = parent[name];
	if(child.IsSequence()){
		if(index >= child.size()){
			return false;
		}
		std::get<1>(dest) = child[index];
		return true;
	}
	if(child.IsMap()){
		int i;
		YAML::Node::iterator iter;
		for(iter = child.begin(), i = 0; iter != child.end(); ++ iter, ++ i) {
			if(i==index){
				std::get<0>(dest) = iter->first.Scalar();
				std::get<1>(dest) = iter->second;
				return true;
			}
		}
	}
	if(child.IsScalar()){
		std::get<0>(dest) = name;
		std::get<1>(dest) = child;
		return true;
	}
	return false;
}

bool getNode(YAML::Node &parent, std::string name, YAML::Node &dest, int index) {
	if(parent[name] == NULL) {
		return false;
	}
	if(index < 0) {
		dest = parent[name];
		return true;
	}
	YAML::Node child = parent[name];
	if(child.IsSequence()){
		if(index >= child.size()){
			return false;
		}
		dest = child[index];
		return true;
	}
	if(child.IsMap()){
		int i;
		YAML::Node::iterator iter;
		for(iter = child.begin(), i = 0; iter != child.end(); ++ iter, ++ i) {
			if(i==index){
				dest = iter->second;
				return true;
			}
		}
	}
	if(child.IsScalar()){
		dest = child;
		return true;
	}
	return false;
}

} // end of namespace yaml
