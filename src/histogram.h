/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Histogram
 * Version: 0.1.5
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <stdint.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>

using HistBin = std::atomic<uint64_t>;

class Histogram;
class HistItr {
public:
    HistItr() : idx(0), maxBins(0), owner(nullptr) { }

    HistItr(size_t _idx, size_t _max_bins, Histogram* _owner)
        : idx(_idx), maxBins(_max_bins), owner(_owner) {}

    // ++A
    HistItr& operator++() {
        idx++;
        if (idx > maxBins) idx = maxBins;
        return *this;
    }

    // A++
    HistItr operator++(int) {
        idx++;
        if (idx > maxBins) idx = maxBins;
        return *this;
    }

    // --A
    HistItr& operator--() {
        if (idx || idx == maxBins) {
            // end()
            idx = maxBins;
        } else {
            idx--;
        }
        return *this;
    }

    // A--
    HistItr operator--(int) {
        if (idx || idx == maxBins) {
            // end()
            idx = maxBins;
        } else {
            idx--;
        }
        return *this;
    }

    HistItr& operator*() {
        // Just return itself
        return *this;
    }


    bool operator==(const HistItr& val) const {
        return (idx == val.idx);
    }

    bool operator!=(const HistItr& val) const {
        return (idx != val.idx);
    }

    size_t getIdx() const { return idx; }

    inline uint64_t getCount();

    uint64_t getLowerBound() {
        size_t idx_rev = maxBins - idx - 1;
        uint64_t ret = 1;

        if (idx_rev) {
            return ret << (idx_rev-1);
        } else {
            return 0;
        }
    }

    uint64_t getUpperBound() {
        size_t idx_rev = maxBins - idx - 1;
        uint64_t ret = 1;

        if (!idx) return std::numeric_limits<std::uint64_t>::max();
        return ret << idx_rev;
    }

private:
    size_t idx;
    size_t maxBins;
    Histogram* owner;
};

class Histogram {
    friend class HistItr;

public:
    using iterator = HistItr;

    Histogram() : count(0), sum(0), max(0) {
        bins = new HistBin[maxBins];
        for (size_t i=0; i<maxBins; ++i) {
            bins[i] = 0;
        }
    }

    Histogram(const Histogram& src) {
        bins = new HistBin[maxBins];
        *this = src;
    }

    ~Histogram() {
        delete[] bins;
    }

    // this = src
    Histogram& operator=(const Histogram& src) {
        count = src.getTotal();
        sum = src.getSum();
        max = src.getMax();
        for (size_t i=0; i<maxBins; ++i) {
            bins[i] += src.bins[i];
        }
        return *this;
    }

    // this += rhs
    Histogram& operator+=(const Histogram& rhs) {
        count += rhs.getTotal();
        sum += rhs.getSum();
        if (max < rhs.getMax()) {
            max = rhs.getMax();
        }

        for (size_t i=0; i<maxBins; ++i) {
            bins[i] += rhs.bins[i];
        }

        return *this;
    }

    // returning lhs + rhs
    friend Histogram operator+(Histogram lhs,
                               const Histogram& rhs) {
        lhs.count += rhs.getTotal();
        lhs.sum += rhs.getSum();
        if (lhs.max < rhs.getMax()) {
            lhs.max = rhs.getMax();
        }

        for (size_t i=0; i<maxBins; ++i) {
            lhs.bins[i] += rhs.bins[i];
        }

        return lhs;
    }

    void add(uint64_t val) {
        // if `val` == 1
        //          == 0x00...01
        //                     ^
        //                     64th bit
        //   then `idx` = 63.
        //
        // if `val` == UINT64_MAX
        //          == 0xff...ff
        //               ^
        //               1st bit
        //   then `idx` = 0.
        //
        // so we should handle `val` == 0 as a special case (`idx` = 64),
        // that's the reason why num bins is 65.

        int idx = maxBins - 1;
        if (val) {
            idx = __builtin_clzl(val);
        }
        bins[idx].fetch_add(1, std::memory_order_relaxed);
        count.fetch_add(1, std::memory_order_relaxed);
        sum.fetch_add(val, std::memory_order_relaxed);

        size_t num_trial = 0;
        while (num_trial++ < max_trial &&
               max.load(std::memory_order_relaxed) < val) {
            // 'max' may not be updated properly under race condition.
            max.store(val, std::memory_order_relaxed);
        }
    }

    uint64_t getTotal() const { return count; }
    uint64_t getSum() const { return sum; }
    uint64_t getAverage() const { return ( (count) ? (sum / count) : 0 ); }
    uint64_t getMax() const { return max; }

    iterator find(double percentile) {
        if (percentile <= 0 || percentile >= 100) {
            return end();
        }

        double rev = 100 - percentile;
        size_t i;
        uint64_t sum = 0;
        uint64_t total = getTotal();
        uint64_t threshold = (double)total * rev / 100.0;

        for (i=0; i<maxBins; ++i) {
            sum += bins[i].load(std::memory_order_relaxed);
            if (sum >= threshold) {
                return HistItr(i, maxBins, this);
            }
        }
        return end();
    }

    uint64_t estimate(double percentile) {
        if (percentile <= 0 || percentile >= 100) {
            return 0;
        }

        double rev = 100 - percentile;
        size_t i;
        uint64_t sum = 0;
        uint64_t total = getTotal();
        uint64_t threshold = (double)total * rev / 100.0;

        if (!threshold) {
            // No samples between the given percentile and the max number.
            // Return max number.
            return max;
        }

        for (i=0; i<maxBins; ++i) {
            uint64_t n_entries = bins[i].load(std::memory_order_relaxed);
            sum += n_entries;
            if (sum < threshold) continue;

            uint64_t gap = sum - threshold;
            uint64_t u_bound = HistItr(i, maxBins, this).getUpperBound();
            double base = 2.0;
            if (max < u_bound) {
                base = (double)max / (u_bound / 2.0);
            }

            return std::pow(base, (double)gap / n_entries) * u_bound / 2;
        }
        return 0;
    }

    iterator begin() {
        size_t i;
        for (i=0; i<maxBins; ++i) {
            if (bins[i].load(std::memory_order_relaxed)) break;
        }
        return HistItr(i, maxBins, this);
    }

    iterator end() {
        return HistItr(maxBins, maxBins, this);
    }

private:
    static const size_t maxBins = 65;
    static const size_t max_trial = 3;

    HistBin* bins;
    std::atomic<uint64_t> count;
    std::atomic<uint64_t> sum;
    std::atomic<uint64_t> max;
};

uint64_t HistItr::getCount() {
    return owner->bins[idx];
}

