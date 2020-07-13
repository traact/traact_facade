/*  BSD 3-Clause License
 *
 *  Copyright (c) 2020, FriederPankratz <frieder.pankratz@gmail.com>
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include <iostream>
#include <spdlog/spdlog.h>
#include "PluginFactory.h"

bool traact::facade::PluginFactory::addLibrary(const std::string &filename) {
  PluginPtr newPlugin;

  spdlog::info("loading library file: {0}", filename);

  try {
    newPlugin = std::make_shared<Plugin>(filename);
    if (!newPlugin->init()) {
      spdlog::error("init failed for: {0}", filename);
      return false;
    }

  } catch (...) {
    spdlog::error("init thows exception for: {0}", filename);
    return false;
  }

  //TODO gather all names in a set, check of set is smaller, if so then there where dublicate entries, error

  for (const auto &datatype_name : newPlugin->datatype_names) {
    registered_datatypes_.emplace(std::make_pair(datatype_name, newPlugin));
  }
  for (const auto &pattern_name : newPlugin->pattern_names) {
    registered_patterns_.emplace(std::make_pair(std::string(pattern_name), newPlugin));
  }

  return true;

}
bool traact::facade::PluginFactory::removeLibrary(const std::string &filename) {
  return false;
}
std::vector<std::string> traact::facade::PluginFactory::getDatatypeNames() {
  std::vector<std::string> result;
  std::transform(registered_datatypes_.begin(), registered_datatypes_.end(),
                 std::inserter(result, result.end()),
                 [](auto pair) { return pair.first; });
  return result;
}
std::vector<std::string> traact::facade::PluginFactory::getPatternNames() {
  std::vector<std::string> result;
  /*for (const auto &item : registered_patterns_) {
    result.emplace_back(item.first);

  }*/
  std::transform(registered_patterns_.begin(), registered_patterns_.end(),
                 std::inserter(result, result.end()),
                 [](auto pair) { return pair.first; });
  return result;
}
traact::facade::PluginFactory::FactoryObjectPtr traact::facade::PluginFactory::instantiateDataType(const std::string &datatype_name) {
  return registered_datatypes_.at(datatype_name)->instantiateDataType(datatype_name);
}
traact::facade::PluginFactory::PatternPtr traact::facade::PluginFactory::instantiatePattern(const std::string &pattern_name) {
  return registered_patterns_.at(pattern_name)->instantiatePattern(pattern_name);
}
traact::facade::PluginFactory::ComponentPtr traact::facade::PluginFactory::instantiateComponent(const std::string &pattern_name,
                                                                                                const std::string &new_component_name) {
  return registered_patterns_.at(pattern_name)->instantiateComponent(pattern_name, new_component_name);
}
traact::facade::PluginFactory::Plugin::Plugin(const std::string &library_file_name) : library_(library_file_name) {

}
traact::facade::PluginFactory::Plugin::~Plugin() {
  teardown();
}
bool traact::facade::PluginFactory::Plugin::init() {
  using namespace rttr;

  if (library_.is_loaded()) {
    spdlog::info("library alreay loaded");
  } else {

    if (!library_.load()) {
      auto error_string = library_.get_error_string();
      spdlog::error("error loading plugin: {0}", std::string(error_string.begin(), error_string.end()));
    }
  }

  for (const auto &t : library_.get_types()) // returns all registered types from this library
  {
    if (!t.is_class() || t.is_wrapper())
      continue;

    spdlog::info("loading plugin: {0}", std::string(t.get_name().begin(), t.get_name().end()));

    constructor ctor = t.get_constructor();
    if (!ctor.is_valid()) {
      spdlog::error("could not load default constructor () for {0}",
                    std::string(t.get_name().begin(), t.get_name().end()));
      return false;
    }
	
    variant var = ctor.invoke();
    if (!var.is_valid()) {
      spdlog::error("invoking constructor() failed for {0}", std::string(t.get_name().begin(), t.get_name().end()));
      return false;
    }


	auto shared_from_base_method = t.get_method("shared_from_base");
	if (!shared_from_base_method.is_valid()) {
		spdlog::error("shared_from_base method not available for: {0}", std::string(t.get_name().begin(), t.get_name().end()));
		return false;
	}

	
    //traact::facade::Plugin::Ptr traact_plugin = var.get_value<traact::facade::Plugin::Ptr>();
	//traact::facade::Plugin::Ptr traact_plugin = shared_from_base_method.invoke(var).get_value<traact::facade::Plugin::Ptr>();
    traact::facade::Plugin::Ptr traact_plugin = var.get_value<traact::facade::Plugin::Ptr>();


    std::vector<std::string> tmp;
    traact_plugin->fillDatatypeNames(tmp);
    for (const auto &datatype_name : tmp) {
      spdlog::info("register datatype: {0}", datatype_name);
      datatype_to_traact_plugin.emplace(std::make_pair(datatype_name, traact_plugin));
    }
    datatype_names.insert(datatype_names.end(), tmp.begin(), tmp.end());

    tmp.clear();
    traact_plugin->fillPatternNames(tmp);
    for (const auto &pattern_name : tmp) {
      spdlog::info("register pattern: {0}", pattern_name);
      pattern_to_traact_plugin.emplace(std::make_pair(pattern_name, traact_plugin));
    }
    pattern_names.insert(pattern_names.end(), tmp.begin(), tmp.end());

  }

  return true;

}
bool traact::facade::PluginFactory::Plugin::teardown() {
  for (auto &item : pattern_to_traact_plugin) {
    item.second.reset();
  }
  for (auto &item : datatype_to_traact_plugin) {
    item.second.reset();
  }
  return library_.unload();
}
traact::facade::PluginFactory::FactoryObjectPtr traact::facade::PluginFactory::Plugin::instantiateDataType(const std::string &datatype_name) {	
	auto find_result = datatype_to_traact_plugin.find(datatype_name);
	if (find_result == datatype_to_traact_plugin.end()) {		
		throw std::runtime_error(std::string("Trying to instantiate unkown datatype: ") + datatype_name);
	}
	return find_result->second->instantiateDataType(datatype_name);
  
}
traact::facade::PluginFactory::PatternPtr traact::facade::PluginFactory::Plugin::instantiatePattern(const std::string &patternName) {
	auto find_result = pattern_to_traact_plugin.find(patternName);
	if (find_result == pattern_to_traact_plugin.end()) {
		throw std::runtime_error(std::string("Trying to instantiate unkown pattern: ") + patternName);
	}
	return find_result->second->instantiatePattern(patternName);  
}
traact::facade::PluginFactory::ComponentPtr traact::facade::PluginFactory::Plugin::instantiateComponent(const std::string &patternName,
                                                                                                        const std::string &new_component_name) {

	auto find_result = pattern_to_traact_plugin.find(patternName);
	if (find_result == pattern_to_traact_plugin.end()) {
		throw std::runtime_error(std::string("Trying to instantiate unkown component: ") + patternName);
	}
	return find_result->second->instantiateComponent(patternName, new_component_name);
  
}

// It is not possible to place the macro multiple times in one cpp file. When you compile your plugin with the gcc toolchain,
// make sure you use the compiler option: -fno-gnu-unique. otherwise the unregistration will not work properly.
RTTR_PLUGIN_REGISTRATION // remark the different registration macro!
{

  using namespace rttr;
registration::class_<traact::facade::Plugin>("Plugin").constructor<>()
	//(
	//policy::ctor::as_std_shared_ptr
	//)
	.method("shared_from_base", &traact::facade::Plugin::shared_from_base<traact::facade::Plugin>);
}