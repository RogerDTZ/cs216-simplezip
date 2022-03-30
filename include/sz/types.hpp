#pragma once

#include <cstdint>

namespace sz {

// clang-format off

using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;

using Byte           = uint8;

// Version needed to extract (minimum)
using OptVersion     = uint16;
// General purpose bit flag
using GeneralPurpose = uint16;

// Compression method
enum class CompressionMethod : uint16 {
  none    = 0x0000,
  deflate = 0x0008
};

// File last modification time & date
using Timestamp = struct {
  uint16 time;
  uint16 date;
};

// 32-bit CRC32 value
using CRC32Value     = uint32;

// 4-byte integer used to indicate size
using SizeType       = uint32;
// 2-byte integer used to indicate length
using LengthType     = uint16;

// Disk number where file starts
using DiskNumber     = uint16;

// 2-byte Internal file attributes
using InternalAttr   = uint16;
// 4-byte External file attributes
using ExternalAttr   = uint32;

// 4-byte Offset type
using Offset         = uint32;

// 2-byte Number of records
using NRecord        = uint16;

// clang-format on

}  // namespace sz
