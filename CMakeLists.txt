cmake_minimum_required(VERSION 3.10)

# project configuration
project(SimpleZip)
set(CMAKE_CXX_STANDARD 17)

# simplezip options
option(SZ_ENABLE_FREAD off)

# simplezip library
add_library(sz "")
set(SZ_PUBLIC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/sz")
target_sources(sz
	PRIVATE
	"${CMAKE_SOURCE_DIR}/crc/crc32.hpp"
	"${CMAKE_SOURCE_DIR}/crc/crc32.cpp"
	"${CMAKE_SOURCE_DIR}/util/bit_util.hpp"
	"${CMAKE_SOURCE_DIR}/util/byte_util.hpp"
	"${CMAKE_SOURCE_DIR}/wrapper/constants.hpp"
	"${CMAKE_SOURCE_DIR}/wrapper/file_entry.cpp"
	"${CMAKE_SOURCE_DIR}/wrapper/version.hpp"
	"${CMAKE_SOURCE_DIR}/wrapper/zipper.cpp"
	"${CMAKE_SOURCE_DIR}/compress/algo_store.hpp"
	"${CMAKE_SOURCE_DIR}/compress/algo_deflate.hpp"
	"${CMAKE_SOURCE_DIR}/compress/deflate.cpp"

	"${SZ_PUBLIC_INCLUDE_DIR}/compressor.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/fs.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/file_entry.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/log.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/sz.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/types.hpp"
	"${SZ_PUBLIC_INCLUDE_DIR}/zipper.hpp"
)
target_include_directories(sz
	PRIVATE
	"${PROJECT_SOURCE_DIR}"
	PUBLIC
	"${PROJECT_SOURCE_DIR}/include"
)
if (SZ_ENABLE_FREAD)
	target_compile_definitions(sz PRIVATE "READ_USE_FREAD")
endif()

# simplezip application
add_executable(app "app/main.cpp")
target_link_libraries(
	app
	sz
)

