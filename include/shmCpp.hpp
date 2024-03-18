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
#include <type_traits>

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


/** Enumeration of access permissions/modes for shared memory. */
enum class Permissions
{
    /** Give the object read permission only. */
    ReadOnly,
    /** Give the object both read and write permissions. */
    ReadWrite
};


/** Shared memory object class.
 * This manages the shared memory from the OS' perspective.
 * @param Sz The number of bytes to store in the shared memory. Must be
 * greater than zero.
 */
template<size_t Sz>
class _SharedMemoryObject {
public:
    // Assert size is acceptable is ok
    static_assert(Sz > 0, "Cannot create shared memory object with size 0");

    /** Constructor.
     * Opens the SMO, creating it if it does not already exist, and sizes it.
     * @param name The name/identifier of the POSIX shared memory object.
     * @note It is advised to use @ref formatName on the name used.
     * @note If the SMO already exists and its size is larger than that
     * specified, the data past the end of the new length in the existing SMO
     * will be lost or corrupted. */
    _SharedMemoryObject(const std::string& name, Permissions perm);

    virtual ~_SharedMemoryObject();

    /** @returns A pointer to the mapped data. */
    inline void* get()
        { return this->_data; }
    inline const void* get() const
        { return this->_data; }

    /** @returns the name used to open the shared memory. */
    inline const std::string& name() const noexcept
        { return this->_name; }

    /** Checks if the memory is mapped. */
    inline bool empty() const noexcept
        { return this->_data != nullptr; }

    /** @returns `true` if the object has write permissions. */
    inline bool is_writable() const noexcept
        { return this->_perm != Permissions::ReadOnly; }

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

    /** Mapped data. */
    void* _data;

    /** Shared memory object's name. */
    const std::string _name;

    /** Shared memory object's permission. */
    Permissions _perm;

    /** File descriptor for the SMO. */
    int fd;
};


/** Class for creating and manipulating a POSIX shared memory object (SMO). */
template<class Tp>
class Object {
public:
    /** Constructor.
     * Opens the SMO, creating it if it does not already exist.
     * @param name The name/identifier of the POSIX shared memory object.
     * @note It is advised to use @ref formatName on the name used.
     * @note If the SMO already exists and the types of this and the other
     * `shm::Object` are not the same size, the data may be corrupted. */
    Object(const std::string& name, Permissions perm = Permissions::ReadWrite):
    _obj{_SharedMemoryObject<sizeof(Tp)>(name, perm)}
    {}

    ~Object() = default;

    /** Object access. */
    inline operator Tp&() noexcept
        { return *this->get_typed(); }
    inline operator Tp&() const noexcept
        { return *this->get_typed(); }
    inline Tp* operator->() noexcept
        { return this->get_typed(); }
    inline const Tp* operator->() const noexcept
        { return this->get_typed(); }
    inline Tp& get() noexcept
        { return *this->get_typed(); }
    inline const Tp& get() const noexcept
        { return *this->get_typed(); }

    /** Object assignment. */
    inline Object& operator=(const Tp& obj)
        { *this->get_typed() = obj; return *this; }
    inline Object& operator=(Tp&& obj)
        { *this->get_typed() = obj; return *this; }

    /** Direct access to the mapped memory. */
    inline Tp* data() noexcept
        { return this->get_typed(); }
    inline const Tp* data() const noexcept
        { return this->get_typed(); }

private:
    inline Tp* get_typed()
        { return static_cast<Tp*>(this->_obj.get()); }
    inline const Tp* get_typed() const
        { return static_cast<const Tp*>(this->_obj.get()); }

    _SharedMemoryObject<sizeof(Tp)> _obj;
};


/** Class for creating and manipulating a POSIX shared memory object (SMO) array. */
template<class Tp, size_t Sz>
class Array {
public:
    /** Constructor.
     * Opens the SMO, creating it if it does not already exist.
     * @param name The name/identifier of the POSIX shared memory object.
     * @note It is advised to use @ref formatName on the name used.
     * @note If the SMO already exists and its size is larger than that
     * specified, the data past the end of the new length in the existing SMO
     * will be lost. */
    Array(const std::string& name, Permissions perm = Permissions::ReadWrite):
    _obj{_SharedMemoryObject<sizeof(Tp) * Sz>(name, perm)}
    {}

    ~Array() = default;

    /** Element access. */
    inline Tp& operator[](size_t n) noexcept
        { return this->get_typed()[n]; }
    inline const Tp& operator[](size_t n) const noexcept
        { return this->get_typed()[n]; }

    /** Bounds-checked element access. */
    inline Tp& at(size_t n)
    {
        if (n >= Sz)
            throw std::out_of_range(
                "Shared memory: tried to access element " + std::to_string(n) +
                ", size = " + std::to_string(Sz)
            );
        return (*this)[n];
    }
    inline const Tp& at(size_t n) const
    {
        if (n >= Sz)
            throw std::out_of_range(
                "Shared memory: tried to access element " + std::to_string(n) +
                ", size = " + std::to_string(Sz)
            );
        return (*this)[n];
    }

    /** @returns @ref Sz; the number of @ref Tp objects in the Array. */
    constexpr size_t size() noexcept
        { return Sz; }

    /** Direct access to the mapped memory. */
    inline Tp* data() noexcept
        { return this->get_typed(); }
    inline const Tp* data() const noexcept
        { return this->get_typed(); }

    /** @returns An iterator to the beginning. */
    inline Tp* begin()
        { return this->data(); }
    inline const Tp* begin() const
        { return this->data(); }
    inline const Tp* cbegin() const
        { return this->data(); }

    /** @returns An iterator to the end. */
    inline Tp* end()
        { return this->data() + this->size(); }
    inline const Tp* end() const
        { return this->data() + this->size(); }
    inline const Tp* cend() const
        { return this->data() + this->size(); }

private:
    inline Tp* get_typed()
        { return static_cast<Tp*>(this->_obj.get()); }
    inline const Tp* get_typed() const
        { return static_cast<const Tp*>(this->_obj.get()); }

    _SharedMemoryObject<sizeof(Tp) * Sz> _obj;
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

// class _SharedMemoryObject

template<size_t Sz>
_SharedMemoryObject<Sz>::_SharedMemoryObject(const std::string& nm, Permissions perm):
_data{nullptr},
_name{nm},
_perm{perm},
fd{-1}
{
    this->open();
    this->map();
    this->close();
}

template<size_t Sz>
_SharedMemoryObject<Sz>::~_SharedMemoryObject() {
    this->unmap();
    this->unlink();
}

template<size_t Sz>
void _SharedMemoryObject<Sz>::open() {
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
void _SharedMemoryObject<Sz>::close() {
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
void _SharedMemoryObject<Sz>::unlink() {
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
void _SharedMemoryObject<Sz>::map() {
    const auto prot = [this]{
        int p = PROT_READ;
        if (this->is_writable())
            p |= PROT_WRITE;
        return p;
    }();
    const auto flags = this->is_writable() ? MAP_SHARED : MAP_PRIVATE;

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
void _SharedMemoryObject<Sz>::unmap() {
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
