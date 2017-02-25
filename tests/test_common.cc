
#include "test_common.h"

TestSuite::TestSuite() :
    cntPass(0),
    cntFail(0)
{
}

TestSuite::~TestSuite()
{
    printf(_CL_GREEN("%zu") " tests passed", cntPass);
    if (cntFail) {
        printf(", " _CL_RED("%zu") " tests failed", cntFail);
    }
    printf(" out of " _CL_CYAN("%zu") "\n", cntPass+cntFail);
}

std::string TestSuite::getTestFileName(std::string prefix)
{
    std::string ret = prefix;
    int rnd_num = std::rand();
    ret += "_";
    ret += std::to_string(rnd_num);
    return ret;
}

void TestSuite::clearTestFile(std::string prefix)
{
    int r;
    (void)r;
    std::string command = "rm -f ";
    command += prefix;
    command += "*";
    r = system(command.c_str());
}

void TestSuite::doTest(std::string test_name,
                       test_func *func)
{
    printf("[ " "...." " ] %s\n", test_name.c_str());
    fflush(stdout);

    int ret = func();
    if (ret < 0) {
        printf("[ " _CL_RED("FAIL") " ] %s\n", test_name.c_str());
        cntFail++;
    } else {
        // Move a line up
        printf("\033[1A");
        // Clear current line
        printf("\r");
        // Overwrite
        printf("[ " _CL_GREEN("PASS") " ] %s\n", test_name.c_str());
        cntPass++;
    }
}

void TestSuite::doTest(std::string test_name,
                       test_func_args *func,
                       TestArgsBase *args)
{
    args->setCallback(test_name, func, this);
    args->testAll();
}

void TestSuite::doTestCB(std::string test_name,
                  test_func_args *func,
                  TestArgsBase *args)
{
    printf("[ " "...." " ] %s\n", test_name.c_str());
    fflush(stdout);

    int ret = func(args);
    if (ret < 0) {
        printf("[ " _CL_RED("FAIL") " ] %s\n", test_name.c_str());
        cntFail++;
    } else {
        // Move a line up
        printf("\033[1A");
        // Clear current line
        printf("\r");
        // Overwrite
        printf("[ " _CL_GREEN("PASS") " ] %s\n", test_name.c_str());
        cntPass++;
    }
}

void test_range_test() {
    TestRange<bool> aa({false, true});
    size_t i, n;

    n = aa.getSteps();
    std::cout << n << "\n";
    for (i=0; i<n; ++i) {
        std::cout << aa.getEntry(i) << "\n";
    }
    std::cout << "\n";

    TestRange<int> bb(2, 8, 2);
    n = bb.getSteps();
    std::cout << n << "\n";
    for (i=0; i<n; ++i) {
        std::cout << bb.getEntry(i) << "\n";
    }
    std::cout << "\n";

    TestRange<int> cc(2, 7, 2);
    n = cc.getSteps();
    std::cout << n << "\n";
    for (i=0; i<n; ++i) {
        std::cout << cc.getEntry(i) << "\n";
    }
    std::cout << "\n";

    TestRange<int> dd(32, 256, 2, TestRange<int>::StepType::EXPONENTIAL);
    n = dd.getSteps();
    std::cout << n << "\n";
    for (i=0; i<n; ++i) {
        std::cout << dd.getEntry(i) << "\n";
    }
    std::cout << "\n";

    TestRange<int> ee(32, 192, 2, TestRange<int>::StepType::EXPONENTIAL);
    n = ee.getSteps();
    std::cout << n << "\n";
    for (i=0; i<n; ++i) {
        std::cout << ee.getEntry(i) << "\n";
    }
    std::cout << "\n";
}


class MyArgs : public TestArgsBase {
public:
    MyArgs() {
        args = std::make_tuple(&arg_bool, &arg_int);
        ranges = std::make_tuple(
                 TestRange<bool>({false, true}),
                 TestRange<int> ({32, 64, 128, 256}) );
    }

    std::string toString()
    {
        std::string ret;
        ret += (arg_bool ? "true" : "false");
        ret += ", " + std::to_string(arg_int);
        return ret;
    }

    TEST_ARGS_CONTENTS();
    bool arg_bool;
    int arg_int;

private:
    std::tuple<bool*, int*> args;
    std::tuple<TestRange<bool>, TestRange<int>> ranges;
};

int myargs_test(TestArgsBase *args) {
    MyArgs *my_args = static_cast<MyArgs*>(args);
    (void)my_args;
    return 0;
}

