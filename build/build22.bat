@ECHO OFF

REM SPDX-License-Identifier: Apache-2.0
REM Copyright (C) 2022 Xilinx, Inc.  All rights reserved.
REM Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
setlocal enabledelayedexpansion
set SCRIPTDIR=%~dp0
set SCRIPTDIR=%SCRIPTDIR:~0,-1%
set BUILDDIR=%SCRIPTDIR%

set DEBUG=1
set RELEASE=1
set EXT_DIR=C:/Xilinx/XRT/ext.new
set SDK=0
set CMAKEFLAGS=
set CLEAN=0
set NOINIT=0
set NOCMAKE=0
set NOBUILD=0
set BASE=0
set NPU=0
set ALVEO=0
set NOCTEST=0
set GENERATOR="Visual Studio 17 2022"

IF DEFINED MSVC_PARALLEL_JOBS ( SET LOCAL_MSVC_PARALLEL_JOBS=%MSVC_PARALLEL_JOBS%) ELSE ( SET LOCAL_MSVC_PARALLEL_JOBS=3 )

:parseArgs
  if [%1] == [] (
    goto argsParsed
  ) else (
  if [%1] == [-clean] (
    SET CLEAN=1
  ) else (
  if [%1] == [-help] (
    goto Help
  ) else (
  if [%1] == [-dbg] (
    if [%DEBUG%] == [0] (
      echo build22.bat: -dbg and -opt are mutually exclusive, specify one or none
      exit /B 1
    )
    set DEBUG=1
    set RELEASE=0
  ) else (
  if [%1] == [-opt] (
    if [%RELEASE%] == [0] (
      echo build22.bat: -opt and -dbg are mutually exclusive, specify one or none
      exit /B 1
    )
    set RELEASE=1
    set DEBUG=0
  ) else (
  if [%1] == [-nobuild] (
    if [%DEBUG%] == [0] (
      echo build22.bat: -nobuild cannot be used -dbg or -opt
      exit /B 1
    )
    if [%RELEASE%] == [0] (
      echo build22.bat: -nobuild cannot be used -opt or -dbg
      exit /B 1
    )
    set DEBUG=0
    set RELEASE=0
  ) else (
  if [%1] == [-ext] (
    shift
    set EXT_DIR=%1
  ) else (
  if [%1] == [-base] (
    set BASE=1
    set CMAKEFLAGS=%CMAKEFLAGS% -DXRT_BASE=1
  ) else (
  if [%1] == [-npu] (
    set NPU=1
    set CMAKEFLAGS=%CMAKEFLAGS% -DXRT_NPU=1
  ) else (
  if [%1] == [-alveo] (
    set ALVEO=1
    set CMAKEFLAGS=%CMAKEFLAGS% -DXRT_ALVEO=1
  ) else (
  if [%1] == [-noinit] (
    set NOINIT=1
  ) else (
  if [%1] == [-nocmake] (
    set NOCMAKE=1
    set NOINIT=1
  ) else (
  if [%1] == [-noabi] (
    set CMAKEFLAGS=%CMAKEFLAGS% -DDISABLE_ABI_CHECK=1
  ) else (
  if [%1] == [-sdk] (
    set SDK=1
    set DEBUG=1
    set RELEASE=1
  ) else (
  if [%1] == [-hip] (
    set CMAKEFLAGS=%CMAKEFLAGS% -DXRT_ENABLE_HIP=ON
  ) else (
    echo Unknown option: %1
    goto Help
  )))))))))))))))
  shift
  goto parseArgs

:argsParsed

if [%CLEAN%] == [1] (
  if EXIST %BUILDDIR%\WBuild (
    echo Removing 'WBuild' directory...
    rmdir /S /Q %BUILDDIR%\WBuild
  )
  exit /B 0
)

if [%NPU%+%ALVEO%+%BASE%] gtr 1 (
  echo build22.bat: -npu, -alveo, -base are mutually exclusive
  exit /B 1
)

if [%NOCMAKE%] == [0] if EXIST %BUILDDIR%\WBuild\CMakeCache.txt (
    echo Disabling cmake configuration; using existing build configuration
    echo Use -clean to remove existing build configuration
    set NOCMAKE=1
    set NOINIT=1
)

if [%NOINIT%] == [0] (
   echo Updating Git submodules, use -noinit option to avoid updating
   git submodule update --init --progress --recursive
)

if [%NOCMAKE%] == [0] (
   echo Configuring CMake project
   set CMAKEFLAGS=%CMAKEFLAGS%^
   -DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS%^
   -DKHRONOS=%EXT_DIR%^
   -DBOOST_ROOT=%EXT_DIR%^
   -DCMAKE_EXPORT_COMPILE_COMMANDS=ON^
   -DCMAKE_INSTALL_PREFIX=%BUILDDIR%\WBuild\xilinx\xrt

   echo cmake -B %BUILDDIR%\WBuild -G "Visual Studio 17 2022" !CMAKEFLAGS! %BUILDDIR%\..\src
   cmake -B %BUILDDIR%\WBuild -G "Visual Studio 17 2022" !CMAKEFLAGS! %BUILDDIR%\..\src
)

if [%DEBUG%] == [1] (
   echo cmake --build %BUILDDIR%\WBuild --config Debug --target install --verbose
   cmake --build %BUILDDIR%\WBuild --config Debug --target install --verbose
)

if [%RELEASE%] == [1] (
   echo cmake --build %BUILDDIR%\WBuild --config Release --target install --verbose
   cmake --build %BUILDDIR%\WBuild --config Release --target install --verbose
)

if [%SDK%] == [1] (
   echo Create SDK NSIS Installer ...
   PUSHD %BUILDDIR%\WBuild
   cpack -G NSIS -C Release;Debug
)

exit /b 0

REM --------------------------------------------------------------------------
:Help
ECHO.
ECHO Usage: build22.bat [options]
ECHO.
ECHO [-help]                    - List this help
ECHO [-clean]                   - Remove build directories
ECHO [-dbg]                     - Creates a debug build (default)
ECHO [-opt]                     - Creates a release build (default)
ECHO [-nocmake]                 - Do not reconfigure the project (implies -noinit)
ECHO [-nobuild]                 - Do not build the project
ECHO [-noinit]                  - Do not initialize submodules
ECHO [-noabi]                   - Do not compile with ABI version check (make incremental builds faster)
ECHO [-sdk]                     - Create NSIS XRT SDK Installer for NPU (requires NSIS installed).
ECHO [-alveo]                   - Build Alveo component of XRT (deployment and development)
ECHO [-npu]                     - Build NPU component of XRT (deployment and development)
ECHO [-hip]                     - Enable hip library build
