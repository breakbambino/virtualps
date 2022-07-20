#ifndef __TYPEDPARAMETER_H__
#define __TYPEDPARAMETER_H__

#include <type_traits>
#include "datatype.h"

class ParametizedData {
public:
	ParametizedData() {
		m_type = DataType::None;
	}
	~ParametizedData() {}
public:
	enum DataType m_type;
};

template <typename T>
class TypedParameter : public ParametizedData {
public:
	TypedParameter(T data) {
		if (std::is_same<T, long>::value) {
			m_type = DataType::Int;
		} else if (std::is_same<T, double>::value) {
			m_type = DataType::Real;
		} else if (std::is_same<T, std::string>::value) {
			m_type = DataType::String;
		}
		m_data = data;
	}

	T m_data;
};


#endif // __TYPEDPARAMETER_H__
