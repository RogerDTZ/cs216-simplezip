cmake_minimum_required(VERSION 3.10)


# Project
project(simplezip VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)


# Options
option(SZ_ENABLE_FREAD_FWRITE "Use fread/fwrite when reading/writing file.")
option(SZ_USE_REVERSEBIT_TABLE "Use reverse bit table.")
option(SZ_BUILD_APP "Build application.")
option(SZ_BUILD_TEST "Build tests.")

set(SZ_ENABLE_FREAD_FWRITE ON CACHE BOOL "" FORCE)
set(SZ_BUILD_APP ON CACHE BOOL "" FORCE)


# Source files
set(SZ_PUBLIC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/sz")

set(SZ_LIBSRC_INCLUDE
	"${SZ_PUBLIC_INCLUDE_DIR}/common.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/file_entry.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/log.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/sz.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/types.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/zipper.hpp"
)

set(SZ_LIBSRC_CRC
	"${CMAKE_SOURCE_DIR}/crc/crc32.hpp"
	"${CMAKE_SOURCE_DIR}/crc/crc32.cpp"
)
set(SZ_LIBSRC_UTIL
	"${CMAKE_SOURCE_DIR}/util/bit_util.hpp"
	"${CMAKE_SOURCE_DIR}/util/bit_util.cpp"
	"${CMAKE_SOURCE_DIR}/util/byte_util.hpp"
	"${CMAKE_SOURCE_DIR}/util/fs.hpp"
	"${CMAKE_SOURCE_DIR}/util/progress_bar.hpp"
)
set(SZ_LIBSRC_WRAPPER
	"${CMAKE_SOURCE_DIR}/wrapper/constants.hpp"
	"${CMAKE_SOURCE_DIR}/wrapper/file_entry.cpp"
	"${CMAKE_SOURCE_DIR}/wrapper/version.hpp"
	"${CMAKE_SOURCE_DIR}/wrapper/zipper.cpp"
)
set(SZ_LIBSRC_COMPRESS
	"${CMAKE_SOURCE_DIR}/compress/compressor.hpp"
	"${CMAKE_SOURCE_DIR}/compress/cps_store.hpp"
	"${CMAKE_SOURCE_DIR}/compress/cps_deflate.hpp"
	"${CMAKE_SOURCE_DIR}/compress/deflate.cpp"
	"${CMAKE_SOURCE_DIR}/compress/deflate_huffman.cpp"
	"${CMAKE_SOURCE_DIR}/compress/lz77_dictionary.cpp"
)

set(SZ_TABLE_DEFLATE "${CMAKE_BINARY_DIR}/include/compress/table_deflate.hpp")
set(SZ_LIBSRC_TABLE
	${SZ_TABLE_DEFLATE}
)
if (SZ_USE_REVERSEBIT_TABLE)
	set(SZ_TABLE_REVERSEBITS "${CMAKE_BINARY_DIR}/include/util/table_reversebits.hpp")
	set(SZ_LIBSRC_TABLE
		${SZ_LIBSRC_TABLE}
		${SZ_TABLE_REVERSEBITS}
	)
endif()

source_group(include FILES ${SZ_LIBSRC_INCLUDE})
source_group(crc FILES ${SZ_LIBSRC_CRC})
source_group(util FILES ${SZ_LIBSRC_UTIL})
source_group(wrapper FILES ${SZ_LIBSRC_WRAPPER})
source_group(compress FILES ${SZ_LIBSRC_COMPRESS})
source_group(table FILES ${SZ_LIBSRC_TABLE})


# Generate tables
# reverse bits table
if (SZ_USE_REVERSEBIT_TABLE)
	add_executable(gt_reversebits "")
	target_sources(gt_reversebits 
		PRIVATE 
		"${CMAKE_SOURCE_DIR}/util/gt_reversebits.cpp"
		"${CMAKE_SOURCE_DIR}/util/bit_util.hpp"
	)
	target_include_directories(gt_reversebits
		PRIVATE
		"${CMAKE_SOURCE_DIR}"
		PUBLIC
		"${CMAKE_SOURCE_DIR}/include"
	)
	target_compile_definitions(gt_reversebits PRIVATE "SZ_USE_REVERSEBIT_TABLE")
	add_custom_command(
		OUTPUT ${SZ_TABLE_REVERSEBITS}
		COMMAND gt_reversebits ${SZ_TABLE_REVERSEBITS}
		DEPENDS gt_reversebits
	)
endif()
# deflate tables
add_executable(gt_deflate "")
target_sources(gt_deflate PRIVATE "${CMAKE_SOURCE_DIR}/compress/gt_deflate.cpp")
add_custom_command(
	OUTPUT ${SZ_TABLE_DEFLATE}
	COMMAND gt_deflate ${SZ_TABLE_DEFLATE}
	DEPENDS gt_deflate
)


# Library
add_library(sz "")
target_sources(sz
	PRIVATE
	${SZ_LIBSRC_INCLUDE}
	${SZ_LIBSRC_CRC}
	${SZ_LIBSRC_UTIL}
	${SZ_LIBSRC_WRAPPER}
	${SZ_LIBSRC_COMPRESS}
	${SZ_LIBSRC_TABLE}
)
target_include_directories(sz
	PRIVATE
	"${CMAKE_SOURCE_DIR}"
	"${CMAKE_BINARY_DIR}/include"
	PUBLIC
	"${CMAKE_SOURCE_DIR}/include"
)

if (SZ_ENABLE_FREAD_FWRITE)
	target_compile_definitions(sz PRIVATE "SZ_IO_USE_FREAD_FWRITE")
endif()
if (SZ_USE_REVERSEBIT_TABLE)
	target_compile_definitions(sz PRIVATE "SZ_USE_REVERSEBIT_TABLE")
endif()


# Application
if (SZ_BUILD_APP)
	include(FetchContent)
	FetchContent_Declare(
	  cli11
	  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
	  GIT_TAG        v2.2.0
	)
	FetchContent_MakeAvailable(cli11)
	
	add_executable(app "app/main.cpp")
	set_target_properties(app PROPERTIES OUTPUT_NAME "simplezip")
	target_link_libraries(
		app
		sz
		CLI11::CLI11
	)
endif()


# Tests
if (SZ_BUILD_TEST)
	enable_testing()

	# googletest
	add_subdirectory(third_party/googletest)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

	target_compile_definitions(sz PUBLIC "BUILD_TEST")

	add_executable(sz_tests "")
	target_sources(sz_tests
		PRIVATE
		"tests/test_entry.cpp"
		"tests/bitstream_test.cpp"
		"tests/deflate_test.cpp"
	)
	target_link_libraries(sz_tests sz gtest gtest_main)
	target_include_directories(sz_tests 
		PUBLIC 
		"${CMAKE_SOURCE_DIR}"
		"${CMAKE_SOURCE_DIR}/include"
	)
	add_test(NAME "sz_tests" COMMAND "sz_tests")
endif()


# Install
install(
	TARGETS sz
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)
install(
	DIRECTORY ${SZ_PUBLIC_INCLUDE_DIR} DESTINATION include
)
if (SZ_BUILD_APP)
	install(
		TARGETS app
		RUNTIME DESTINATION bin
	)
endif()

# Package
include(CPack)
