#pragma once

#include "Common.h"
#include "UnixSocket.h"

#include <liburing.h>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/epoll.h>

static inline void _ring_add_poll(io_uring* ring, int fd, void* data)
{
    static_assert(sizeof(data) == sizeof(io_uring_sqe::user_data), "pointer size mismatch");

    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    io_uring_prep_poll_add(sqe, fd, POLLIN);
    io_uring_sqe_set_data(sqe, data);
    io_uring_submit(ring);
}

class RunLoop
{
public:
    static constexpr size_t bufSize = 1024;
    static constexpr size_t numBuffers = 1024;
    std::vector<char*> buffers;

    io_uring* ring = nullptr;
    int epollfd = 0;
    uint64_t eventTime = 0;

    bool stop = false;

    uint64_t pollTimeout = Time::second * 5;

    RunLoop(bool useEpoll = false)
    {
        if(useEpoll)
        {
            epollfd = epoll_create(16);
            fatal_assert(epollfd > 0, "Failed to create epoll: %d %s\n", errno, strerror(errno));
        }

        static constexpr int uring_max_entries = 64;

        ring = (io_uring*)calloc(1, sizeof(io_uring));
        int err = io_uring_queue_init(uring_max_entries, ring, 0);
        fatal_assert(err == 0, "failed to init uring: %d %s\n", err, strerror(-err));

        for(size_t i = 0; i < numBuffers; i++)
            buffers.push_back((char*)aligned_alloc(64, bufSize));
    }

    ~RunLoop()
    {
        if(ring)
        {
            io_uring_queue_exit(ring);
            free(ring);
        }
    }

    struct CallbackInterface
    {
        virtual ~CallbackInterface() {}
        virtual int getFileDescriptor() = 0;

        uint32_t eventType = 0;
    };

    struct ReaderInterface : CallbackInterface
    {
        static constexpr uint32_t type = __COUNTER__;

        ReaderInterface() { eventType = type; }
        virtual ~ReaderInterface() {}

        virtual void onMessage(RunLoop* rl, char* buf, size_t len) = 0;
        virtual void onDisconnect(RunLoop* rl) = 0;

        bool deleteOnDisconnect = false;
        unsigned bufSize = 8192;
        iovec* iov = nullptr;
    };

    struct ListenInterface : CallbackInterface
    {
        typedef UnixServerSocket ServerSocket;

        static constexpr uint32_t type = __COUNTER__;

        ListenInterface(ServerSocket* server)
            : socket(server)
        {
            static_assert(type != 0, "wrong type");

            eventType = type;
        }

        virtual ~ListenInterface()
        {
            for(auto c : clients)
                delete c;
        }

        ServerSocket* socket = nullptr;

        virtual int getFileDescriptor() override { return socket->fd; }

        std::vector<ReaderInterface*> clients;
        void removeClient(ReaderInterface* c)
        {
            clients.erase(std::remove(clients.begin(), clients.end(), c), clients.end());
        }

        virtual void onAccept(RunLoop* rl, const typename ServerSocket::ClientSocket& socket) = 0;
        virtual void onClientMessage(RunLoop* rl, const typename ServerSocket::ClientSocket& clientsock, char* data, size_t len) = 0;

        virtual void onClientDisconnect(RunLoop* rl, const typename ServerSocket::ClientSocket& clientsock) = 0;
    };

    struct ListenClientHelper : RunLoop::ReaderInterface
    {
        typedef UnixServerSocket ServerSocket;
        typedef typename ServerSocket::ClientSocket ClientSocket;

        RunLoop::ListenInterface* listener = nullptr;
        ClientSocket socket;

        ListenClientHelper(RunLoop::ListenInterface* listener, ClientSocket&& socket)
            : listener(listener), socket(socket)
        {
            ReaderInterface::deleteOnDisconnect = true;
        }

        virtual int getFileDescriptor() override { return socket.fd; }

        virtual void onMessage(RunLoop* rl, char* buf, size_t len) override
        {
            listener->onClientMessage(rl, socket, buf, len);
        }

        virtual void onDisconnect(RunLoop* rl) override
        {
            listener->onClientDisconnect(rl, socket);
            listener->removeClient(this);
        }
    };

    void addEpollEventHandler(CallbackInterface* cbi)
    {
        struct epoll_event ev = {};
        ev.events = EPOLLIN | EPOLLHUP | EPOLLPRI | EPOLLWAKEUP;
        ev.data.ptr = cbi;
        int res = epoll_ctl(epollfd, EPOLL_CTL_ADD, cbi->getFileDescriptor(), &ev);
        fatal_assert(res == 0, "Could not add epoll handler: %d %s\n", errno, strerror(errno));
        return;
    }


