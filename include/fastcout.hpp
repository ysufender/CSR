#pragma once

#include "system.hpp"
#include <cstring>
#include <iostream>
#include <new>
#include <streambuf>
#include <cstdlib>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

class FastCout : public std::streambuf
{
public:
    enum BufferSize : size_t
    {
        Large = 4 << 20,
        Default = 1 << 20,
        Medium = 1 << 19,
        Small = 1 << 18
    };

    static VM_INLINE FastCout& Get() noexcept
    { return *FastCout::singleton; }

    static VM_INLINE void Init(BufferSize bufferSize = BufferSize::Default) noexcept
    {
        std::call_once(FastCout::initFlag, [bufferSize]() {
            FastCout::singleton = new (std::nothrow) FastCout { bufferSize };
            if (FastCout::singleton == nullptr)
            {
                std::cerr << "Error while initializing stdout\n";
                std::abort();
            }
            std::cout.rdbuf(FastCout::singleton);
        });
    }

    static VM_INLINE System::ErrorCode Flush() noexcept
    {
        if (FastCout::singleton == nullptr)
            return System::ErrorCode::IOError;

        if (FastCout::singleton->pubsync() == -1)
            return System::ErrorCode::IOError;

        return System::ErrorCode::Ok;
    }

protected:
    VM_INLINE int_type overflow(int_type character) override
    {
        std::lock_guard<std::mutex> lockGuard { this->mutex };
        if (character != traits_type::eof())
        {
            *this->pptr() = character;
            this->pbump(1);
        }
        if (this->pptr() == this->epptr())
            FlushBuffer();
        return character;
    }

    VM_INLINE std::streamsize xsputn(const char* data, std::streamsize length) override
    {
        std::lock_guard<std::mutex> lockGuard { this->mutex };
        std::streamsize written { 0 };
        while (written < length)
        {
            std::streamsize space { this->epptr() - this->pptr() };
            if (space == 0)
            {
                FlushBuffer();
                space = this->epptr() - this->pptr();
            }
            std::streamsize toWrite { std::min(space, length - written) };
            std::memcpy(this->pptr(), data + written, toWrite);
            this->pbump(static_cast<int>(toWrite));
            written += toWrite;
        }
        return written;
    }

    VM_INLINE int sync() override
    {
        std::lock_guard<std::mutex> lockGuard { this->mutex };
        return FlushBuffer() ? 0 : -1;
    }

private:
    char* buffer = nullptr;
    const size_t bufSize = 0;
    std::mutex mutex { };

    static inline FastCout* singleton = nullptr;
    static inline std::once_flag initFlag { };

    VM_INLINE FastCout(size_t bufferSize) : bufSize { bufferSize }
    {
#ifdef _WIN32
        this->buffer = reinterpret_cast<char*>(_aligned_malloc(this->bufSize, 4096));
#else
        if (posix_memalign(reinterpret_cast<void**>(&this->buffer),sysconf(_SC_PAGESIZE), this->bufSize) != 0)
            std::abort();
#endif
        if (this->buffer == nullptr)
            std::abort();

        this->setp(this->buffer, this->buffer + this->bufSize);
    }

    VM_INLINE bool FlushBuffer() noexcept
    {
        size_t count { static_cast<size_t>(this->pptr() - this->pbase()) };
        if (count == 0)
        {
            this->setp(this->buffer, this->buffer + this->bufSize);
            return true;
        }

#ifdef _WIN32
        DWORD written = 0;
        BOOL ok = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
                            this->buffer, static_cast<DWORD>(count),
                            &written, nullptr);
        if (!ok || written != count)
            return false;
#else
        ssize_t written = ::write(STDOUT_FILENO, this->buffer, count);
        if (written < 0 || static_cast<size_t>(written) != count)
            return false;
#endif

        this->setp(this->buffer, this->buffer + this->bufSize);
        return true;
    }
};
