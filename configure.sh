#!/usr/bin/env bash

to_lower() {
    awk '{print tolower($0)}' <<< $1
}

USE_CLANG=n
USE_LLVM_TOOLS=n
CLANG_CC=clang
CLANG_CXX=clang++
CLANG_OVERRIDES=clang-overrides.cmake
BUILD_FOLDER=build
BUILD_SUFFIX=
while [[ "$1" =~ ^- && ! "$1" == "--" ]] ; do
	case $1 in
		--use-clang) shift ; USE_CLANG=y ;;
		--use-llvm-tools) shift ; USE_LLVM_TOOLS=y ;;
		--clang-overrides) shift ; CLANG_OVERRIDES=$1 ; shift ;;
		--clang-cc) shift ; CLANG_CC=$1 ; shift ;;
		--clang-cxx) shift ; CLANG_CXX=$1 ; shift ;;
        --build-folder) shift ; BUILD_FOLDER=$1 ; shift ;;
        --build-suffix) shift ; BUILD_SUFFIX=$1 ; shift ;;

		* | --help)
			echo "Usage: $0 [--use-clang] [--use-llvm-tools] [--clang-overrides <file>] [--clang-cc <cmd>] [--clang-cxx <cmd>] [--build-folder <path>] [--build-suffix <name>] [--help]"
		    exit 2
		;;
	esac;
done

if [[ "$1" == "--" ]] ; then
    shift
fi

ADD_CMAKE_OPTIONS=
CLANG_OPTIONS=
if [[ `to_lower ${USE_CLANG}` = y ]] ; then
	export CC=${CLANG_CC} CXX=${CLANG_CXX}
	CLANG_OPTIONS="${CLANG_OPTIONS} -DCMAKE_USER_MAKE_RULES_OVERRIDE=${CLANG_OVERRIDES}"
fi
if [[ `to_lower ${USE_LLVM_TOOLS}` = y ]] ; then
	CLANG_OPTIONS="${CLANG_OPTIONS} -DUSE_LLVM_TOOLS=ON"
fi
ADD_CMAKE_OPTIONS="${ADD_CMAKE_OPTIONS} ${CLANG_OPTIONS}"

BUILD_DST=
if [[ -z $BUILD_SUFFIX ]] ; then
    BUILD_DST=$BUILD_FOLDER
else
    BUILD_DST=$BUILD_FOLDER-$BUILD_SUFFIX
fi

cmake -S . -B $BUILD_DST \
	$ADD_CMAKE_OPTIONS \
	-DCMAKE_MAKE_PROGRAM=make \
	-DUSE_VCPKG=TRUE \
	-DVCPKG_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake "$@"