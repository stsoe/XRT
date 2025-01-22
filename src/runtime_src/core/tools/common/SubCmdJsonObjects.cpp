// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include <iostream>
#include <string>
#include <unordered_map>
#include <numeric>

#include <boost/property_tree/ptree.hpp>
#include <boost/program_options.hpp>
#include "SubCmdJsonObjects.h"

namespace SubCmdJsonObjects {

static constexpr std::string_view const_name_literal = "name";
static constexpr std::string_view const_description_literal = "description";
static constexpr std::string_view const_tag_literal = "tag";
static constexpr std::string_view const_alias_literal = "alias";
static constexpr std::string_view const_default_value_literal = "default_value";
static constexpr std::string_view const_option_type_literal = "option_type";
static constexpr std::string_view const_value_type_literal = "value_type";
static constexpr std::string_view const_options_literal = "options";

static std::unordered_map<std::string, ValueType> valueTypeMap = {
  {"bool", ValueType::boolean},
  {"string", ValueType::string},
  {"array", ValueType::array},
  {"none", ValueType::none}
};

OptionBasic::OptionBasic(const pt::ptree& configurations)
  : m_name(configurations.get<std::string>(std::string(const_name_literal))), 
    m_description(concatenate(configurations, std::string(const_description_literal))), 
    m_tag(configurations.get<std::string>(std::string(const_tag_literal)))
    {}

std::string
OptionBasic::concatenate(const pt::ptree& pt, const std::string& path) const
{
  try {
    const auto& child = pt.get_child(path);

    // Check if the path is a list
    if (child.empty()) {
      // Return the single string value
      return pt.get<std::string>(path);
    }
    else {
      // If the child has size, it means its a list. Concatenate strings from the list
      return std::accumulate(child.begin(), child.end(), std::string(),
        [](const std::string& acc, const boost::property_tree::ptree::value_type& item) {
          return acc + item.second.get_value<std::string>();
        });
    }
  } catch (const pt::ptree_error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return "";
  }
}

void
OptionBasic::printOption() const
{
  std::cout << "Name: " << m_name << std::endl;
  std::cout << "Description: " << m_description << std::endl;
  std::cout << "Tag: " << m_tag << std::endl;
}

SubCommandOption::SubCommandOption(const pt::ptree& configurations):
    OptionBasic(configurations),
    m_alias(configurations.get<std::string>(std::string(const_alias_literal), "")),
    m_defaultValue(configurations.get<std::string>(std::string(const_default_value_literal), "")),
    m_optionType(configurations.get<std::string>(std::string(const_option_type_literal), "")),
    m_valueType(configurations.get<std::string>(std::string(const_value_type_literal), "")),
    m_ptEmpty(pt::ptree()),
    m_subOptionMap(createBasicOptions(configurations.get_child(std::string(const_options_literal), m_ptEmpty))){}

/**
 * Adds the sub-command option to the options description.
 * This method checks the option type and adds the option to the provided options description
 * based on its value type (BOOL, STRING, ARRAY).
 * If the option type does not match the provided optionsType, the option is not added.
 * If the value type is invalid, an exception is thrown.
 */
void 
SubCommandOption::addProgramOption(po::options_description& options, const std::string& optionsType) 
{
  if (m_optionType != optionsType) return;

  auto valueType = valueTypeMap.find(m_valueType);
  if (valueType == valueTypeMap.end()) {
    throw std::runtime_error("Invalid value type for option " + m_name);
  }
  switch (valueType->second){
    case ValueType::boolean:
    {
      auto defaultVal = m_defaultValue == "true" ? true : false;
      options.add_options()((m_name + "," + m_alias).c_str()
                            , po::value<bool>()->default_value(defaultVal)
                            , m_description.c_str());
      break;
    }
    case ValueType::string:
    {
      options.add_options()((m_name + "," + m_alias).c_str()
                            , po::value<std::string>()->implicit_value(m_defaultValue)
                            , m_description.c_str());
      break;
    }
    case ValueType::array:
    {
      options.add_options()((m_name + "," + m_alias).c_str()
                            , po::value<std::vector<std::string>>()->multitoken()->zero_tokens()
                            , m_description.c_str());
      break;
    }
    case ValueType::none:
    {
      options.add_options()((m_name + "," + m_alias).c_str()
                            , po::bool_switch()
                            , m_description.c_str());
      break;
    }
    default:
      throw std::runtime_error("Invalid value type for option " + m_name);
  }
}

std::unordered_map<std::string, OptionBasic>
SubCommandOption::createBasicOptions(const pt::ptree& pt)
{
  std::unordered_map<std::string, OptionBasic> optionMap;
  for (const auto& [key, value] : pt) {
    optionMap.emplace(value.get<std::string>(std::string(const_name_literal)), OptionBasic(value));
  }
  return optionMap;
}

void
SubCommandOption::printOption() const
{
  std::cout << "Name: " << m_name << std::endl;
  std::cout << "Description: " << m_description << std::endl;
  std::cout << "Tag: " << m_tag << std::endl;
  std::cout << "Alias: " << m_alias << std::endl;
  std::cout << "Default Value: " << m_defaultValue << std::endl;
  std::cout << "Option Type: " << m_optionType << std::endl;
  std::cout << "Value Type: " << m_valueType << std::endl;
  for (const auto& [key, value] : m_subOptionMap) {
    value.printOption();
  }
}

SubCommand::SubCommand(const pt::ptree& configurations) :
  OptionBasic(configurations),
  m_optionMap(createSubCommandOptions(configurations.get_child(std::string(const_options_literal)))) 
  {}

std::unordered_map<std::string, SubCommandOption>
SubCommand::createSubCommandOptions(const pt::ptree& pt)
{
  std::unordered_map<std::string, SubCommandOption> optionMap;
  for (const auto& [key, value] : pt) {
    optionMap.emplace(value.get<std::string>(std::string(const_name_literal)), SubCommandOption(value));
  }
  return optionMap;
}

void 
SubCommand::addProgramOptions(po::options_description& options, const std::string& optionsType)
{
  for (auto& [optionName, optionObj] : m_optionMap) {
    optionObj.addProgramOption(options, optionsType);
  }
}

/**
 * Creates sub-commands from the property tree.
 * This method parses the property tree to create a map of sub-command names to SubCommand objects.
 * Only sub-commands matching the provided subCommand name are included.
 */
std::unordered_map<std::string, SubCommand>
JsonConfig::createSubCommands(const pt::ptree& pt, const std::string& subCommand)
{
  std::unordered_map<std::string, SubCommand> subCommandMap;
  for (const auto& [key, value] : pt) {
    if (value.get<std::string>(std::string(const_name_literal)) != subCommand) {
      continue;
    }
    subCommandMap.emplace(value.get<std::string>(std::string(const_name_literal)), SubCommand(value));
  }
  return subCommandMap;
}

/**
 * Adds program options to the options description for a specific sub-command.
 * This method finds the specified sub-command and adds its options to the provided options description.
 * If the sub-command is not found, an exception is thrown.
 */
void
JsonConfig::addProgramOptions(po::options_description& options
                              , const std::string& optionsType
                              , const std::string& subCommand)
{
  auto subCommandIter = m_subCommandMap.find(subCommand);
  if (subCommandIter == m_subCommandMap.end()) {
    throw std::runtime_error("Subcommand not found");
  }
  subCommandIter->second.addProgramOptions(options, optionsType);
}

void
JsonConfig::printConfigurations() const
{
  for (const auto& [key, value] : m_subCommandMap) {
    std::cout << "Subcommand: " << key << std::endl;
    std::cout << "Description: " << value.getDescription() << std::endl;
    std::cout << "Tag: " << value.getTag() << std::endl;
    for (const auto& [key2, value2] : value.getOptionMap()) {
      value2.printOption();
    }
  }
}
} // namespace SubCmdJsonObjects