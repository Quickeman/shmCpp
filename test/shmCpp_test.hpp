#ifndef SHM_CPP_TEST_H
#define SHM_CPP_TEST_H

#include "shmCpp.hpp"

#include <array>
#include <numeric>

namespace shmTest {

using type = int;

const std::string name {shm::formatName("ShmCpp_Test_Mem")};

static constexpr size_t size {8};

static constexpr size_t size_bytes {size * sizeof(type)};

static constexpr std::array<type, size> seq {4, 8, 6286, 2, 264};

static const auto sum {std::accumulate(seq.begin(), seq.end(), 0)};

} // namespace shm

#endif