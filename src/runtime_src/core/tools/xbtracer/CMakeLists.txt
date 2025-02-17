# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved
# -----------------------------------------------------------------------------
# Include the generated header files (e.g., version.h)
include_directories(${XRT_BINARY_DIR}/gen)

set(XBTRACER_NAME "xbtracer")

if(WIN32)
  set(XRT_HELPER_SCRIPTS "xbtracer" "xbtracer.bat")
else()
  set(XRT_HELPER_SCRIPTS "xbtracer")
endif()

set(SRCS src/app/launcher.cpp)
if (WIN32)
  list(APPEND SRCS src/app/getopt.c)
endif()

add_executable(${XBTRACER_NAME} ${SRCS})
set_target_properties(${XBTRACER_NAME} PROPERTIES DEBUG_POSTFIX "_d")

install(TARGETS ${XBTRACER_NAME}
  RUNTIME DESTINATION ${XRT_INSTALL_UNWRAPPED_DIR} COMPONENT ${XRT_BASE_COMPONENT})

install(PROGRAMS ${XRT_HELPER_SCRIPTS}
  DESTINATION ${XRT_INSTALL_BIN_DIR}
  COMPONENT ${XRT_BASE_COMPONENT})

add_subdirectory(src/lib)
