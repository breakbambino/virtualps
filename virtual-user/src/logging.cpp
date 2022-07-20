/*
 * logging.h
 *
 *  Created on: 2020. 9. 9.
 *      Author: ymtech
 */

#include <string>

#include <ndn-cxx/util/logger.hpp>

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

#include "logging.h"

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

using namespace ndn::util;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "Timestamp", std::string)
void add_text_file_sink(LogSetting & setting) {
	std::string filename, format;
	long rotation_size = 16384;

	if(setting.file_name.empty()) {
		filename = "log_%Y%m%dT%H%M%S-%N.txt";
	} else {
		filename = setting.file_name;
	}
	if(setting.format.empty()) {
		format = "[%TimeStamp%] (%Severity%) : %Message%";
	} else {
		format = setting.format;
	}
	if(0 < setting.rotation_size) {
		rotation_size = setting.rotation_size;
	}

	boost::shared_ptr< sinks::text_file_backend > file_backend = boost::make_shared< sinks::text_file_backend >(
		keywords::file_name = filename,
		keywords::rotation_size = rotation_size,
		keywords::auto_flush = true,
		keywords::open_mode = (std::ios::out | std::ios::app | std::ios::ate),

		// 시점 (일 변경시)
		keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),

		// 주기 (1분마다)
		//keywords::time_based_rotation = sinks::file::rotation_at_time_interval(boost::posix_time::minutes(1)),

		keywords::format = format,
		keywords::min_free_space= setting.max_size * 10
	);

	file_backend->auto_flush(true);

	typedef sinks::synchronous_sink< sinks::text_file_backend > sink_t;
	boost::shared_ptr< sink_t > sink(new sink_t(file_backend));

	namespace expr = boost::log::expressions;
	sink->set_formatter(
			expr::stream << expr::attr<std::string>(timestamp.get_name()) << " "
					<< std::setw(5)
					<< expr::attr<LogLevel>(log::severity.get_name()) << ": "
					<< "[" << expr::attr<std::string>(log::module.get_name())
					<< "] " << expr::smessage);

	sink->locked_backend()->set_file_collector(
			sinks::file::make_collector(keywords::target = setting.target,
					keywords::max_size = setting.max_size));

	sink->locked_backend()->scan_for_files();

	logging::core::get()->add_sink(sink);
}

void add_text_file_sink(std::string path, std::string prefix) {

	LogSetting setting;
	//setting.file_name = "subscriber-%Y%m%d_%H:%M.txt";
	setting.file_name = prefix+"-%Y%m%d_%H%M-%3N.txt";
	setting.format = "[%TimeStamp%] (%Severity%) : %Message%";
	setting.format_date_time = "%Y-%m-%d %H:%M:%S.%f";
	setting.file_ordering_window_sec = 1;
	setting.rotation_size = 1024L*1024L*1024L; // 1G
	setting.max_size = 100L*1024L*1024L*1024L; // 100G
	setting.target = path;

	add_text_file_sink(setting);
}

void remove_all_sinks() {
	logging::core::get()->remove_all_sinks();
}
