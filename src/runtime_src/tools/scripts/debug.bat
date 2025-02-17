@echo off

REM SPDX-License-Identifier: Apache-2.0
REM Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

REM Script for debug environment for XRT
set XILINX_XRT_DEBUG=%~dp0

set PATH=%XILINX_XRT_DEBUG%;%PATH%

echo XILINX_XRT_DEBUG : %XILINX_XRT_DEBUG%
echo PATH             : %PATH%
