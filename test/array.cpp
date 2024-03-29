#include "shmCpp.hpp"

#include "shmCpp_test.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <thread>

int main() {
    const auto pid {fork()};

    if (pid > 0) {
        // Parent (sender)

        shm::Array<shmTest::arr_type, shmTest::arr_size> mem(shmTest::arr_name);

        std::cout << "Sender launched\n";

        for (auto i {0}; i < shmTest::arr_size; i++) {
            mem.at(i) = shmTest::arr_seq.at(i);
        }

        std::cout << "Data sent\n";

        waitpid(pid, nullptr, 0);

        // Check data was not changed by read-only receiver
        if (mem[1] != shmTest::arr_seq[1])
            throw std::runtime_error("Array was changed by read-only mapping");

    }
    else if (pid == 0) {
        // Child (receiver)

        std::cout << "Receiver launched\n";

        shm::Array<shmTest::arr_type, shmTest::arr_size> mem(shmTest::arr_name, shm::Permissions::ReadOnly);

        while (true) {
            shmTest::arr_type sum {0};

            for (auto i {0}; i < shmTest::arr_size; i++)
                sum += mem.at(i);

            if (sum == shmTest::arr_sum)
                break;
        };

        std::cout << "Data received:\n";
        for (const auto& el : mem)
            std::cout << el << '\t';
        std::cout << std::endl;

        // Test for read-only-ness
        mem[1] = ~mem[1];
        // Ensure enough time has passed for data carry-through
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    }
    else {
        // Error

        std::cerr << "Failed to fork\n";
        exit(1);
    }
}
