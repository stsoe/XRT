# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/gen # includes version.h
  )

file(GLOB XBMGMT_FILES
  "*.h"
  "*.cpp"
  )

set(XBMGMT_SRC ${XBMGMT_FILES})
add_executable(xbmgmt ${XBMGMT_SRC})

target_link_libraries(xbmgmt
  PRIVATE
  xrt_core_static
  xrt_coreutil_static
  pthread
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  uuid
  )

install (TARGETS xbmgmt RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR})
