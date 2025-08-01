// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef AIE_PROFILE_UTIL_DOT_H
#define AIE_PROFILE_UTIL_DOT_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include "xdp/profile/plugin/aie_profile/aie_profile_defs.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp::aie::profile {

  #define START_TO_BYTES_TRANSFERRED_REPORT_EVENT_ID 3600
  #define INTF_TILE_LATENCY_REPORT_EVENT_ID          3601

  const std::map<xdp::module_type, uint16_t> counterBases = {
    {module_type::core,     static_cast<uint16_t>(0)},
    {module_type::dma,      BASE_MEMORY_COUNTER},
    {module_type::shim,     BASE_SHIM_COUNTER},
    {module_type::mem_tile, BASE_MEM_TILE_COUNTER},
    {module_type::uc,       BASE_UC_MDM_COUNTER}
  };

  const std::vector<XAie_ModuleType> falModuleTypes = {
    XAIE_CORE_MOD,
    XAIE_MEM_MOD,
    XAIE_PL_MOD,
    XAIE_MEM_MOD,
    XAIE_PL_MOD    // TODO: replace if/when there is an uC module type
  };

  enum adfAPI {
    START_TO_BYTES_TRANSFERRED,
    INTF_TILE_LATENCY
  };

  struct adfAPIResourceInfo {
    uint64_t srcPcIdx;
    uint64_t destPcIdx;
    uint64_t profileResult;
    bool isSourceTile = false;
  };

  const std::unordered_map<std::string, uint16_t> adfApiMetricSetMap = {
    {METRIC_BYTE_COUNT, static_cast<uint16_t>(START_TO_BYTES_TRANSFERRED_REPORT_EVENT_ID)},
    {METRIC_LATENCY,    static_cast<uint16_t>(INTF_TILE_LATENCY_REPORT_EVENT_ID)}
  };

  /**
   * @brief   Get metric sets for core modules
   * @param   hwGen integer representing the hardware generation
   * @return  Map of core metric set names with vectors of event IDs
   */
  std::map<std::string, std::vector<XAie_Events>> getCoreEventSets(const int hwGen);

  /**
   * @brief   Get metric sets for memory modules
   * @param   hwGen integer representing the hardware generation
   * @return  Map of memory metric set names with vectors of event IDs
   */
  std::map<std::string, std::vector<XAie_Events>> getMemoryEventSets(const int hwGen);
 
  /**
   * @brief   Get metric sets for interface modules
   * @param   hwGen integer representing the hardware generation
   * @return  Map of interface metric set names with vectors of event IDs
   */
  std::map<std::string, std::vector<XAie_Events>> getInterfaceTileEventSets(const int hwGen);

  /**
   * @brief   Get metric sets for memory tile modules
   * @param   hwGen integer representing the hardware generation
   * @return  Map of memory tile metric set names with vectors of event IDs
   */
  std::map<std::string, std::vector<XAie_Events>> getMemoryTileEventSets(const int hwGen);

  /**
   * @brief   Get metric sets for debug modules in microcontrollers
   * @param   hwGen integer representing the hardware generation
   * @return  Map of microcontroller metric set names with vectors of event IDs
   */
  //std::map<std::string, std::vector<XAie_Events>> getMicrocontrollerEventSets(const int hwGen);
  std::map<std::string, std::vector<uint32_t>> getMicrocontrollerEventSets(const int hwGen);

  /**
   * @brief  Modify configured events
   * @param type module type 
   * @param subtype plio/gmio type
   * @param events vector of events to replace with the respective channel 1 events
   * @param hwGen AIE hardware generation
   */
  void modifyEvents(const module_type type, const io_type subtype, const uint8_t channel,
                    std::vector<XAie_Events>& events, const int hwGen);

  /**
   * @brief Configure the individual AIE events for metric sets that use group events
   * @param aieDevInst AIE device instance
   * @param loc Tile location
   * @param mod AIE driver module type
   * @param type xdp module type
   * @param metricSet metric set to be configured
   * @param event metric set group event
   * @param channel channel to be configured
   */
   void configGroupEvents(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                          const XAie_ModuleType mod, const module_type type,
                          const std::string metricSet, const XAie_Events event,
                          const uint8_t channel);

  /**
   * @brief Configure the selection index to monitor channel number in memory tiles
   * @param aieDevInst AIE device instance
   * @param loc Tile location
   * @param type xdp module type
   * @param metricSet metric set to be configured
   * @param channel channel to be configured
   */
  void configEventSelections(XAie_DevInst* aieDevInst, const XAie_LocType loc,
                             const module_type type, const std::string metricSet,
                             const uint8_t channel);
  
  /**
   * @brief Get XAie module enum at the module index 
   * @param moduleIndex module index
   * @return  
   */
  XAie_ModuleType getFalModuleType(const int moduleIndex);

  /**
   * @brief Get base event number for a module
   * @param type module type
   * @return  
   */
  uint16_t getCounterBase(const xdp::module_type type);

  /**
   * @brief Check the match of the XAie enum module type with our xdp::module_type
   * @param type xdp::module_type enum type
   * @param mod AIE driver enum type
   */
  bool isValidType(const module_type type, const XAie_ModuleType mod);

  /**
   * @brief Get physical event IDs for metric set
   * @param aieDevInst AIE device instance
   * @param tileLoc tile location
   * @param xaieModType AIE driver enum type
   * @param xdpModType xdp module type
   * @param metricSet metric set to be configured
   * @param startEvent start event ID
   * @param endEvent end event ID
   */
  std::pair<uint16_t, uint16_t>
  getEventPhysicalId(XAie_DevInst* aieDevInst, XAie_LocType& tileLoc,
                     XAie_ModuleType& xaieModType, module_type xdpModType,
                     const std::string& metricSet, XAie_Events startEvent, 
                     XAie_Events endEvent);

  bool metricSupportsGraphIterator(std::string metricSet);
  bool profileAPIMetricSet(const std::string metricSet);
  uint16_t getAdfApiReservedEventId(const std::string metricSet);
  inline bool adfAPIStartToTransferredConfigEvent(uint32_t eventID) { 
    return (eventID == START_TO_BYTES_TRANSFERRED_REPORT_EVENT_ID); 
  }
  inline bool adfAPILatencyConfigEvent(uint32_t eventID) { 
    return (eventID == INTF_TILE_LATENCY_REPORT_EVENT_ID);
  }
  std::pair<int, XAie_Events> getPreferredPLBroadcastChannel();

  uint32_t convertToBeats(const std::string& metricSet, uint32_t bytes, uint8_t hwGen);

  /**
   * @brief Configure counters in Microblaze Debug Module (MDM) 
   * @param aieDevInst AIE device instance
   * @param col tile column
   * @param row tile row
   * @param events vector of events to use in counters
   */
  void configMDMCounters(XAie_DevInst* aieDevInst, int hwGen, uint8_t col, uint8_t row, 
                         const std::vector<uint32_t> events);

  /**
   * @brief Read counters in Microblaze Debug Module (MDM) 
   * @param aieDevInst AIE device instance
   * @param col tile column
   * @param row tile row
   * @param values vector of values from counters
   */
  void readMDMCounters(XAie_DevInst* aieDevInst, int hwGen, uint8_t col, uint8_t row, 
                       std::vector<uint64_t>& values);
  
}  // namespace xdp::aie::profile

#endif
