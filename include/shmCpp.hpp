#ifndef SHM_CPP_H
#define SHM_CPP_H

#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>

#include <string>
#include <stdexcept>
#include <iostream>

/** Namespace encapsulating the shmCpp library. */
namespace shm {

// Errors

/** Errors concerning file access, permissions, etc. */
class FileError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

/** Errors concerning memory mapping. */
class MemoryError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};


/** Shared memory base class.
 * @param Sz The number of bytes to store in the shared memory. Must be
 * greater than zero. */
template<size_t Sz>
class _SharedMemory {
public:
    // Assert size is acceptable is ok
    static_assert(Sz > 0, "Cannot create shared memory object for type with size 0");

    /** Constructor.
     * Opens the SMO, creating it if it does not already exist, and resizes it
     * to @a size.
     * @param name The name/identifier of the POSIX shared memory object.
     * @param size The number of bytes to store in the shared memory. Must be
     * greater than zero.
     * @note It is advised to use @ref formatName on the name used.
     * @note If the SMO already exists and its size is larger than that
     * specified, the data past the end of the new length in the existing SMO
     * will be lost or corrupted. */
    _SharedMemory(const std::string& name);

    virtual ~_SharedMemory();

    /** @returns the name used to open the shared memory. */
    const std::string& name() const noexcept;

    /** Checks if the memory is mapped. */
    bool empty() const noexcept;

private:
    /** Opens a SMO.
     * Writes to @ref fd. */
    void open();

    /** Closes the underlying file without unlinking the memory object. */
    void close();

    /** Unlinks the underlying shared memory object file.
     * The memory can continue to be used by objects with an open mapping to it,
     * but it cannot be opened by new objects. */
    void unlink();

    /** Maps the shared memory to @ref _data. */
    void map();

    /** Unmaps the shared memory from @ref _data. */
    void unmap();

protected:
    /** Mapped data. */
    void* _data;

private:
    /** Shared memory object's name. */
    const std::string _name;

    /** File descriptor for the SMO. */
    int fd;
};


/** Class for creating and manipulating a POSIX shared memory object (SMO). */
template<class Tp>
class Object : public _SharedMemory<sizeof(Tp)> {
public:
    /** Constructor.
     * Opens the SMO, creating it if it does not already exist.
     * @param name The name/identifier of the POSIX shared memory object.
     * @note It is advised to use @ref formatName on the name used.
     * @note If the SMO already exists and the types of this and the other
     * `shm::Object` are not the same size, the data may be corrupted. */
    Object(const std::string& name);

    ~Object() = default;

    /** Object access. */
    operator Tp&() noexcept;
    operator Tp&() const noexcept;
    Tp* operator->() noexcept;
    const Tp* operator->() const noexcept;
    Tp& get() noexcept;
    const Tp& get() const noexcept;

    /** Direct access to the mapped memory. */
    Tp* data() noexcept;
};


/** Class for creating and manipulating a POSIX shared memory object (SMO) array. */
template<class Tp, size_t Sz>
class Array : public _SharedMemory<sizeof(Tp) * Sz> {
public:
    /** Constructor.
     * Opens the SMO, creating it if it does not already exist, and resizes it
     * to @a size.
     * @param name The name/identifier of the POSIX shared memory object.
     * @note It is advised to use @ref formatName on the name used.
     * @note If the SMO already exists and its size is larger than that
     * specified, the data past the end of the new length in the existing SMO
     * will be lost. */
    Array(const std::string& name);

    ~Array() = default;

    /** Element access. */
    Tp& operator[](size_t n) noexcept;
    const Tp& operator[](size_t n) const noexcept;

    /** Bounds-checked element access. */
    Tp& at(size_t n);
    const Tp& at(size_t n) const;

    /** @returns @ref Sz; the number of @ref Tp objects in the Array. */
    constexpr size_t size() const noexcept { return Sz; }

    /** Direct access to the mapped memory. */
    Tp* data() noexcept;
};

/** Tests whether a SMO called @a name exists.
 * @note This test will also fail if the memory fails to open for reasons such
 * as process- or system-wide limits on file openings being reached. */
static bool exists(const std::string& name);

/** Formats the identifier name according to the naming conventions outlined at
 * https://www.man7.org/linux/man-pages/man3/shm_open.3.html#DESCRIPTION. */
