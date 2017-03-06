#include <stdio.h>
#include <thread>

#include "test_common.h"
#include "latency.h"

struct test_args {
    LatencyCollector* lat;
};

LatencyCollector *global_lat;

void* insert_thread(void *voidargs)
{
    collectFuncLatency(global_lat);

    test_args *args = (test_args*)voidargs;
    size_t n = 1024 * 1;

    for (size_t i=0; i<n; ++i) {
        int item_no = std::rand() % 16;
        uint64_t latency = 100 + (std::rand() % 100);
        args->lat->addLatency(std::to_string(item_no), latency);
    }

    return nullptr;
}

int MT_basic_insert_test() {
    global_lat = new LatencyCollector();

    size_t i;
    size_t n_threads = 8;

    std::vector<std::thread> t_hdl(n_threads);
    test_args args;
    LatencyCollector lat;

    for (i=0; i<n_threads; ++i) {
        args.lat = &lat;
        t_hdl[i] = std::thread(insert_thread, &args);
    }

    for (i=0; i<n_threads; ++i){
        t_hdl[i].join();
    }

    printf("%s\n\n", lat.dump().c_str());

    printf("%s\n\n", global_lat->dump().c_str());
    delete global_lat;
    global_lat = nullptr;

    return 0;
}

void test_function_1ms() {
    collectFuncLatency(global_lat);
    std::this_thread::sleep_for (std::chrono::milliseconds(1));
}

void test_function_2ms() {
    collectFuncLatency(global_lat);
    std::this_thread::sleep_for (std::chrono::milliseconds(2));
}

void test_function_3ms() {
    collectFuncLatency(global_lat);
    std::this_thread::sleep_for (std::chrono::milliseconds(2));
}

int latency_macro_test() {
    global_lat = new LatencyCollector();

    size_t i, j;
    std::vector<size_t> n_calls = {53, 47, 17};
    std::vector<void (*)()> funcs = {test_function_1ms,
                                     test_function_2ms,
                                     test_function_3ms};

    for (i=0; i<n_calls.size(); ++i) {
        collectBlockLatency(global_lat, "outer for-loop");
        for (j=0; j<n_calls[i]; ++j) {
            funcs[i]();
        }
    }

    printf("%s\n\n", global_lat->dump().c_str());
    delete global_lat;
    global_lat = nullptr;

    return 0;
}

int main()
{
    TestSuite test;

    test.doTest("multi thread test", MT_basic_insert_test);
    test.doTest("function latency macro test", latency_macro_test);

    return 0;
}
