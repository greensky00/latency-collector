
Latency-Collector
---
Easy-to-use latency collecting library for C++ programs.


Author
---
Jung-Sang Ahn <jungsang.ahn@gmail.com>


How to Use
---
Copy all files in [src](./src) to your source repository.

Collecting latencies:
```C++
#include "latency_collector.h"

// Define a collector.
static LatencyCollector lat_clt;

void my_function() {
    // It will collect the latency of this function.
    collectFuncLatency(&lat_clt);
    // ... your code to measure latency ...
}

void another_function() {
   // ...
   {
       // It will collect the latency of this block.
       collectBlockLatency(&lat_clt, "latency name");
       // ... your code to measure latency ...
   }
}
```

How to dump (using the default dump implementation):
```C++
#include "latency_dump.h"

int main() {
    // ...
    LatencyDumpDefaultImpl default_dump;
    std::cout << lat_clt.dump(&default_dump) << std::endl;
    // ...
}
```

It will print out the results as follows:
```
STAT NAME   :    TOTAL   RATIO  CALLS  AVERAGE      p50      p99    p99.9
my_function : 131.1 ms     ---    100   1.3 ms   1.2 ms   3.5 ms   4.7 ms
latency name:   501 us     ---    100     5 us     4 us    10 us   120 us
```

Please refer to [examples/quick_start.cc](./examples/quick_start.cc) or [tests/latency_test.cc](./tests/latency_test.cc) for more details.