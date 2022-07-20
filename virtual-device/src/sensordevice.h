#ifndef __SENSORDEVICE_H__
#define SENSORDEVICE_H__

#include "stlutils.h"
#include "typedparameter.h"

class SensorDevice;
using SensorPtr = std::shared_ptr<SensorDevice>;

class SensorDevice {
public:
	SensorDevice(std::string name)
	: m_name(name)
	, m_type(DataType::None)
	, m_random(getRandomGenerator()) {
	}
	SensorDevice(std::string name, DataType type)
	: m_name(name)
	, m_type(type)
	, m_random(getRandomGenerator()) {
	}
	~SensorDevice(){}

	std::string getName() {
		return m_name;
	}

	void setType(DataType type) {
		m_type = type;
	}

	enum DataType getType() {
		return m_type;
	}

	std::vector<ParametizedData *>::iterator begin() {
		return m_datas.begin();
	}

	std::vector<ParametizedData *>::iterator end() {
		return m_datas.end();
	}

	bool push_back(ParametizedData *data) {
		if(m_type == DataType::None) {
			m_type = data->m_type;
			m_datas.push_back(data);
			return true;
		}

		// 이전 데이터 Type과 같은 데이터 Type인지 확인
		if (data->m_type == DataType::Int) {
			if(m_type != DataType::Int) {
				return false;
			}
		} else if (data->m_type == DataType::Real) {
			if(m_type != DataType::Real) {
				return false;
			}
		} else if (data->m_type == DataType::String) {
			if(m_type != DataType::String) {
				return false;
			}
		}

		m_datas.push_back(data);
		return true;
	}

	size_t dataSize() {
		return m_datas.size();
	}

	std::vector<ParametizedData *> & getData() {
		return m_datas;
	}

	bool generateData(long &data) {
		if(m_type != DataType::Int) {
			return false;
		}

		if(m_datas.size() == 0) {
			return false;
		}

		ParametizedData *first = NULL;
		ParametizedData *last = NULL;

		std::vector<ParametizedData *>::iterator dataiter;
		for(dataiter = m_datas.begin(); dataiter != m_datas.end(); dataiter++) {
			if(first == NULL) {
				first = *dataiter;
			}

			last = *dataiter;
		}

		TypedParameter<long> *firstparam = (TypedParameter<long> *)first;
		TypedParameter<long> *lastparam = (TypedParameter<long> *)last;
		long v1 = firstparam->m_data;
		long v2 = lastparam->m_data;
		if(v2 < v1) {
			std::swap(v1, v2);
		}
		data = generateUniformIntData(m_random, v1, v2);

		return true;
	}

	bool generateData(double &data) {
		if(m_type != DataType::Real) {
			return false;
		}

		if(m_datas.size() == 0) {
			return false;
		}

		ParametizedData *first = NULL;
		ParametizedData *last = NULL;

		std::vector<ParametizedData *>::iterator dataiter;
		for(dataiter = m_datas.begin(); dataiter != m_datas.end(); dataiter++) {
			if(first == NULL) {
				first = *dataiter;
			}

			last = *dataiter;
		}
		TypedParameter<double> *firstparam = (TypedParameter<double> *)first;
		TypedParameter<double> *lastparam = (TypedParameter<double> *)last;
		double v1 = firstparam->m_data;
		double v2 = lastparam->m_data;
		if(v2 < v1) {
			std::swap(v1, v2);
		}
		data = generateUniformRealData(m_random, v1, v2);

		return true;
	}

	bool generateData(std::string &data) {
		if(m_type != DataType::String) {
			return false;
		}

		if(m_datas.size() == 0) {
			return false;
		}

		std::vector<std::string> strlist;

		std::vector<ParametizedData *>::iterator dataiter;
		for(dataiter = m_datas.begin(); dataiter != m_datas.end(); dataiter++) {
			TypedParameter<std::string> *param = (TypedParameter<std::string> *)*dataiter;
			strlist.push_back(param->m_data);
		}

		data = generateUniformStringData(m_random, strlist);

		return true;
	}

public:
	std::string m_name;
	enum DataType m_type;
	std::mt19937_64 m_random;
	std::vector<ParametizedData *> m_datas;
};


#endif // SENSORDEVICE_H__
