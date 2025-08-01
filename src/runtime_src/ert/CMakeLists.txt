# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

set(ERT_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/fw")
set(ERT_INSTALL_FIRMWARE_PREFIX "/lib/firmware/xilinx")

if ((EXISTS $ENV{XILINX_VITIS}/gnu/microblaze) AND (NOT XRT_EDGE) AND (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64"))

message("-- MicroBlaze toolchain found, preparing ERT build")
set(ERT_BUILD_ALL "yes")
add_subdirectory(scheduler)

add_custom_target(ert ALL
 DEPENDS scheduler
 )

elseif (DEFINED ENV{XRT_FIRMWARE_DIR})

file(GLOB ERTFW_FILES
  "$ENV{XRT_FIRMWARE_DIR}/sched*.bin"
  )

message("-- ERT firmare files copied from ${ERTFW_FILES}")

install(FILES
  ${ERTFW_FILES}
  DESTINATION ${ERT_INSTALL_PREFIX}
  )

install(FILES
  ${ERTFW_FILES}
  DESTINATION ${ERT_INSTALL_FIRMWARE_PREFIX}
  )

set(ERT_BUILD_ALL "no")
add_subdirectory(scheduler)

else()

message(WARNING
"****************************************************************\n\
No firmware files built or copied, resulting XRT package will be missing ERT \
scheduler firmware.  Use build.sh -ertfw <dir> to specify path to a directory \
with firmware to copy during XRT build.\n
****************************************************************")
set(ERT_BUILD_ALL "no")
add_subdirectory(scheduler)

endif()
