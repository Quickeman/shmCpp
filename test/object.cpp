#include "shmCpp.hpp"

#include "shmCpp_test.hpp"

#include <unistd.h>
#include <sys/wait.h>

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

    }
    else if (pid == 0) {
        // Child (receiver)

        std::cout << "Receiver launched\n";

        shm::Object<shmTest::obj_type> mem(shmTest::obj_name, shm::Permission::ReadOnly);

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

    }
    else {
        // Error

        std::cerr << "Failed to fork\n";
        exit(1);
    }
}
