#pragma once
/*
 * Platform compatibility shim.
 * Provides POSIX-like wrappers for functions that differ between Linux and
 * Windows/MinGW so the rest of the code can use Linux-style APIs uniformly.
 */

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <io.h>
    #include <sys/stat.h>
    // Map fdatasync/fsync → _commit (flushes OS buffers for a C fd)
    inline int fdatasync(int fd) { return _commit(fd); }
    inline int fsync(int fd)     { return _commit(fd); }
    // pread: read n bytes from fd at given offset without changing position
    #include <windows.h>
    #include <cstdint>
    inline ssize_t pread(int fd, void* buf, size_t n, off_t offset) {
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        OVERLAPPED ov{};
        ov.Offset     = (DWORD)((uint64_t)offset & 0xFFFFFFFFULL);
        ov.OffsetHigh = (DWORD)((uint64_t)offset >> 32);
        DWORD read = 0;
        if (!ReadFile(h, buf, (DWORD)n, &read, &ov)) return -1;
        return (ssize_t)read;
    }
    // Link with: -lws2_32
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif
