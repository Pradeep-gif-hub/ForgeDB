#pragma once

#include <cstddef>
#include <cstdint>

namespace forgedb {

inline constexpr size_t kPageSize = 4096;
inline constexpr size_t kDefaultBufferPoolSize = 64;
inline constexpr uint32_t kMagicNumber = 0x46474442;  // "FGDB" in ASCII
inline constexpr uint32_t kDatabaseVersion = 1;

}  // namespace forgedb
