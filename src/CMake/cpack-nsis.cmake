# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
cmake_minimum_required(VERSION 3.20)

set(CPACK_PACKAGE_VENDOR "Advanced Micro Devices, Inc.")
set(CPACK_PACKAGE_CONTACT "soren.soe@amd.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XDNA RunTime stack for use with AMD FPGA and NPU platforms")

set(CPACK_PACKAGE_NAME "XRT")
set(CPACK_PACKAGE_VERSION_RELEASE "${XRT_VERSION_RELEASE}")
set(CPACK_PACKAGE_VERSION "${XRT_VERSION_MAJOR}.${XRT_VERSION_MINOR}.${XRT_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_MAJOR "${XRT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${XRT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${XRT_VERSION_PATCH}")
set(CPACK_REL_VER "windows")
set(CPACK_ARCH "x86_64")

set(CPACK_ARCHIVE_COMPONENT_INSTALL 1)
set(CPACK_PACKAGE_FILE_NAME "xrt-${CPACK_PACKAGE_VERSION}-${CPACK_REL_VER}-${CPACK_ARCH}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "XRT")
set(CPACK_NSIS_PACKAGE_NAME "XRT ${CPACK_PACKAGE_VERSION}")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MENU_LINKS "")

# Initialize CPACK_COMPONENTS_ALL variable and remove
# bogus improperly bucketed components
get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Runtime" "runtime")

message("-- ${CMAKE_BUILD_TYPE} ${PACKAGE_KIND} package")

include(CPack)

cpack_add_component(base
  DISPLAY_NAME "XRT Base Runtime"
  DESCRIPTION "XRT tools and runtime libraries. Required if development is installed"
  GROUP Runtime
  )

cpack_add_component(base_dev
  DISPLAY_NAME "XRT SDK"
  DESCRIPTION "XRT software development kit with header files and link libraries for application development."
  GROUP Development
  DEPENDS base
  )

cpack_add_component(npu
  DISPLAY_NAME "NPU Runtime"
  DESCRIPTION "NPU optional runtime libraries."
  GROUP Runtime
  DISABLED
  )

cpack_add_component(npu_dev
  DISPLAY_NAME "NPU Development"
  DESCRIPTION "NPU optional development header files and link libraries."
  GROUP Development
  DEPENDS npu
  DISABLED
  )

cpack_add_component(alveo
  DISPLAY_NAME "Alveo Runtime"
  DESCRIPTION "Alveo optional runtime libraries."
  GROUP Runtime
  DISABLED
  )

cpack_add_component(alveo_dev
  DISPLAY_NAME "Alveo Development"
  DESCRIPTION "Alveo optional development header files and link libraries."
  GROUP Development
  DEPENDS alveo
  DISABLED
  )

cpack_add_component(xrt
  DISPLAY_NAME "XRT Legacy Runtime"
  DESCRIPTION "XRT legacy Runtime and Development."
  GROUP Development
  DISABLED
  )