static std::string formatName(const std::string& name);

} // namespace shm

// END OF API HEADER
////////////////////////////////////////////////////////////////////////////////
// START OF IMPLEMENTATION

namespace shm {

// class _SharedMemory

template<size_t Sz>
_SharedMemory<Sz>::_SharedMemory(const std::string& nm):
_data{nullptr}, _name{nm}, fd{-1}
{
    this->open();
    this->map();
    this->close();
}

template<size_t Sz>
_SharedMemory<Sz>::~_SharedMemory() {
    this->unmap();
    this->unlink();
}

template<size_t Sz>
const std::string& _SharedMemory<Sz>::name() const noexcept {
    return this->_name;
}

template<size_t Sz>
bool _SharedMemory<Sz>::empty() const noexcept {
    return this->_data != nullptr;
}

template<size_t Sz>
void _SharedMemory<Sz>::open() {
    const auto oflag {O_RDWR | O_CREAT};
    const mode_t mode {S_IRWXU | S_IRGRP};

    this->fd = shm_open(this->_name.c_str(), oflag, mode);

    if (this->fd == -1) {
        // error handling
        std::string msg {"Shared memory: could not open " + this->_name};

        switch (errno) {
            case EACCES:
                msg.append(": permission denied");
            break;
            case EINVAL:
                msg.append(": invalid name");
            break;
            case EMFILE:
            case ENFILE:
                msg.append(": too many files open");
            break;
            case ENAMETOOLONG:
                msg.append(": filename too long");
            break;
            default:
                msg.append(": error code " + std::to_string(errno));
            break;
        }

        throw FileError(msg);
    }

    const auto err {ftruncate(this->fd, Sz)};

    if (err == -1) {
        // error handling
        std::string msg {
            "Shared memory: could not create shared memory object " + this->_name +
            " of size " + std::to_string(Sz) + " bytes"
        };

        switch (errno) {
            case EFBIG:
                msg.append(": larger than maximum file size");
            break;
            case EPERM:
                msg.append(": permission denied");
            break;
            case EINTR:
                msg.append(": interrupted by signal");
            break;
            case EBADF:
            case EINVAL:
                msg.append(": internal error");
            break;
            default:
                msg.append(": error code " + std::to_string(errno));
            break;
        }

        throw FileError(msg);
    }
}

template<size_t Sz>
void _SharedMemory<Sz>::close() {
    if (this->fd != -1) {
        // Only close the file if it is open
        const auto err {::close(this->fd)};

        if (err == -1) {
            // error handling
            std::string msg {
                "Shared memory: error when closing file " + this->_name +
                " with descriptor " + std::to_string(this->fd)
            };

            switch (errno) {
                case EBADF:
                    msg.append(": internal error");
                break;
                case EINTR:
                    msg.append(": interrupted by signal");
                break;
                default:
                    msg.append(" error code " + std::to_string(errno));
                break;
            }

            msg.push_back('\n');
            std::cerr << msg;
        }
    }

    this->fd = -1;
}

template<size_t Sz>
void _SharedMemory<Sz>::unlink() {
    const auto err {shm_unlink(this->_name.c_str())};

    if (err == -1) {
        std::string msg {
            "Shared memory: error when unlinking shared memory " + this->_name
        };

        switch (errno) {
            case EACCES:
                msg.append(": permission denied");
            break;
            case EMFILE:
            case ENFILE:
            case ENAMETOOLONG:
                msg.append(": file error");
            break;
            case ENOENT:
                // Probably already unlinked by another destructor
                return;
            break;
            default:
                msg.append(" error code " + std::to_string(errno));
            break;
        }

        msg.push_back('\n');
        std::cerr << msg;
    }
}

template<size_t Sz>
void _SharedMemory<Sz>::map() {
    const auto prot {PROT_READ | PROT_WRITE | PROT_EXEC};
    const auto flags {MAP_SHARED};

    this->_data = mmap(NULL, Sz, prot, flags, this->fd, 0);

    if (this->_data == MAP_FAILED) {
        std::string msg {
            "Shared memory: error mapping memory object " + this->_name +
            " of size " + std::to_string(Sz) + " bytes"
        };

        switch (errno) {
            case EACCES:
            case EBADF:
                msg.append(": permissions/file error");
            break;
            case EAGAIN:
                msg.append(": locking error");
            break;
            case EINVAL:
                // Size > 0 is asserted at compile-time
                msg.append(": too large");
            break;
            case ENODEV:
                msg.append(": filesystem does not support mamory mapping");
            break;
            case ENOMEM:
                msg.append(": no memory available or too many mappings");
            break;
            case EPERM:
                msg.append(": file sealed or execution denied");
            break;
            default:
                msg.append(" error code " + std::to_string(errno));
            break;
        }

        throw MemoryError(msg);
    }
}

template<size_t Sz>
void _SharedMemory<Sz>::unmap() {
    if (this->_data != nullptr && this->_data != MAP_FAILED) {
        const auto err {munmap(this->_data, Sz)};

        if (err == -1) {
            std::string msg {
                "Shared memory: error unmapping object " + this->_name
            };

            switch (errno) {
                case EINVAL:
                    msg.append(": internal error");
                break;
                default:
                    msg.append(" error code " + std::to_string(errno));
                break;
            }

            msg.push_back('\n');
            std::cerr << msg;
        }
    }
}


// class Object

template<class Tp>
Object<Tp>::Object(const std::string& nm):
_SharedMemory<sizeof(Tp)>(nm) {}

template<class Tp>
Object<Tp>::operator Tp&() noexcept {
    return *static_cast<Tp*>(this->_data);
}
template<class Tp>
Object<Tp>::operator Tp&() const noexcept {
    return *static_cast<Tp*>(this->_data);
}

template<class Tp>
Tp* Object<Tp>::operator->() noexcept {
    return this->_data;
}
template<class Tp>
const Tp* Object<Tp>::operator->() const noexcept {
    return this->_data;
}

template<class Tp>
Tp& Object<Tp>::get() noexcept {
    return *static_cast<Tp*>(this->_data);
}
template<class Tp>
const Tp& Object<Tp>::get() const noexcept {
    return *static_cast<Tp*>(this->_data);
}

template<class Tp>
Tp* Object<Tp>::data() noexcept {
    return static_cast<Tp*>(this->_data);
}


// class Array

template<class Tp, size_t Sz>
Array<Tp, Sz>::Array(const std::string& nm):
_SharedMemory<sizeof(Tp) * Sz>(nm) {}

template<class Tp, size_t Sz>
Tp& Array<Tp, Sz>::operator[](size_t n) noexcept {
    return static_cast<Tp*>(this->_data)[n];
}
template<class Tp, size_t Sz>
const Tp& Array<Tp, Sz>::operator[](size_t n) const noexcept {
    return static_cast<Tp*>(this->_data)[n];
}

template<class Tp, size_t Sz>
Tp& Array<Tp, Sz>::at(size_t n) {
    if (n >= Sz)
        throw std::out_of_range(
            "Shared memory: tried to access element " + std::to_string(n) +
            ", size = " + std::to_string(Sz)
        );
    return static_cast<Tp*>(this->_data)[n];
}
template<class Tp, size_t Sz>
const Tp& Array<Tp, Sz>::at(size_t n) const {
    if (n >= Sz)
        throw std::out_of_range(
            "Shared memory: tried to access element " + std::to_string(n) +
            ", size = " + std::to_string(Sz)
        );
    return static_cast<Tp*>(this->_data)[n];
}

template<class Tp, size_t Sz>
Tp* Array<Tp, Sz>::data() noexcept {
    return static_cast<Tp*>(this->_data);
}


// Other API functions

bool exists(const std::string& name) {
    bool b {false};

    auto fd {shm_open(name.c_str(), O_RDONLY, S_IRUSR)};

    if (fd == -1) {
        switch (errno) {
            default:
            case EINVAL:
            case ENAMETOOLONG:
            case ENOENT:
            case EMFILE:
            case ENFILE:
                b = false;
            break;

            case EEXIST:
            case EACCES:
                b = true;
            break;
        }
    }
    else {
        b = true;

        close(fd);
    }

    return b;
}

std::string formatName(const std::string& n) {
    std::string s {'/' + n};

    // Remove other slashes
    while (true) {
        auto pos {s.find('/', 1)};

        if (pos == std::string::npos)
            // No '/' found
            break;

        s.erase(pos, 1);
    }

    if (s.length() >= NAME_MAX)
        s.erase(s.begin() + NAME_MAX, s.end());

    return s;
}

} // namespace shm


#endif
