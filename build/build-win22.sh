#!/bin/bash

# Use this script within a bash shell under WSL
# No need to use Visual Studio separate shell.
# Clone workspace must be on /mnt/c

set -e

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
SRCDIR=$(readlink -f $BUILDDIR/../src)

unix2dos()
{
    echo $(sed -e 's|/mnt/\([A-Za-z]\)/\(.*\)|\1:/\2|' <<< $1)
}

CMAKE="/mnt/c/Program Files/CMake/bin/cmake.exe"
CPACK="/mnt/c/Program Files/CMake/bin/cpack.exe"
EXT=${EXT_DIR:-/mnt/c/Xilinx/xrt/ext.new}
BOOST=$EXT
KHRONOS=$EXT

usage()
{
    echo "Usage: build.sh [options]"
    echo
    echo "[-help]                    List this help"
    echo "[clean|-clean]             Remove build directories"
    echo "[-prefix]                  CMAKE_INSTALL_PREFIX (default: $BUILDDIR/WBuild/xilinx/xrt)"
    echo "[-cmake]                   CMAKE executable (default: $CMAKE)"
    echo "[-ext]                     Location of link dependencies (default: $EXT)"
    echo "[-boost]                   BOOST libaries root directory (default: $BOOST)"
    echo "[-nocmake]                 Do not rerun cmake generation, just build"
    echo "[-noinit]                  Do not initialize Git submodules (default with [-nocmake])"
    echo "[-noabi]                   Do compile with ABI version check"
    echo "[-j <n>]                   Compile parallel (default: system cores)"
    echo "[-dbg]                     Build debug library (default)"
    echo "[-opt]                     Build release library (default)"
    echo "[-nobuild]                 Disables build step"
    echo "[-sdk]                     Create NSIS XRT SDK NPU Installer (requires NSIS installed)."

    exit 1
}

clean=0
prefix=$BUILDDIR/WBuild/xilinx/xrt
jcore=`grep -c ^processor /proc/cpuinfo`
nocmake=0
noinit=0
noabi=0
dbg=1
release=1
sdk=0
alveo_build=0
npu_build=0
base_build=0
cmake_flags="-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
generator="Visual Studio 17 2022"

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        clean|-clean)
            clean=1
            shift
            ;;
        -prefix)
            shift
            prefix="$1"
            shift
            ;;
	-cmake)
	    shift
	    CMAKE="$1"
	    shift
	    ;;
	-ext)
	    shift
	    EXT="$1"
            BOOST=$EXT
            KHRONOS=$EXT
	    shift
	    ;;
        -dbg)
            if [[ $dbg == 0 ]]; then
                echo "-dbg and -opt are mutually exclusive, specify one only, or none at all"
                exit 1
            fi
            dbg=1
            release=0
            shift
            ;;
        -opt)
            if [[ $release == 0 ]]; then
                echo "-opt and -dbg are mutually exclusive, specify one only, or none at all"
                exit 1
            fi
            dbg=0
            release=1
            shift
            ;;
        -nobuild)
            if [[ $dbg == 0 || $release == 0 ]]; then
                echo "-nobuild cannot be used with either -opt or -dbg"
                exit 1
            fi
            dbg=0
            release=0
            shift
            ;;
	-boost)
	    shift
	    BOOST="$1"
	    shift
	    ;;
        -hip)
            cmake_flags+= " -DXRT_ENABLE_HIP=ON"
            shift
            ;;
        -base)
            shift
            base_build=1
            cmake_flags+= " -DXRT_BASE=1"
            ;;
        -alveo)
            shift
            alveo_build=1
            cmake_flags+=" -DXRT_ALVEO=1"
            ;;
	-npu)
            shift
	    npu_build=1
	    cmake_flags+=" -DXRT_NPU=1"
            ;;
        -j)
            shift
            jcore=$1
            shift
            ;;
        -nocmake)
            nocmake=1
            noinit=1
            shift
            ;;
        -noinit)
            noinit=1
            shift
            ;;
        -noabi)
            cmake_flags+=" -DDISABLE_ABI_CHECK=1"
            shift
            ;;
        -sdk)
            shift
            dbg=1
            release=1
            sdk=1
            ;;
        *)
            echo "unknown option '$1'"
            usage
            ;;
    esac
done

if [[ $((npu_build + alveo_build + base_build)) > 1 ]]; then
    echo "build.sh: -npu, -alveo, -base are mutually exclusive"
    exit 1
fi

if [[ $clean == 1 ]]; then
    echo "/bin/rm -rf $BUILDDIR/WBuild"
    /bin/rm -rf $BUILDDIR/WBuild
    exit 0
fi

BOOST_DOS=$(unix2dos $BOOST)
KHRONOS_DOS=$(unix2dos $KHRONOS)
SRCDIR_DOS=$(unix2dos $SRCDIR)
BUILDDIR_DOS=$(unix2dos $BUILDDIR/WBuild)
PREFIX_DOS=$(unix2dos $prefix)

if [[ $nocmake == 0 && -e $BUILDDIR/WBuild/CMakeCache.txt ]]; then
    echo "Disabling cmake configuration; using existing build configuration"
    echo "Use -clean to remove existing build configuration"
    nocmake=1
    noinit=1
fi

if [[ $noinit == 0 ]]; then
   echo "Updating Git submodules, use -noinit option to avoid updating"
   git submodule update --init --progress --recursive
fi

if [[ $nocmake == 0 ]]; then
    cmake_flags+=" -DMSVC_PARALLEL_JOBS=$jcore"
    cmake_flags+=" -DKHRONOS=$KHRONOS_DOS"
    cmake_flags+=" -DBOOST_ROOT=$BOOST_DOS"
    cmake_flags+=" -DCMAKE_INSTALL_PREFIX=$PREFIX_DOS"

    echo "$CMAKE -G $generator -B $BUILDDIR_DOS $cmake_flags $SRCDIR_DOS"
    "$CMAKE" -G "$generator" -B $BUILDDIR_DOS $cmake_flags $SRCDIR_DOS
fi

if [[ $dbg == 1 ]]; then
    "$CMAKE" --build $BUILDDIR_DOS --config Debug --target install --verbose
fi

if [[ $release == 1 ]]; then
    "$CMAKE" --build $BUILDDIR_DOS --config Release --target install --verbose
fi

if [[ $sdk == 1 ]]; then
    echo "Creating SDK NSIS installer ..."
    here=$PWD
    cd $BUILDDIR/WBuild
    "$CPACK" -G NSIS -C "Release;Debug"
    cd $here
fi
