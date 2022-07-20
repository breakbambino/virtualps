/*
 * ndnutils.h
 *
 *  Created on: 2020. 9. 7.
 *      Author: ymtech
 */

#ifndef __NDNUTILS_H__
#define __NDNUTILS_H__


#include <string>
#include <ndn-cxx/util/logger.hpp>


#include <string>
#include <ndn-cxx/util/logger.hpp>

#define NDN_LOG_TRACE_SS( x ) \
do { \
	std::stringstream ss; \
	ss << x; \
	NDN_LOG_TRACE(ss.str()); \
} while (false)

#define NDN_LOG_DEBUG_SS( x ) \
do { \
	std::stringstream ss; \
	ss << x; \
	NDN_LOG_DEBUG(ss.str()); \
} while (false)

#define NDN_LOG_INFO_SS( x ) \
do { \
	std::stringstream ss; \
	ss << x; \
	NDN_LOG_INFO(ss.str()); \
} while (false)

#define NDN_LOG_WARN_SS( x ) \
do { \
	std::stringstream ss; \
	ss << x; \
	NDN_LOG_WARN(ss.str()); \
} while (false)

#define NDN_LOG_ERROR_SS( x ) \
do { \
	std::stringstream ss; \
	ss << x; \
	NDN_LOG_ERROR(ss.str()); \
} while (false)

#define NDN_LOG_FATAL_SS( x ) \
do { \
	std::stringstream ss; \
	ss << x; \
	NDN_LOG_FATAL(ss.str()); \
} while (false)


#endif /* __NDNUTILS_H__ */
