#pragma once

#include "Common.h"

#include <string>

class UnixSocket
{
public:
    UnixSocket(const std::string& path, bool abstractLinuxSocket = true);
    virtual ~UnixSocket() {}

    virtual bool connect(bool makeNonBlocking = false);
    virtual int64_t send(const char* buf, unsigned len) const;
    virtual int64_t read(const char* buf, unsigned len) const;

    std::string path;
    bool abstractLinuxSocket;
    int fd;

    UnixSocket(int fd, const std::string& path);
};


class UnixServerSocket
{
public:
    typedef UnixSocket ClientSocket;

    int fd = -1;
    std::string path;

    char* namebuf = nullptr;
    const char* endpointName() const;

    UnixServerSocket(const std::string& path, bool abstractLinuxSocket = true);
    virtual ~UnixServerSocket();

    ClientSocket accept();
};
