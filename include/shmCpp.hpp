#ifndef SHM_CPP_H
#define SHM_CPP_H

#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

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


/** Class for creating and manipulating a POSIX shared memory object (SMO). */
template<class T>
class SharedMemory {
    /** Constructor.
     * Opens the SMO, creating it if it does not already exist, and resizes it
     * to @a size.
     * @param name The name/identifier of the POSIX shared memory object.
     * @param size The number of T objects to store in the shared memory. Must
     * be greater than zero.
     * @note It is advised to use @ref formatName on the name used.
     * @note If the SMO already exists and its size is larger than that
     * specified, the data past the end of the new length in the existing SMO
     * will be lost. */
    SharedMemory(const std::string& name, size_t size);

    ~SharedMemory();

    /** @returns the name used to open the shared memory. */
    const std::string& name() const noexcept {
        return this->_name;
    }

    /** @returns the number of @ref T objects currently stored in the shared
     * memory; the size of the shared memory object. */
    size_t size() const noexcept {
        return this->_size;
    }

    /** Direct access to the mapped memory. */
    T* data() noexcept {
        return this->_data;
    }

    /** Checks if the memory is mapped. */
    bool empty() const noexcept;

    /** Element access. */
    T& operator[](size_t n) noexcept;
    const T& operator[](size_t n) const noexcept;

    /** Bounds-checked element access. */
    T& at(size_t n);
    const T& at(size_t n) const;

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

    const std::string _name;

    /** File descriptor for the SMO. */
    int fd;

    /** Virtual memory mapping.
     * Use this to access the data in the shared memory after mapping. */
    T* _data;

    /** Size of the data; the number of elements stored. */
    const size_t _size;
};

/** Tests whether a SMO called @a name exists.
 * @note This test will also fail if the memory fails to open for reasons such
 * as process- or system-wide limits on file openings being reached. */
bool exists(const std::string& name);

/** Formats the identifier name according to the naming conventions outlined at
 * https://www.man7.org/linux/man-pages/man3/shm_open.3.html#DESCRIPTION. */
std::string formatName(const std::string& name);

} // namespace shm

// END OF API HEADER
////////////////////////////////////////////////////////////////////////////////
// START OF IMPLEMENTATION

namespace shm {

// class SharedMemory

template<class T>
SharedMemory<T>::SharedMemory(const std::string& nm, size_t sz):
_name{nm}, fd{-1}, _data{nullptr}, _size{sz} {
    if (this->_size < 1)
        throw std::invalid_argument("Cannot create shared memory object with size < 1");

    this->open();
    this->map();
    this->close();
}

template<class T>
SharedMemory<T>::~SharedMemory() {
    this->unmap();
    this->unlink();
}

template<class T>
bool SharedMemory<T>::empty() const noexcept {
    return this->_data != nullptr;
}

template<class T>
T& SharedMemory<T>::operator[](size_t n) noexcept {
    return this->_data[n];
}
template<class T>
const T& SharedMemory<T>::operator[](size_t n) const noexcept {
    return this->_data[n];
}

template<class T>
T& SharedMemory<T>::at(size_t n) {
    if (n >= this->_size)
        throw std::out_of_range(
            "Shared memory: tried to access element " + std::to_string(n) +
            " (size = " + std::to_string(this->_size) + ')'
        );
    return this->_data[n];
}
template<class T>
const T& SharedMemory<T>::at(size_t n) const {
    if (n >= this->_size)
        throw std::out_of_range(
            "Shared memory: tried to access element " + std::to_string(n) +
            " (size = " + std::to_string(this->_size) + ')'
        );
    return this->_data[n];
}

template<class T>
void SharedMemory<T>::open() {
    const auto oflag {O_RDWR | O_CREAT};
    const mode_t mode {S_IRWXU | S_IRWXG | S_IRWXO};

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

    const auto err {ftruncate(this->fd, this->_size * sizeof(T))};

    if (err == -1) {
        // error handling
        std::string msg {
            "Shared memory: could not set shared memory object " + this->_name +
            " size to " + std::to_string(this->_size) + " (" + this->_size * sizeof(T)
            + " bytes)"
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

template<class T>
void SharedMemory<T>::close() {
    if (this->fd != -1) {
        // Only close the file if it is open
        const auto err {::close(this->fd)};

        if (err == -1) {
            // error handling
            std::string msg {
                "Shared memory: error when closing file " + this->_name +
                " with descriptor " + this->fd
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

template<class T>
void SharedMemory<T>::unlink() {
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
                msg.append(": does not exist");
            break;
            default:
                msg.append(" error code " + std::to_string(errno));
            break;
        }

        std::cerr << msg;
    }
}

template<class T>
void SharedMemory<T>::map() {
    const auto prot {PROT_READ | PROT_WRITE | PROT_EXEC};
    const auto flags {MAP_SHARED};

    this->_data = mmap(NULL, this->_size * sizeof(T), prot, flags, this->fd, 0);

    if (this->_data == MAP_FAILED) {
        std::string msg {
            "Shared memory: error mapping memory object " + this->_name +
            " of size " + std::to_string(this->_size) + " (" +
            std::to_string(this->_size * sizeof(T)) + " bytes)"
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
                if (this->_size * sizeof(T))
                    msg.append(": too large");
                else
                    msg.append(": cannot map zero bytes");
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

template<class T>
void SharedMemory<T>::unmap() {
    if (this->_data != nullptr && this->_data != MAP_FAILED) {
        const auto err {munmap(this->_data, this->_size)};

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


bool exists(const std::string& name) {
    bool b {false};

    auto fd {shm_open(name.c_str(), O_RDONLY)};

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
