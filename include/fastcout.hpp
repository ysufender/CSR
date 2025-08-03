#pragma once

#include <cstring>
#include <iostream>
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
		FastCout(size_t bufferSize = 4 << 20)
			: bufSize { bufferSize }
		{
#ifdef _WIN32
			this->buffer = reinterpret_cast<char*>(_aligned_malloc(this->bufferSize, 4096));
#else
			posix_memalign(reinterpret_cast<void**>(&this->buffer), sysconf(_SC_PAGESIZE), this->bufSize);
#endif
			this->setp(this->buffer, this->buffer + this->bufSize);
		}

		~FastCout()
		{
			this->sync();
#ifdef _WIN32
			_aligned_free(this->buffer);
#else
			free(this->buffer);
#endif
		}

	protected:
		inline int_type overflow(int_type character) override
		{
			std::lock_guard<std::mutex> lockGuard { this->mutex };
			if (character != traits_type::eof())
			{
				*this->pptr() = character;
				this->pbump(1);
			}
			if (this->pptr() == this->epptr())
				this->FlushBuffer();
			return character;
		}

		inline std::streamsize xsputn(const char* data, std::streamsize length) override
		{
			std::lock_guard<std::mutex> lockGuard { this->mutex };
			std::streamsize written { 0 };
			while (written < length)
			{
				std::streamsize space { this->epptr() - this->pptr() };
				if (space == 0)
				{
					this->FlushBuffer();
					space = this->epptr() - this->pptr();
				}
				std::streamsize toWrite { std::min(space, length - written) };
				std::memcpy(this->pptr(), data + written, toWrite);
				this->pbump(static_cast<int>(toWrite));
				written += toWrite;
			}
			return written;
		}

		inline int sync() override
		{
			std::lock_guard<std::mutex> lockGuard { this->mutex };
			this->FlushBuffer();
			return 0;
		}

	private:
		inline void FlushBuffer()
		{
			if (!this->mutex.try_lock())
				this->mutex.unlock();
			size_t count { static_cast<size_t>(this->pptr() - this->pbase()) };
			if (count == 0)
			{
				this->setp(this->buffer, this->buffer + this->bufSize);
				return;
			}
#ifdef _WIN32
			DWORD written;
			WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), this->buffer, static_cast<DWORD>(count), &written, nullptr);
#else
			write(STDOUT_FILENO, this->buffer, count);
#endif
			this->setp(this->buffer, this->buffer + this->bufSize);
		}

		char* buffer;
		const size_t bufSize;
		std::mutex mutex;
};

inline void InitFastOutput(size_t bufferSize = 4 << 20)
{
	static FastCout fastCout { bufferSize };
	//std::ios::sync_with_stdio(false);
    std::cin.tie(&std::cout);
	std::cout.rdbuf(&fastCout);
}
