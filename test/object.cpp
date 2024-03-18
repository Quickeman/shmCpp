#include "shmCpp.hpp"

#include "shmCpp_test.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <thread>

void setObjZ(shmTest::obj_type& o) {
    o.z = shmTest::obj_value.z;
}

bool getObjZ(const shmTest::obj_type& o) {
    return o.z;
}

int main() {
    const auto pid {fork()};

    if (pid > 0) {
        // Parent (sender)

        shm::Object<shmTest::obj_type> mem(shmTest::obj_name);

        std::cout << "Sender launched\n";

        mem = shmTest::obj_value;
        mem->x = shmTest::obj_value.x;
        mem.get().y = shmTest::obj_value.y;
        setObjZ(mem);

        std::cout << "Data sent\n";

        waitpid(pid, nullptr, 0);

        // Check data was not changed by read-only receiver
        if (mem->x != shmTest::obj_value.x)
            throw std::runtime_error("Object was changed by read-only mapping");

    }
    else if (pid == 0) {
        // Child (receiver)

        std::cout << "Receiver launched\n";

        shm::Object<shmTest::obj_type> mem(shmTest::obj_name, shm::Permissions::ReadOnly);

        while (true) {
            if ((mem->x == shmTest::obj_value.x)
                && (mem.get().y == shmTest::obj_value.y)
                && (getObjZ(mem) == shmTest::obj_value.z)
            ) {
                break;
            }
        };

        std::cout << "Data received:\n";
        std::cout << "x: " << mem->x << '\t';
        std::cout << "y: " << mem.get().y << '\t';
        std::cout << "z: " << std::boolalpha << getObjZ(mem) << '\n';

        // Test for read-only-ness
        mem->x = ~mem->x;
        // Ensure enough time has passed for data carry-through
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    }
    else {
        // Error

        std::cerr << "Failed to fork\n";
        exit(1);
    }
}
