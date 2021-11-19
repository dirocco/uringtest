# uringtest
A test comparing io_uring, epoll, and boost::lockfree::spsc_queue latency when using a pair of Unix stream sockets.

### To build:
```
cmake .
make
```

### Sample output:

Tested on Ubuntu 20.04.3 LTS with kernel 5.11.0-38-generic 

```
LocklessTimes 1024 p25: 0.129 p50: 0.139 p90: 0.199 p95: 0.228 p99: 0.268 p999: 2.026
LocklessTimes 1024 p25: 0.129 p50: 0.139 p90: 0.189 p95: 0.228 p99: 0.239 p999: 2.026
LocklessTimes 1024 p25: 0.129 p50: 0.139 p90: 0.189 p95: 0.228 p99: 0.238 p999: 2.017
LocklessTimes 1024 p25: 0.129 p50: 0.139 p90: 0.189 p95: 0.228 p99: 0.239 p999: 1.619
LocklessTimes 1024 p25: 0.129 p50: 0.139 p90: 0.189 p95: 0.219 p99: 0.238 p999: 3.397
LocklessTimes 1024 p25: 0.129 p50: 0.139 p90: 0.198 p95: 0.228 p99: 0.248 p999: 2.930
---
EpollSocketTimes 1024 p25: 3.466 p50: 3.745 p90: 6.237 p95: 7.042 p99: 8.790 p999: 10.806
EpollSocketTimes 1024 p25: 3.477 p50: 3.734 p90: 6.068 p95: 6.952 p99: 9.872 p999: 15.911
EpollSocketTimes 1024 p25: 3.496 p50: 3.774 p90: 6.366 p95: 7.191 p99: 8.620 p999: 9.813
EpollSocketTimes 1024 p25: 3.496 p50: 3.804 p90: 6.317 p95: 7.032 p99: 10.280 p999: 14.839
EpollSocketTimes 1024 p25: 3.477 p50: 3.744 p90: 6.496 p95: 7.310 p99: 11.938 p999: 974.607
EpollSocketTimes 1024 p25: 3.655 p50: 4.181 p90: 7.061 p95: 7.727 p99: 10.121 p999: 19.615
---
UringSocketTimes 1024 p25: 4.718 p50: 5.066 p90: 7.976 p95: 10.925 p99: 15.394 p999: 36.610
UringSocketTimes 1024 p25: 4.797 p50: 5.354 p90: 8.611 p95: 9.942 p99: 14.064 p999: 20.153
UringSocketTimes 1024 p25: 4.836 p50: 5.532 p90: 8.204 p95: 9.257 p99: 11.382 p999: 16.209
UringSocketTimes 1024 p25: 4.857 p50: 5.910 p90: 8.968 p95: 10.280 p99: 15.553 p999: 36.609
UringSocketTimes 1024 p25: 4.986 p50: 7.191 p90: 9.187 p95: 10.717 p99: 14.839 p999: 23.360
UringSocketTimes 1024 p25: 4.827 p50: 5.890 p90: 8.363 p95: 9.574 p99: 13.448 p999: 18.473
---

```
