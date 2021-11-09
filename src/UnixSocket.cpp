#include "UnixSocket.h"
#include "Common.h"

#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

UnixSocket::UnixSocket(const std::string& path, bool abstractLinuxSocket)
: path(path), abstractLinuxSocket(abstractLinuxSocket)
{
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    fatal_assert(fd >= 0, "Failed to create UnixSocket at path: %s %d %s\n", path.c_str(), errno, strerror(errno));

    info("UnixSocket %s connected on fd %d\n", path.c_str(), fd);
}

bool UnixSocket::connect(bool)
{
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    socklen_t namelen = offsetof(struct sockaddr_un, sun_path) + path.length();
    if(abstractLinuxSocket)
    {
        addr.sun_path[0] = '\0';
        namelen++;
        strncpy(addr.sun_path + 1, path.c_str(), std::min(sizeof(addr.sun_path)-1, path.size()));
    }
    else
        strncpy(addr.sun_path, path.c_str(), std::min(sizeof(addr.sun_path), path.size()));

    int err = ::connect(fd, (struct sockaddr*)&addr, namelen);
    fatal_assert(err >= 0, "UnixSocket failed to connect: %d %s\n",
            errno, strerror(errno)); // warn instead of fatal?

    return err >= 0;
}

int64_t UnixSocket::send(const char* buf, unsigned len) const
{
    return ::send(fd, buf, len, MSG_DONTWAIT);
}

int64_t UnixSocket::read(const char* buf, unsigned len) const
{
    return ::read(fd, (char*)buf, len);
}

UnixSocket::UnixSocket(int fd, const std::string& path)
: path(path), fd(fd)
{
    fd = fd;
    info("UnixSocket client %s connected on fd %d\n", path.c_str(), fd);
}


UnixServerSocket::UnixServerSocket(const std::string& path, bool abstractLinuxSocket)
    : path(path)
{
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    fatal_assert(fd >= 0, "Failed to create UnixServerSocket at path: %s %d %s\n", path.c_str(), errno, strerror(errno));

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    socklen_t namelen = offsetof(struct sockaddr_un, sun_path) + path.length();
    if(abstractLinuxSocket)
    {
        addr.sun_path[0] = '\0';
        namelen++;
        strncpy(addr.sun_path + 1, path.c_str(), std::min(sizeof(addr.sun_path)-1, path.size()));
    }
    else
        strncpy(addr.sun_path, path.c_str(), std::min(sizeof(addr.sun_path), path.size()));

    int err = bind(fd, (struct sockaddr*)&addr, namelen); // sizeof(addr));
    fatal_assert(err >= 0, "UnixServerSocket failed to bind: %d %s\n",
            errno, strerror(errno));
#if 1
    err = listen(fd, 10);
    fatal_assert(err >= 0, "UnixServerSocket failed to listen: %d %s\n",
            errno, strerror(errno));
#endif
}

UnixServerSocket::~UnixServerSocket()
{
    close(fd);
}

UnixServerSocket::ClientSocket UnixServerSocket::accept()
{
    int clientfd = ::accept(fd, nullptr, nullptr);
    fatal_assert(clientfd >= 0, "UnixServerSocket %.*s failed to accept: %d %s\n",
            (int)path.size(), &path.data()[1], errno, strerror(errno));

    return UnixSocket(clientfd, path);
}
