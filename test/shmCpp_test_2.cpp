#include "shmCpp.hpp"

#include "shmCpp_test.hpp"

#include <thread>

int main() {
    std::cout << "Receiver launched\n";

    shm::SharedMemory<shmTest::type> mem(shmTest::name, shmTest::size);

    while (true) {
        shmTest::type sum {0};

        for (auto i {0}; i < shmTest::size; i++)
            sum += mem.at(i);

        if (sum == shmTest::sum)
            break;
    };

    std::cout << "Data received:\n";
    for (auto i {0}; i < shmTest::size; i++)
        std::cout << mem.at(i) << '\t';
    std::cout << std::endl;
}
