/*
 * psdcn.hpp
 *
 * psdcn variables
 */

#ifndef __PUBSUB_HPP__
#define __PUBSUB_HPP__

#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/face.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "datatype.h"

using namespace ndn;

using FacePtr = std::shared_ptr<Face>;
using InterestPtr = std::shared_ptr<Interest>;

using DocumentPtr = std::shared_ptr<rapidjson::Document>;


#endif /* __PUBSUB_HPP__ */
