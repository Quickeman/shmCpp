#ifndef SHM_CPP_TEST_H
#define SHM_CPP_TEST_H

#include "shmCpp.hpp"

#include <array>
#include <numeric>

namespace shmTest {

// Object testing
using obj_type = struct {
    int x;
    float y;
    bool z;
};

const std::string obj_name {shm::formatName("ShmCpp_Test_Object")};

static constexpr size_t obj_size_bytes {sizeof(obj_type)};

static constexpr obj_type obj_value {84314, 0.214984561, true};


// Array testing
using arr_type = int;

const std::string arr_name {shm::formatName("ShmCpp_Test_Array")};

static constexpr size_t arr_size {8};

static constexpr size_t arr_size_bytes {arr_size * sizeof(arr_type)};

static constexpr std::array<arr_type, arr_size> arr_seq {4, 8, 6286, 2, 264};

static const auto arr_sum {std::accumulate(arr_seq.begin(), arr_seq.end(), 0)};

} // namespace shm

#endif