#include "shmCpp.hpp"

#include "shmCpp_test.hpp"

#include <thread>
#include <unistd.h>

int main() {
    shm::SharedMemory<shmTest::type> mem(shmTest::name, shmTest::size);

    const auto pid {fork()};
    if (pid > 0) {
        // Parent (this)
    }
    else if (pid == 0) {
        // Child (other)
        char* args[] = {"shmCpp_test_2", NULL};
        const auto err {execv("./shmCpp_test_2", args)};
        std::cerr << "Failed to launch receiver\n";
        exit(1);
    }
    else {
        // Error
        std::cerr << "Failed to fork sender\n";
        exit(1);
    }

    std::cout << "Sender launched\n";

    for (auto i {0}; i < shmTest::size; i++) {
        mem.at(i) = shmTest::seq.at(i);
    }

    std::cout << "Data sent\n";

    std::this_thread::sleep_for(std::chrono::seconds(5));
}
