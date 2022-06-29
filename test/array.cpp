#include "shmCpp.hpp"

#include "shmCpp_test.hpp"

#include <unistd.h>
#include <sys/wait.h>

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

    }
    else if (pid == 0) {
        // Child (receiver)

        std::cout << "Receiver launched\n";

        shm::Array<shmTest::arr_type, shmTest::arr_size> mem(shmTest::arr_name);

        while (true) {
            shmTest::arr_type sum {0};

            for (auto i {0}; i < shmTest::arr_size; i++)
                sum += mem.at(i);

            if (sum == shmTest::arr_sum)
                break;
        };

        std::cout << "Data received:\n";
        for (auto i {0}; i < shmTest::arr_size; i++)
            std::cout << mem.at(i) << '\t';
        std::cout << std::endl;

    }
    else {
        // Error

        std::cerr << "Failed to fork\n";
        exit(1);
    }
}
