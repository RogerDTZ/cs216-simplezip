cmake_minimum_required(VERSION 3.10)

# Project configuration
project(SimpleZip)
set(CMAKE_CXX_STANDARD 17)

# Third party build
add_subdirectory(third_party/googletest)

# Options
option(SZ_ENABLE_FREAD_FWRITE "Use fread/fwrite when reading/writing file.")
option(SZ_USE_REVERSEBIT_TABLE "Use reverse bit table.")
option(SZ_BUILD_TEST "Build tests.")

# Generate tables
# reverse bits table
if (SZ_USE_REVERSEBIT_TABLE)
	add_executable(gt_reversebits "")
	target_sources(gt_reversebits 
		PRIVATE 
		"${CMAKE_CURRENT_SOURCE_DIR}/util/gt_reversebits.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/util/bit_util.hpp"
	)
	target_include_directories(gt_reversebits
		PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	target_compile_definitions(gt_reversebits PRIVATE "SZ_USE_REVERSEBIT_TABLE")
	set(TABLE_REVERSEBITS "${CMAKE_CURRENT_BINARY_DIR}/include/util/table_reversebits.hpp")
	add_custom_command(
		OUTPUT ${TABLE_REVERSEBITS}
		COMMAND gt_reversebits ${TABLE_REVERSEBITS}
		DEPENDS gt_reversebits
	)
endif()
# deflate tables
add_executable(gt_deflate "")
target_sources(gt_deflate PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compress/gt_deflate.cpp")
set(TABLE_DEFLATE "${CMAKE_CURRENT_BINARY_DIR}/include/compress/table_deflate.hpp")
add_custom_command(
	OUTPUT ${TABLE_DEFLATE}
	COMMAND gt_deflate ${TABLE_DEFLATE}
	DEPENDS gt_deflate
)

# Library build
add_library(sz "")
set(SZ_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/sz")
target_sources(sz
	PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/crc/crc32.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/crc/crc32.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/util/bit_util.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/util/bit_util.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/util/byte_util.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/wrapper/constants.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/wrapper/file_entry.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/wrapper/version.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/wrapper/zipper.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/compress/algo_store.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/compress/algo_deflate.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/compress/deflate.cpp"

	${TABLE_DEFLATE}
	
	"${SZ_PUBLIC_INCLUDE_DIR}/compressor.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/fs.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/file_entry.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/log.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/sz.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/types.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/zipper.hpp"
)

if (SZ_USE_REVERSEBIT_TABLE)
	target_sources(sz
		PRIVATE
		${TABLE_REVERSEBITS}
	)
endif()

target_include_directories(sz
	PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_BINARY_DIR}/include"
	PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if (SZ_ENABLE_FREAD_FWRITE)
	target_compile_definitions(sz PRIVATE "SZ_IO_USE_FREAD_FWRITE")
endif()

if (SZ_USE_REVERSEBIT_TABLE)
	target_compile_definitions(sz PRIVATE "SZ_USE_REVERSEBIT_TABLE")
endif()

# Application build
add_executable(app "app/main.cpp")
target_link_libraries(
	app
	sz
)

# Tests
if (SZ_BUILD_TEST)
	enable_testing()

	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

	target_compile_definitions(sz PUBLIC "BUILD_TEST")

	add_executable(sz_tests "")
	target_sources(sz_tests
		PRIVATE
		"tests/test_entry.cpp"
		"tests/bitflow_test.cpp"
	)
	target_link_libraries(sz_tests sz gtest gtest_main)
	target_include_directories(sz_tests 
		PUBLIC 
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	add_test(NAME "sz_tests" COMMAND "sz_tests")
endif()

