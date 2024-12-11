# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
#
if(NOT WIN32)
  add_subdirectory(cloud-daemon)

  if(XRT_XRT OR XRT_ALVEO)
    add_subdirectory(xbflash.qspi)
  endif()

endif(NOT WIN32)
