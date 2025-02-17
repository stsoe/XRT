# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

# This generates the config.h file for the XRT library.  CMake
# is configured to generate a debug prefix for all libraries
# such that debug and release can co-exist in the same location.
# This is particularly useful for the windows SDK which is created
# from both release and debug profiles.

# Do not regenerate config.h if it already exists, this is to
# avoid recompiling all files that include config.h
if (EXISTS ${PROJECT_BINARY_DIR}/gen/build_config.h)
  return()
endif()

message(STATUS "Generating config.h")

# The generated config.h file is internal to the build
# it is not installed.
configure_file(
  ${XRT_SOURCE_DIR}/CMake/config/config.h.in
  ${PROJECT_BINARY_DIR}/gen/build_config.h
)

