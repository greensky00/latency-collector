#pragma once

#include <string>
#include <iostream>
#include <vector>
#include <tuple>
#include <cmath>

#define _CLM_GREEN     "\033[32m"
#define _CLM_B_GREEN   "\033[1;32m"
#define _CLM_RED       "\033[31m"
#define _CLM_B_RED     "\033[1;31m"
#define _CLM_BLUE      "\033[34m"
#define _CLM_B_BLUE    "\033[1;34m"
#define _CLM_MAGENTA   "\033[35m"
#define _CLM_B_MAGENTA "\033[1;35m"
#define _CLM_CYAN      "\033[36m"
#define _CLM_END       "\033[0m"

#define _CL_GREEN(str)     _CLM_GREEN str _CLM_END
#define _CL_RED(str)       _CLM_RED str _CLM_END
#define _CL_MAGENTA(str)   _CLM_MAGENTA str _CLM_END
#define _CL_B_MAGENTA(str) _CLM_MAGENTA str _CLM_END
#define _CL_CYAN(str)      _CLM_CYAN str _CLM_END


#define CHK_EQ(exp_value, value) \
    if ((exp_value) != (value)) { \
        std::cout << "\n    " _CLM_GREEN << __FILE__ << _CLM_END ":"; \
        std::cout << _CLM_GREEN << __LINE__ << _CLM_END << ", " ; \
        std::cout << _CLM_CYAN << __func__ << "()" _CLM_END << "\n"; \
        std::cout << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"; \
        std::cout << "    expected: " _CLM_B_GREEN << (exp_value) << _CLM_END "\n"; \
        std::cout << "      actual: " _CLM_B_RED << (value) << _CLM_END "\n"; \
        return -1; \
    } \

#define CHK_OK(value) \
    if (!(value)) { \
        std::cout << "\n    " _CLM_GREEN << __FILE__ << _CLM_END ":"; \
        std::cout << _CLM_GREEN << __LINE__ << _CLM_END << ", " ; \
        std::cout << _CLM_CYAN << __func__ << "()" _CLM_END << "\n"; \
        std::cout << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"; \
        std::cout << "    expected: " _CLM_B_GREEN << "true" << _CLM_END "\n"; \
        std::cout << "      actual: " _CLM_B_RED << "false" << _CLM_END "\n"; \
        return -1; \
    } \

#define CHK_NOT(value) \
    if (value) { \
        std::cout << "\n    " _CLM_GREEN << __FILE__ << _CLM_END ":"; \
        std::cout << _CLM_GREEN << __LINE__ << _CLM_END << ", " ; \
        std::cout << _CLM_CYAN << __func__ << "()" _CLM_END << "\n"; \
        std::cout << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"; \
        std::cout << "    expected: " _CLM_B_GREEN << "false" << _CLM_END "\n"; \
        std::cout << "      actual: " _CLM_B_RED << "true" << _CLM_END "\n"; \
        return -1; \
    } \

#define CHK_NULL(value) \
    if (value) { \
        std::cout << "\n    " _CLM_GREEN << __FILE__ << _CLM_END ":"; \
        std::cout << _CLM_GREEN << __LINE__ << _CLM_END << ", " ; \
        std::cout << _CLM_CYAN << __func__ << "()" _CLM_END << "\n"; \
        std::cout << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"; \
        std::cout << "    expected: " _CLM_B_GREEN << "NULL" << _CLM_END "\n"; \
        printf("      actual: " _CLM_B_RED "%p" _CLM_END "\n", value); \
        return -1; \
    } \

#define CHK_NONNULL(value) \
    if (!(value)) { \
        std::cout << "\n    " _CLM_GREEN << __FILE__ << _CLM_END ":"; \
        std::cout << _CLM_GREEN << __LINE__ << _CLM_END << ", " ; \
        std::cout << _CLM_CYAN << __func__ << "()" _CLM_END << "\n"; \
        std::cout << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"; \
        std::cout << "    expected: " _CLM_B_GREEN << "non-NULL" << _CLM_END "\n"; \
        std::cout << "      actual: " _CLM_B_RED << "NULL" << _CLM_END "\n"; \
        return -1; \
    } \



typedef int (test_func)();

class TestArgsBase;
typedef int (test_func_args)(TestArgsBase* args);

class TestSuite {
public:
    TestSuite();
    ~TestSuite();

    static std::string getTestFileName(std::string prefix);
    static void clearTestFile(std::string prefix);

    void doTest(std::string test_name,
                test_func *func);

    void doTest(std::string test_name,
                test_func_args *func,
                TestArgsBase *args);

    void doTestCB(std::string test_name,
                  test_func_args *func,
                  TestArgsBase *args);

private:
    size_t cntPass;
    size_t cntFail;
};


template<typename T>
class TestRange {
public:
    enum class StepType {
        LINEAR,
        EXPONENTIAL
    };

    TestRange() {
        type = RangeType::NONE;
    }

    // Constructor for given values
    TestRange(const std::vector<T>& _array) :
        type(RangeType::ARRAY), array(_array)
    { }

    // Constructor for regular steps
    TestRange(T _begin, T _end, T _step,
              StepType _type = StepType::LINEAR) :
        begin(_begin), end(_end), step(_step)
    {
        if (_type == StepType::LINEAR) {
            type = RangeType::LINEAR;
        } else {
            type = RangeType::EXPONENTIAL;
        }
    }

    T getEntry(size_t idx) {
        if (type == RangeType::ARRAY) {
            return array[idx];
        } else if (type == RangeType::LINEAR) {
            return begin + step * idx;
        } else if (type == RangeType::EXPONENTIAL) {
            return begin * std::pow(step, idx);
        }

        return begin;
    }

    size_t getSteps() {
        if (type == RangeType::ARRAY) {
            return array.size();
        } else if (type == RangeType::LINEAR) {
            return ((end - begin) / step) + 1;
        } else if (type == RangeType::EXPONENTIAL) {
            size_t coe = end / begin;
            double steps_double = (double)std::log(coe) / std::log(step);
            return steps_double + 1;
        }

        return 0;
    }

private:
    enum class RangeType {
        NONE,
        ARRAY,
        LINEAR,
        EXPONENTIAL
    };

    RangeType type;
    std::vector<T> array;
    T begin;
    T end;
    T step;
};


struct TestArgsSetParamFunctor
{
    template<typename T>
    void operator()(T* t, TestRange<T>& r, size_t param_idx) const {
        *t = r.getEntry(param_idx);
    }
};

template<std::size_t I = 0,
         typename FuncT,
         typename... Tp>
inline typename std::enable_if<I == sizeof...(Tp), void>::type
TestArgsSetParamScan(int,
                     std::tuple<Tp*...> &,
                     std::tuple<TestRange<Tp>...> &,
                     FuncT,
                     size_t) { }

template<std::size_t I = 0,
         typename FuncT,
         typename... Tp>
inline typename std::enable_if<I < sizeof...(Tp), void>::type
TestArgsSetParamScan(int index,
                     std::tuple<Tp*...>& t,
                     std::tuple<TestRange<Tp>...>& r,
                     FuncT f,
                     size_t param_idx)
{
    if (index == 0) f(std::get<I>(t), std::get<I>(r), param_idx);
    TestArgsSetParamScan<I + 1, FuncT, Tp...>(index-1, t, r, f, param_idx);
}
struct TestArgsGetNumStepsFunctor
{
    template<typename T>
    void operator()(T* t, TestRange<T>& r, size_t& steps_ret) const {
        (void)t;
        steps_ret = r.getSteps();
    }
};

template<std::size_t I = 0,
         typename FuncT,
         typename... Tp>
inline typename std::enable_if<I == sizeof...(Tp), void>::type
TestArgsGetStepsScan(int,
                     std::tuple<Tp*...> &,
                     std::tuple<TestRange<Tp>...> &,
                     FuncT,
                     size_t) { }

template<std::size_t I = 0,
         typename FuncT,
         typename... Tp>
inline typename std::enable_if<I < sizeof...(Tp), void>::type
TestArgsGetStepsScan(int index,
                     std::tuple<Tp*...>& t,
                     std::tuple<TestRange<Tp>...>& r,
                     FuncT f,
                     size_t& steps_ret)
{
    if (index == 0) f(std::get<I>(t), std::get<I>(r), steps_ret);
    TestArgsGetStepsScan<I + 1, FuncT, Tp...>(index-1, t, r, f, steps_ret);
}

#define TEST_ARGS_CONTENTS() \
    void setParam(size_t param_no, size_t param_idx) { \
        TestArgsSetParamScan(param_no, args, ranges, \
                             TestArgsSetParamFunctor(),\
                             param_idx); } \
    size_t getNumSteps(size_t param_no) { \
        size_t ret = 0; \
        TestArgsGetStepsScan(param_no, args, ranges, \
                             TestArgsGetNumStepsFunctor(), \
                             ret); \
        return ret; } \
    size_t getNumParams() { return std::tuple_size<decltype(args)>::value; }


class TestArgsBase {
public:
    virtual ~TestArgsBase() { }

    void setCallback(std::string test_name,
                     test_func_args *func,
                     TestSuite* test_instance) {
        testName = test_name;
        testFunction = func;
        testInstance = test_instance;
    }

    void testAll() { testAllInternal(0); }

    virtual void setParam(size_t param_no, size_t param_idx) = 0;
    virtual size_t getNumSteps(size_t param_no) = 0;
    virtual size_t getNumParams() = 0;
    virtual std::string toString() { return ""; }

private:
    void testAllInternal(size_t depth) {
        size_t i;
        size_t n_params = getNumParams();
        size_t n_steps = getNumSteps(depth);

        for (i=0; i<n_steps; ++i) {
            setParam(depth, i);
            if (depth+1 < n_params) {
                testAllInternal(depth+1);
            } else {
                std::string test_name;
                std::string args_name = toString();
                if (!args_name.empty()) {
                    test_name = testName + " (" + toString() + ")";
                }
                testInstance->doTestCB(test_name,
                                       testFunction, this);
            }
        }
    }

    std::string testName;
    test_func_args *testFunction;
    TestSuite *testInstance;
};


