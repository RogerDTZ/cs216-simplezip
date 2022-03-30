#pragma once

#include "sz/types.hpp"

namespace sz {

constexpr uint32 local_file_header_signature = 0x04034B50;
constexpr uint32 central_directory_file_header_signature = 0x02014b50;
constexpr uint32 endof_central_directory_file_header_signature = 0x06054b50;

}  // namespace sz