    void addReader(ReaderInterface* reader)
    {
        if(unlikely(epollfd))
        {
            addEpollEventHandler(reader);
            return;
        }

        if(unlikely(!reader->iov))
            reader->iov = (iovec*)calloc(1, sizeof(iovec));

        if(likely(!reader->iov->iov_base))
        {
            reader->iov->iov_base = buffers.back();
            reader->iov->iov_len = bufSize;
            buffers.pop_back();
        }

        io_uring_sqe* sqe = io_uring_get_sqe(ring);
        fatal_assert(sqe, "RunLoop::addReader failed to get uring sqe %p\n", sqe);

        io_uring_prep_readv(sqe, reader->getFileDescriptor(), reader->iov, 1, 0);
        io_uring_sqe_set_data(sqe, reader);
        int err = io_uring_submit(ring);
        fatal_assert(1 == err, "RunLoop::addReader io_uring_submit failed: %d %s\n", err, strerror(err));
    }

    void addSocketServer(ListenInterface* listener)
    {
        if(epollfd)
        {
            addEpollEventHandler(listener);
            return;
        }

        _ring_add_poll(ring, listener->socket->fd, listener);
    }

    void uringLoopRun(int core)
    {
        setAffinity(core);

        io_uring_cqe* cqe;
        while(!stop)
        {
            __kernel_timespec timeout;
            timeout.tv_sec = (time_t)(pollTimeout / Time::sec);
            timeout.tv_nsec = (long)(pollTimeout % Time::sec);
            int ret = io_uring_wait_cqe_timeout(ring, &cqe, &timeout);
            eventTime = Time::now();

            if(unlikely(ret < 0 && (-ret == EAGAIN || -ret == ETIME || -ret == EINTR)))
                continue;

            static_assert(sizeof(CallbackInterface*) == sizeof(cqe->user_data), "pointer sizeof mismatch");
            CallbackInterface* cbi = static_cast<CallbackInterface*>(io_uring_cqe_get_data(cqe));

            switch(cbi->eventType)
            {
            case ReaderInterface::type:
                {
                    auto r = static_cast<ReaderInterface*>(cbi);
                    if(unlikely(cqe->res == 0))
                    {
                        r->onDisconnect(this);
                        close(r->getFileDescriptor());
                    }
                    else
                    {
                        int res = cqe->res;
                        if(res < 0)
                            warn("RunLoop::ReaderInterface error: %d %s\n", -res, strerror(-res));
                        else
                        {
                            char* b = (char*)r->iov->iov_base;
                            r->onMessage(this, b, res);
                            r->iov->iov_base = nullptr;
                            buffers.push_back(b);
                        }

                        addReader(r);
                    }
                }
                break;
            case ListenInterface::type:
                {
                    auto l = static_cast<RunLoop::ListenInterface*>(cbi);
                    auto lc = new ListenClientHelper(l, std::move(l->socket->accept()));
                    l->clients.push_back(lc);

                    l->onAccept(this, lc->socket);
                    addReader(lc);

                    _ring_add_poll(ring, l->socket->fd, l);
                }
                break;
            }

            io_uring_cqe_seen(ring, cqe);
        }
    }

    void epollLoopRun(int core)
    {
        setAffinity(core);

        static constexpr int numEvents = 16;
        struct epoll_event events[numEvents];

        while(!stop)
        {
            int timeoutMs = pollTimeout / Time::msec;
            int nfds = epoll_wait(epollfd, events, numEvents, timeoutMs);
            eventTime = Time::now();
            fatal_assert(nfds >= 0, "epoll_wait error: %d %s\n", errno, strerror(errno));

            while(nfds-- > 0)
            {
                epoll_event& e = events[nfds];
                CallbackInterface* cbi = static_cast<CallbackInterface*>(e.data.ptr);
                switch(cbi->eventType)
                {
                case ReaderInterface::type:
                    {
                        auto r = static_cast<ReaderInterface*>(cbi);
                        char* buf = buffers.back(); // Do the std::vector dance to make things fair for io_uring
                        buffers.pop_back();
                        int ret = read(r->getFileDescriptor(), buf, sizeof(buf));

                        if(ret > 0)
                        {
                            r->onMessage(this, buf, ret);
                            buffers.push_back(buf);
                            continue;
                        }
                        else if(ret == 0)
                        {
                            r->onDisconnect(this);
                            close(r->getFileDescriptor());
                            continue;
                        }
                    }
                    break;
                case ListenInterface::type:
                    {
                        auto l = static_cast<RunLoop::ListenInterface*>(cbi);
                        auto lc = new ListenClientHelper(l, std::move(l->socket->accept()));
                        l->clients.push_back(lc);

                        l->onAccept(this, lc->socket);
                        addReader(lc);
                    }
                    break;
                }
            }
        }

    }
};

