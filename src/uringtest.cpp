#include <boost/lockfree/spsc_queue.hpp>

#include "RunLoop.h"
#include "PercentileTimer.h"

#include <thread>

static constexpr int sendcount = 1024 * 6;

static constexpr int sendGapUsec = 100;

void SocketPerformanceTest(bool useEpoll)
{
    const std::string path("usptest");
    UnixServerSocket clientSocket(path);

    PercentileTimer<1024> timer(useEpoll ? "EpollSocketTimes" : "UringSocketTimes");

    struct Receiver : RunLoop::ListenInterface
    {
        PercentileTimer<1024>& timer;
        std::atomic<int> recvcount = -1;

        Receiver(UnixServerSocket* socket, PercentileTimer<1024>& timer)
        : ListenInterface(socket), timer(timer)
        { }

        virtual void onClientMessage(RunLoop* rl, const UnixSocket& s, char* data, size_t len) override
        {
            int64_t sendtime = *reinterpret_cast<const int64_t*>(data);
            timer.tick(sendtime);
            timer.tock(rl->eventTime);

            recvcount += len / sizeof(uint64_t);
        }

        virtual void onClientDisconnect(RunLoop* rl, const UnixSocket&) override
        {
            printf("receiver disconnected\n");
        }

        virtual void onAccept(RunLoop* rl, const UnixSocket& socket) override
        {
            recvcount = 0;
        }
    };

    setAffinity(0);

    Receiver r(&clientSocket, timer);
    RunLoop clientloop(useEpoll);
    clientloop.pollTimeout = Time::msec * 100;
    clientloop.addSocketServer(&r);
    std::thread clientthread([&] {
            if(useEpoll)
                clientloop.epollLoopRun(1);
            else
                clientloop.uringLoopRun(1);
            });

    UnixSocket senderSocket(path);
    senderSocket.connect();

    while(r.recvcount != 0)
        asm("pause");

    info("Sending %d timestamps\n", sendcount);

    for(int i = 0; i < sendcount; i++)
    {
        uint64_t sendTime = Time::now();
        senderSocket.send((char*)&sendTime, sizeof(sendTime));
        usleep(sendGapUsec);
    }

    usleep(1000);

    clientloop.stop = true;
    clientthread.join();
}


void LocklessQueueTest()
{
    PercentileTimer<1024> timer("LocklessTimes");
    typedef boost::lockfree::spsc_queue<uint64_t, boost::lockfree::capacity<sendcount>> SPSCQueue;

    SPSCQueue q;
    std::atomic<bool> receiverReady = false;
    std::atomic<unsigned> nextExpected = 0;

    std::thread receiver([&]()
        {
            setAffinity(1);
            receiverReady = true;

            for(unsigned i = 0; i < sendcount; i++)
            {
                uint64_t sendTime;
                while(!q.pop(sendTime))
                    asm("pause");

                timer.tick(sendTime);
                timer.tock(Time::now());

                nextExpected = i;
            }
        });

    std::thread sender([&]()
        {
            setAffinity(0);
            while(!receiverReady)
                asm("pause");

            for(unsigned i = 0; i < sendcount; i++)
            {
                q.push(Time::now());

                while(nextExpected != i)
                    usleep(sendGapUsec);
            }
        });

    receiver.join();
    sender.join();
}

int main(int argc, char** argv)
{
    LocklessQueueTest();
    SocketPerformanceTest(true);
    SocketPerformanceTest(false);

    return 0;
}
