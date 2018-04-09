#include "test_common.h"
#include "latency_collector.h"
#include "latency_dump.h"

#include <thread>

#include <stdio.h>

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

    LatencyDumpDefaultImpl default_dump;
    printf("%s\n", lat.dump(&default_dump).c_str());

    printf("%s\n", global_lat->dump(&default_dump).c_str());

    delete global_lat;
    global_lat = nullptr;

    return 0;
}

void inner_function() {
    collectFuncLatency(global_lat);
    std::this_thread::sleep_for(std::chrono::microseconds(1));
}

void test_function_1ms() {
    collectFuncLatency(global_lat);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    inner_function();
}

void test_function_2ms() {
    collectFuncLatency(global_lat);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    inner_function();
}

void test_function_3ms() {
    collectFuncLatency(global_lat);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    inner_function();
}

void test_function_4ms() {
    collectFuncLatency(global_lat);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    inner_function();
}

int latency_macro_test() {
    global_lat = new LatencyCollector();

    size_t i, j;
    std::vector<size_t> n_calls = {23, 19, 13};
    std::vector<std::function<void()> >
            funcs = {test_function_1ms,
                     test_function_2ms,
                     test_function_3ms};

    for (i=0; i<n_calls.size(); ++i) {
        collectBlockLatency(global_lat, "outer for-loop");
        for (j=0; j<n_calls[i]; ++j) {
            funcs[i]();
        }
    }

    for (i=0; i<n_calls.size(); ++i) {
        funcs[i]();
    }

    for (i=0; i<n_calls[2]+1; ++i) {
        test_function_4ms();
    }

    LatencyDumpDefaultImpl default_dump;

    LatencyCollectorDumpOptions opt;
    opt.view_type = LatencyCollectorDumpOptions::TREE;
    opt.sort_by = LatencyCollectorDumpOptions::TOTAL_TIME;
    printf("%s\n", global_lat->dump(&default_dump, opt).c_str());

    opt.sort_by = LatencyCollectorDumpOptions::AVG_LATENCY;
    printf("%s\n", global_lat->dump(&default_dump, opt).c_str());

    opt.sort_by = LatencyCollectorDumpOptions::NUM_CALLS;
    printf("%s\n", global_lat->dump(&default_dump, opt).c_str());

    opt.view_type = LatencyCollectorDumpOptions::FLAT;
    opt.sort_by = LatencyCollectorDumpOptions::TOTAL_TIME;
    printf("%s\n", global_lat->dump(&default_dump, opt).c_str());

    opt.sort_by = LatencyCollectorDumpOptions::AVG_LATENCY;
    printf("%s\n", global_lat->dump(&default_dump, opt).c_str());

    opt.sort_by = LatencyCollectorDumpOptions::NUM_CALLS;
    printf("%s\n", global_lat->dump(&default_dump, opt).c_str());

    LatencyItem chk = global_lat->getAggrItem("test_function_3ms");
    printf("%lu, %lu\n", chk.getTotalTime(), chk.getNumCalls());

    printf("%s\n", global_lat->dump(nullptr).c_str());

    delete global_lat;
    global_lat = nullptr;

    return 0;
}

int main()
{
    TestSuite test;

    test.options.printTestMessage = true;
    test.doTest("multi thread test", MT_basic_insert_test);
    test.doTest("function latency macro test", latency_macro_test);

    return 0;
}
