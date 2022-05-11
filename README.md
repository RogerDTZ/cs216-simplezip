<h1>SimpleZip</h1>

- [1. Introduction](#1-introduction)
- [2. Frontend Application](#2-frontend-application)
  - [2.1. Example](#21-example)
- [3. Backend Library Implementation](#3-backend-library-implementation)
  - [3.1. Zip Format Wrapper](#31-zip-format-wrapper)
  - [3.2. Byte and Bit Utilities](#32-byte-and-bit-utilities)
  - [3.3. Compressor](#33-compressor)
    - [3.3.1. Store Compressor](#331-store-compressor)
    - [3.3.2. Deflate Compressor](#332-deflate-compressor)
    - [3.3.3. Huffman Tree within Maximum Code Length](#333-huffman-tree-within-maximum-code-length)
    - [3.3.4. LZ77 Dictionary](#334-lz77-dictionary)
  - [3.4. Unit Test](#34-unit-test)
- [4. Evaluation](#4-evaluation)
- [5. Highlights, Improvement, and Summary](#5-highlights-improvement-and-summary)

# 1. Introduction

The zip compressor, *SimpleZip*, is implemented in C++ and provides basic compressing functionalities.
* It supports 3 compression methods: store, deflate with static Huffman encoding, and deflate with dynamic Huffman encoding.
* User can specify the level of LZ77 algorithm to control the trade-off between compression rate and compression speed.
* It is also capable of compressing multiple files and directories.
* The compressor supports multiple work threads and speeds up the compression process.

*SimpleZip* comprises two parts: the backend library, and the frontend application. The project is built by CMake build system with GoogleTest integrated, so that the targets, table generators, and unit tests can be managed conveniently.

# 2. Frontend Application

The application uses CLI11[^1] library to parse command line arguments. The usage is as follows:
```
SimpleZip
Usage: simplezip [OPTIONS] target source...

Positionals:
  target TEXT REQUIRED        The filename of the result.
  source TEXT ... REQUIRED    The source file(s) to be compressed.

Options:
  -h,--help                   Print this help message and exit
  -v,--verbose                Verbose mode
  -m,--method TEXT            store | deflate (default: deflate)
  --deflate_static            Use static encoding (for deflate)
  -l,--level INT:INT in [0 - 3]
                              Level of LZ77 (0..3), default: 1
  -t,--thread UINT            number of threads used (for deflate)

```

By enable the verbose mode, detail information is printed.

The compression method can be specified. The deflate method uses dynamic encoding as default. To use static encoding, add option `--deflate_static`.

The level of LZ77 algorithm can be specified. A higher level means higher compression rate and higher time cost. The default level is 1.

The thread number can be specified. The default value is the number of threads of hardware CPU.

## 2.1. Example

**single file**
![](images/example_singlefile.jpg)

**multiple files with directories**
![](images/example_mutliplefiles.jpg)
![](images/example_mutliplefiles_2.jpg)

The results can all be successfully extracted.

# 3. Backend Library Implementation

## 3.1. Zip Format Wrapper

By learning the format of zip file, it can be known that a zip file is consisted of
* file entries
  * [local file header 1]
  * [file data 1]
  * [local file header 2]
  * [file data 2]
  * ...
  * [local file header n]
  * [file data n]
* central directory
  * [file header 1]
  * [file header 2]
  * ...
  * [file header n]
* end of central directory

From the overall structure, two classes can be extracted from the perspective of OOP: the file entry class, and the zip manager class.

A `FileEntry` class manages a file entity. It reads, manages, and compresses the raw data of a single file. `FileEntry` is capable of:
* get the last modified time of the file
* calculate the CRC-32 of the raw content
* compress the file with the specified method
* export the local file header byte stream, file data byte stream, and file header byte stream

CRC-32 is accelerated by a pre-calculated extension table, so that the calculation is 8-times faster than the brute force.
```c++
CRC32Value extend(CRC32Value init_val, const Byte* data, size_t n) {
  const Byte* p = data;
  const Byte* q = data + n;

  CRC32Value crc = init_val ^ CRC32InitXor;
  while (p != q) {
    crc = (crc >> 8) ^ CRC32ByteExtTable[(crc & 0xFF) ^ *p++];
  }
  return crc ^ CRC32InitXor;
}
```

A `Zipper` class manages the construction of a zip file. It accepts multiple `FileEntry` instances as the file components. `Zipper` is capable of:
* register a `FileEntry` instance
* export the zip file consisting of all registered file entries.

The implementation of file entry registration naturally enables *SimpleZip* to support compression of multiple files.

## 3.2. Byte and Bit Utilities

A byte stream is simply stored by `std::vector<Byte>`. Several utility functions are provided to marshal integer (8, 16, or 32 bits) or string into a byte stream.

To manage bit stream is much harder. Therefore, A `BitStream` class is presented to help construct a bit stream. It has the following features:
* dynamically expanding capacity
* append several bits in little endian or big endian
* append another bit stream
* export the bit stream to byte stream

Appending bits to bit stream may call for a bit reverse operation due to endian specification. A build option `SZ_USE_REVERSEBIT_TABLE` controls whether to use a pre-calculated static reverse table to accelerate the process. Unfortunately, the benefit is minute considering the data scale is usually not big enough to embody its advantage.

## 3.3. Compressor

Each compression method corresponds to a compressor class. For better extensibility, all compressors extend from a virtual base class `Compressor`
```c++
class Compressor {
 public:
  Compressor();
  Compressor(const Compressor&) = delete;
  Compressor& operator=(const Compressor&) = delete;
  Compressor(Compressor&&) = delete;
  Compressor& operator=(Compressor&&) = delete;
  virtual ~Compressor();

  // Feed the content to be compressed.
  virtual void feed(const Byte* data, size_t n);

  // Compress the fed content.
  // Return the length of compressed content.
  virtual size_t compress() = 0;

  [[nodiscard]] virtual size_t get_length_compressed() const = 0;

  // Write the compressed content to dst.
  // Call compress() first if the compression has not been called.
  virtual void write_result(Byte* dst) = 0;

 protected:
  // Source content to be compressed.
  Byte* m_src;
  // Size of source content.
  size_t m_src_len;
  // Whether the compressed data has been calculated.
  // Unset when fed by new data, set when compress() is called and finished.
  bool m_finish;
};
```
A `FileEntry` class creates a member compressor of specified method. Currently, store and deflate are supported.

### 3.3.1. Store Compressor

For store method, the compressor literally exports the raw content.

### 3.3.2. Deflate Compressor

Deflate compressor executes LZ77 algorithm on the fed content, then encodes the result using static encoding and dynamic encoding. The file data is divided into sections and allocated to multiple work threads. Each work thread's entry function is as follows:
```c++
// The work thread that runs LZ77 on [st, ed) and encodes the result.
auto work_thread = [this](const Byte* st, const Byte* ed,
                          const bool last_section,
                          std::shared_ptr<BitStream>& bs, ProgressBar& bar) {
  bs = std::make_shared<BitStream>((ed - st) << 3);
  // LZ77 dictionary.
  auto dict = std::make_shared<LZ77Dictionary>();
  // The vector that stores LZ77's result. Though the input bytes is actually
  // O(2 * DeflateBlockSize), it is not necessary to reserve that many.
  std::vector<LZ77Item> items;
  items.reserve(DeflateBlockSize);

  for (const Byte *p = st, *q; p < ed; p = q) {
    q = p + DeflateBlockSize <= ed ? p + DeflateBlockSize : ed;
    if (static_cast<size_t>(ed - q) < DeflateBlockSize) {
      q = ed;
    }
    const bool last_block = q == ed && last_section;

    // Run LZ77 to obtain the deflate items.
    items.clear();
    dict->calc(p, q - p, items, bar);
    items.push_back(LZ77Item{LZ77ItemType::eob, DeflateEOBCode});

    // Encode the deflate items into bit stream.
    // The size of the bit stream is then compared to the one of a store
    // block. The better one is adopted.
    auto bs_cps = std::make_shared<BitStream>(items.size() << 3);
    switch (m_coding_type) {
      case DeflateCodingType::static_coding:
        deflate_encode_static_block(bs_cps, items, last_block);
        break;
      case DeflateCodingType::dynamic_coding:
        deflate_encode_dynamic_block(bs_cps, items, last_block);
        break;
    }

    if (bs_cps->get_bytes_size() >= static_cast<size_t>(q - p)) {
      // Use store.
      deflate_encode_store_block(bs, p, q - p, last_block);
    } else {
      // Use static/dynamic coding.
      bs->append(*bs_cps);
    }
  }
};
```

The compressor will decide whether to use the compressed block or store block (in deflate format) after comparing the size.

For static encoding, the code of each LZ77 item is fixed. To accelerate the encoding, a pre-calculated static table is used. The table is generated automatically by the build system.
```c++
// Code | #ExtraBits | ExtraBits
constexpr uint16 DeflateLiteralTable[256][3];
constexpr uint16 DeflateLengthTable[259][3];
constexpr uint16 DeflateDistanceTable[32769][3];
```

For dynamic encoding:
1. Extract all literals, codes of length, and end-of-block, build their Huffman tree (code: 0~285, max code length: 15).
2. Extract all codes of distance, build their Huffman tree (code: 0~29, max code length: 15).
3. Calculate the run length code of the code length representations of above 2 Huffman trees. Extract all possible run length code, build their Huffman tree (code: 0~18, max code length: 7).
4. Encode the block data using these 3 Huffman trees.

According to the specification of deflate, the first two Huffman trees should not be deeper than 15, the third Huffman tree should not be deeper than 7. The traditional Huffman construction implemented by heap can easily break the limitation. **Therefore, we need a Huffman construction algorithm with maximum code length limitation.**

### 3.3.3. Huffman Tree within Maximum Code Length

The Huffman tree construction with maximum code length limitation is implemented by **Package-Merge** algorithm. This algorithm helps to obtain the best encoding solution under the limitation. The time complexity is $O(NL)$, where $N$ is the number of different items, and $L$ is the maximum code length.

First, for each code length, create a list containing all $N$ items, sorted by increasing frequency.

Iterate list $0$ to list $L-1$. For each list, assume its length is $M$, group all two adjacent items into $\lfloor \frac M 2 \rfloor$ groups of size $2$ (leave the biggest one alone if $M$ is odd). These groups are called packages and are added into the next list as normal "item"s with frequency equals to the summation of the original two items. These newly added items should not break the order of increasing frequency, so a merge process resembling Merge Sort is performed.

Pick the first $2(N-1)$ items in the last list. For each of the $N$ original items, its code length in the final Huffman tree equals to the number of its representing items that are contained in the picked items / package.

From the code length sequence, a Huffman tree can be generated.

### 3.3.4. LZ77 Dictionary

A `LZ77Dictionary` is supposed to calculate the LZ77 sequence of the given input byte stream.

The deflate block size is set to 1024 KB, while the dictionary size is set to 32 KB.

The dictionary uses a cycling array to maintain the hash of bytes in scanned window before the cursor (32768 Bytes) as well as the future window ahead of the cursor (258 Bytes, the maximum repeat length in deflate). This enables the dictionary to quickly decide if two byte sequences are not equal.

The minimum repeat length is 3, so the dictionary seeks for every position in the scanned window that has 3 bytes equal to the 3 bytes ahead of the cursor. A full-hash-map function can be used:
```c++
// Return the uint16 hash value of 3 bytes.
// -1 for out of bound.
static uint32 get_hash3b(int a, int b, int c) {
  return (a + 1) * 66049 + (b + 1) * 257 + (c + 1);
}
```
There are totally $257^3=16974593$ possible combinations. For each hash value $h$, use a linked list to record all positions whose following 3 bytes hash into $h$. The linked list stores nodes in forward type, so that a position nearer to the cursor is in front of a farther one.

To find the longest matching, the dictionary scans through the corresponding linked list, and advance the longest matching $m$. For each position, first check if the first $m$ bytes matches (use the cycling hash to quickly filter negative ones). Then, advance the longest matching step by step. The iteration stops when the position is out of the scanned window.

To optimize the matching process, configurations are used. There are 4 levels:
|level|max chain length|good length|nice length|perfect length|
|:-:|:-:|:-:|:-:|:-:|
|0|64|4|8|16|
|1|128|8|16|128|
|2|512|16|128|258|
|3|4096|258|258|258|
where max chain length is the maximum steps walking through the linked list. If the longest matching is more than good or nice length, the max chain length is shrunk to $\frac 14$ or $\frac 1{16}$. If the longest matching reaches perfect length, the iteration immediately stops. A higher level implies higher compression rate and higher time cost.

The default level is 1.

## 3.4. Unit Test

To build tests, enable `SZ_BUILD_TEST` option in CMake.

Tests are written to make sure the `BitStream` class, LZ77 dictionary, and run length coder perform correctly.

![](images/googletest.jpg)

# 4. Evaluation

**test target 1**: `alice_in_wonderland.txt` (148,574 bytes)
|compressor|mode|output size (byte)|size ratio|
|:-:|:-:|:-:|:-:|
| SimpleZip | static, level-0 | 70,621 | 47.53% |
| SimpleZip | static, level-1 | 67,737 | 45.59% |
| SimpleZip | static, level-2 | 67,180 | 45.22% |
| SimpleZip | static, level-3 | 67,178 | 45.22% |
| SimpleZip | dynamic, level-0 | 57,468 | 38.68% |
| SimpleZip | dynamic, level-1 | 55,530 | 37.38% |
| SimpleZip | dynamic, level-2 | 55,112 | 37.09% |
| SimpleZip | dynamic, level-3 | 55,108 | 37.09% |
| 7-zip | fast compression | 56,544 | 38.06% |
| 7-zip | standard compression | 51,681 | 34.78% |
| 7-zip | deflate extreme compression | 51,232 | 34.48% |

**test target 2**: `code.cpp` (3,741,894 bytes), all codes written in my OI career.
|compressor|mode|output size (byte)|size ratio|
|:-:|:-:|:-:|:-:|
| SimpleZip | static, level-0 | 733,462 | 19.60% |
| SimpleZip | static, level-1 | 690,745 | 18.46% |
| SimpleZip | static, level-2 | 673,947 | 18.01% |
| SimpleZip | static, level-3 | 671,855 | 17.95% |
| SimpleZip | dynamic, level-0 | 633,957 | 16.94% |
| SimpleZip | dynamic, level-1 | 601,729 | 16.08% |
| SimpleZip | dynamic, level-2 | 588,157 | 15.71% |
| SimpleZip | dynamic, level-3 | 586,442 | 15.67% |
| 7-zip | fast compression | 605,201 | 16.17% |
| 7-zip | standard compression | 548,822 | 14.66% |
| 7-zip | extreme compression | 543,937 | 14.53% |

**test target 3**: `enwiki9` (1,000,000,000 bytes), the first $10^9$ bytes of the English Wikipedia dump on Mar. 3, 2006.
CPU: AMD Ryzen 7 5800X 8-Core Processor 3.80 GHz
|compressor|mode|thread used|output size (byte)|size ratio|time used (sec)|speed (MB/s)|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| SimpleZip | dynamic level-0 | 1 | 343,140,928 | 34.31% | 150.4 | 6.341 | 
| SimpleZip | dynamic level-1 | 1 | 335,530,236 | 33.55% | 160.4 | 5.946 | 
| SimpleZip | dynamic level-2 | 1 | 333,880,468 | 33.39% | 171.0 | 5.577 | 
| SimpleZip | dynamic level-3 | 1 | 333,829,437 | 33.38% | 173.6 | 5.496 | 
| SimpleZip | static level-0 | 16 | 408,855,119 | 40.89% | 34.16 | 27.918 | 
| SimpleZip | static level-1 | 16 | 398,682,098 | 39.87% | 36.15 | 26.381 | 
| SimpleZip | static level-2 | 16 | 396,608,624 | 39.66% | 37.61 | 25.357 | 
| SimpleZip | static level-3 | 16 | 396,549,141 | 39.65% | 37.96 | 25.123 | 
| SimpleZip | dynamic level-0 | 16 | 343,140,928 | 34.31% | 34.3 | 27.804 | 
| SimpleZip | dynamic level-1 | 16 | 335,530,236 | 33.55% | 37.0 | 25.775 | 
| SimpleZip | dynamic level-2 | 16 | 333,880,468 | 33.39% | 38.4 | 24.835 | 
| SimpleZip | dynamic level-3 | 16 | 333,829,437 | 33.38% | 38.2 | 24.962 | 
| 7-zip | fast compression | 16 | 336,979,190 | 33.70% | 15.8 | 60.359 | 
| 7-zip | standard compression | 16 | 312,712,170 | 31.27% | 77.3 | 12.337 | 
| 7-zip | extreme compression | 16 | 310,706,257 | 31.07% | 412.5 | 2.312 | 

**Conclusion**
* The parallelism of 16 threads speeds up the compression by about 4.3 times.
* The compression rate of dynamic Huffman encoding is significantly higher than the one of static Huffman encoding.
* The compression rate of *SimpleZip* approximates to the ones of 7-zip.
* Compared to 7-zip's fast compression mode, *SimpleZip* is slower but has higher compression rates.
* Compared to 7-zip's standard and extreme compression modes, *SimpleZip*'s all levels are slightly worse in compression rate but significantly faster.


# 5. Highlights, Improvement, and Summary

**What are the highlights of *SimpleZip*?**
* User-friendly and flexible command line interface with help.
* Good-looking progress bar.
* Multiple files and directory structure supported.
* Verbose mode that prints details during compressing.
* Multiple compression methods supported: store / deflate.
* Dynamic Huffman encoding implemented for deflate. User can choose to use static version instead.
* Use Package-Merge algorithm to build the Huffman tree with limited depth.
* Multiple LZ77 levels available (trade-off between compression rate and speed).
* Deflate will automatically choose whether to store blocks directly or use the compressed block.
* The compressor will automatically choose whether to use store instead of deflate.
* Parallelism supported. User can specify the number of threads to be used.
* OOP programming. Well-designed interfaces for better extensibility.
* Use CMake as the build system.
* GoogleTest integrated. Unit tests written for crucial parts.
* Packaged into library for good usability.

**What can be improved?**
* The configuration of LZ77 level: level-2 is somehow slower than level-3 in some testcases.
* The delay matching strategy is not implemented in LZ77. This strategy leads to higher compression rate.
* Better matching strategies needed to be explored.

*SimpleZip* implements basic zip compress functionalities. The total size of source code is 68.8 KB. As a lightweight and elementary zip compressor, the performance is acceptable.

[^1]: https://github.com/CLIUtils/CLI11 CLI11 github repository
