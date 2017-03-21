/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
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

#include "latency_collector.h"


static std::string usToString(uint64_t us) {
    std::stringstream ss;
    if (us < 1000) {
        // us
        ss << std::fixed << std::setprecision(0) << us << " us";
    } else if (us < 1000000) {
        // ms
        double tmp = static_cast<double>(us / 1000.0);
        ss << std::fixed << std::setprecision(1) << tmp << " ms";
    } else if (us < (uint64_t)600 * 1000000) {
        // second (from 1 second to 10 mins)
        double tmp = static_cast<double>(us / 1000000.0);
        ss << std::fixed << std::setprecision(1) << tmp << " s";
    } else {
        // minute
        double tmp = static_cast<double>(us / 60.0 / 1000000.0);
        ss << std::fixed << std::setprecision(0) << tmp << " m";
    }
    return ss.str();
}

static std::string countToString(uint64_t count) {
    std::stringstream ss;
    if (count < 1000) {
        ss << count;
    } else if (count < 1000000) {
        // K
        double tmp = static_cast<double>(count / 1000.0);
        ss << std::fixed << std::setprecision(1) << tmp << "K";
    } else if (count < (uint64_t)1000000000) {
        // M
        double tmp = static_cast<double>(count / 1000000.0);
        ss << std::fixed << std::setprecision(1) << tmp << "M";
    } else {
        // B
        double tmp = static_cast<double>(count / 1000000000.0);
        ss << std::fixed << std::setprecision(1) << tmp << "B";
    }
    return ss.str();
}

std::string LatencyItem::dump(size_t max_filename_field) {
    if (!max_filename_field) {
        max_filename_field = 32;
    }
    std::stringstream ss;
    ss << std::left << std::setw(max_filename_field) << statName << ": ";
    ss << std::right;
    ss << std::setw(8) << usToString(getTotalTime()) << ", ";
    ss << std::setw(6) << countToString(getNumCalls()) << ", ";
    ss << std::setw(8) << usToString(getAvgLatency()) << " (";
    ss << std::setw(8) << usToString(getMinLatency()) << " ";
    ss << std::setw(8) << usToString(getMaxLatency()) << ")";
    return ss.str();
}


std::string MapWrapper::dump(LatencyCollectorDumpOptions opt) {
    std::stringstream ss;
    ss << "# stats: " << getSize() << std::endl;
    if (!getSize()) {
        return ss.str();
    }

    std::multimap<uint64_t, LatencyItem*, std::greater<uint64_t>>
            map_uint64_t;
    std::map<std::string, LatencyItem*> map_string;

    size_t max_name_len = 9; // reserved for "STAT NAME" 9 chars
    for (auto& entry: map) {
        LatencyItem *item = entry.second;
        if (!item->getNumCalls()) {
            continue;
        }

        switch (opt.sort_by) {
        case LatencyCollectorDumpSortBy::NAME:
            map_string.insert( std::make_pair(item->getName(), item));
            break;

        case LatencyCollectorDumpSortBy::TOTAL_TIME:
            map_uint64_t.insert( std::make_pair(item->getTotalTime(), item));
            break;

        case LatencyCollectorDumpSortBy::NUM_CALLS:
            map_uint64_t.insert( std::make_pair(item->getNumCalls(), item));
            break;

        case LatencyCollectorDumpSortBy::AVG_LATENCY:
            map_uint64_t.insert( std::make_pair(item->getAvgLatency(), item));
            break;
        }

        if (item->getName().size() > max_name_len) {
            max_name_len = item->getName().size();
        }
    }

    ss << std::left << std::setw(max_name_len) << "STAT NAME" << ": ";
    ss << std::right;
    ss << std::setw(8) << "TOTAL" << ", ";
    ss << std::setw(6) << "CALLS" << ", ";
    ss << std::setw(8) << "AVERAGE" << " (";
    ss << std::setw(8) << "MIN" << " ";
    ss << std::setw(8) << "MAX" << ")";
    ss << std::endl;

    if (map_string.size()) {
        for (auto& entry: map_string) {
            LatencyItem *item = entry.second;
            if (item->getNumCalls()) {
                ss << item->dump(max_name_len)
                   << std::endl;
            }
        }
    } else {
        for (auto& entry: map_uint64_t) {
            LatencyItem *item = entry.second;
            if (item->getNumCalls()) {
                ss << item->dump(max_name_len)
                   << std::endl;
            }
        }
    }

    return ss.str();
}



