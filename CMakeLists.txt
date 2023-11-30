cmake_minimum_required(VERSION 3.20)

# toolchain setup
set(USE_VCPKG TRUE CACHE BOOL "whether you want to use vcpkg or not")

if(USE_VCPKG)
	set(CMAKE_TOOLCHAIN_FILE ${VCPKG_TOOLCHAIN_FILE} CACHE FILEPATH "path to vcpkg toolchain file")

	# variable from vcpkg.cmake toolchain file
	# this directory will contain all installed <Package>Config.cmake files
	# this is required for find_package()
	set(VCPKG_ALL_INSTALLED_DIR ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
	message(STATUS ${VCPKG_ALL_INSTALLED_DIR})
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${VCPKG_ALL_INSTALLED_DIR})
	message(STATUS ${CMAKE_MODULE_PATH})

	# vcpkg dependencies
	set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${VCPKG_ALL_INSTALLED_DIR})
	message(STATUS ${CMAKE_PREFIX_PATH})
endif()


project(yin_yang)

# setup arch string
if(WIN32)
	set(platform "windows")
elseif(UNIX)
	set(platform "unix")
else()
    message(FATAL_ERROR "unknown platform")
endif()

# dependencies
# opengl
if(WIN32 OR (UNIX AND NOT APPLE))
	find_package(OpenGL REQUIRED)
	set(OPENGL_LIBS OpenGL::GL)
else()
	message(FATAL_ERROR "System unsupported")
endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenAL CONFIG REQUIRED) # yeah, installed by vcpkg or whatever package manager you use
find_package(nlohmann_json CONFIG REQUIRED)

#set(ALL_DEPS "${OPENGL_LIBS};glfw;GLEW::GLEW;EnTT::EnTT;glm::glm;OpenAL::OpenAL;nlohmann_json::nlohmann_json")
set(ALL_DEPS ${OPENGL_LIBS} glfw GLEW::GLEW EnTT::EnTT glm::glm OpenAL::OpenAL nlohmann_json::nlohmann_json)
set(JSON_LIB nlohmann_json::nlohmann_json)

# utility to create neat project filters
# takes one optional parameter - source directory that is current directory (.) by default
# globs all files in a directory and provides filter tree
# TODO : populate filters using name field
# TODO : utility to create project (maybe totally unneccessary)
function(populate_filters)
	if(DEFINED ARGV0)
		set(src_dir ${ARGV0})
	else()
		set(src_dir ".")
	endif()

	#cmake_path(ABSOLUTE_PATH src_dir NORMALIZE)
	#message(STATUS ${src_dir})

	if(MSVC_IDE)
		file(GLOB_RECURSE files LIST_DIRECTORIES false
			${src_dir}/*.cpp ${src_dir}/*.hpp
			${src_dir}/*.c ${src_dir}/*.h)
		#message(STATUS "populate_filters: ${files}")
		source_group(TREE "${src_dir}" FILES ${files})
	endif()
endfunction()


# __VA_OPT__ fix for MSVC
if (MSVC)
	message(STATUS "MSVC compiler detected")
	add_compile_options("/Zc:preprocessor")
endif()

# cpp standart
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# very first include
add_subdirectory(src)

# can you yin_yang
add_subdirectory(tests)