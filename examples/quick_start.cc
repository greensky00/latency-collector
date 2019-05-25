#include "latency_collector.h"
#include "latency_dump.h"

#include <cstdlib>
#include <iostream>
#include <thread>

static LatencyCollector lat_clt;

void function3() {
    collectFuncLatency(&lat_clt);
    std::this_thread::sleep_for( std::chrono::milliseconds( rand() % 10 ) );
}

void function2() {
    collectFuncLatency(&lat_clt);
    for (size_t ii=0; ii<10; ++ii) function3();
}

void function1() {
    collectFuncLatency(&lat_clt);
    for (size_t ii=0; ii<3; ++ii) function2();
}

int main(int argc, char** argv) {
    {
        collectBlockLatency(&lat_clt, "main block");
        for (size_t ii=0; ii<5; ++ii) function1();
        for (size_t ii=0; ii<2; ++ii) function2();
        for (size_t ii=0; ii<3; ++ii) function3();
    }

    LatencyDumpDefaultImpl default_dump;
    std::cout << lat_clt.dump(&default_dump) << std::endl;

    return 0;
}

