/**
 * Copyright (C) 2017-present Jung-Sang Ahn <jungsang.ahn@gmail.com>
 * All rights reserved.
 *
 * https://github.com/greensky00
 *
 * Latency Collector Dump Module
 * Version: 0.1.3
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

#include <list>
#include <memory>
#include <vector>

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

static std::string ratioToPercent(uint64_t a, uint64_t b) {
    std::stringstream ss;
    double tmp = (double)100.0 * a / b;
    ss << std::fixed << std::setprecision(1) << tmp << " %";
    return ss.str();
}

size_t getNumStacks(std::string& str) {
    size_t pos = 0;
    size_t str_size = str.size();
    size_t ret = 0;
    while (pos < str_size) {
        pos = str.find(" ## ", pos);
        if (pos == std::string::npos) break;
        pos += 4;
        ret++;
    }
    return ret;
}

std::string getActualFunction(std::string str,
                              bool add_tab = true) {
    size_t level = getNumStacks(str);
    if (!level) {
        return str;
    }

    size_t pos = str.rfind(" ## ");
    std::string ret = "";
    if (level > 1 && add_tab) {
        for (size_t i=1; i<level; ++i) {
            ret += "  ";
        }
    }
    ret += str.substr(pos + 4);
    return ret;
}

std::string LatencyItem::dump(size_t max_filename_field,
                              uint64_t parent_total_time,
                              bool add_tab) {
    if (!max_filename_field) {
        max_filename_field = 32;
    }
    std::stringstream ss;
    ss << std::left << std::setw(max_filename_field)
       << getActualFunction(statName, add_tab) << ": ";
    ss << std::right;
    ss << std::setw(8) << usToString(getTotalTime()) << " ";
    if (parent_total_time) {
        ss << std::setw(7)
           << ratioToPercent(getTotalTime(), parent_total_time)
           << " ";
    } else {
        ss << "    ---" << " ";
    }
    ss << std::setw(6) << countToString(getNumCalls()) << " ";
    ss << std::setw(8) << usToString(getAvgLatency()) << " ";
    ss << std::setw(8) << usToString(getPercentile(50)) << " ";
    ss << std::setw(8) << usToString(getPercentile(99)) << " ";
    ss << std::setw(8) << usToString(getPercentile(99.9));
    return ss.str();
}

struct DumpItem {
    using UPtr = std::unique_ptr<DumpItem>;

    DumpItem() : level(0), itself(nullptr), parent(nullptr) {}
    DumpItem(size_t _level, LatencyItem* _item, LatencyItem* _parent)
        : level(_level),
          itself(_item),
          parent(_parent) {}

    size_t level;
    LatencyItem* itself;
    LatencyItem* parent;
    std::list<UPtr> child;
};
using DumpItemP = DumpItem::UPtr;

void dumpRecursive(std::stringstream& ss,
                   DumpItem* dump_item,
                   size_t max_name_len) {
    if (dump_item->itself) {
        if (dump_item->parent) {
            ss << dump_item->itself->dump(max_name_len,
                                          dump_item->parent->getTotalTime());
        } else {
            ss << dump_item->itself->dump(max_name_len);
        }
        ss << std::endl;
    }
    for (auto& entry : dump_item->child) {
        DumpItem* child = entry.get();
        dumpRecursive(ss, child, max_name_len);
    }
}

void addDumpTitle(std::stringstream& ss, size_t max_name_len) {
    ss << std::left << std::setw(max_name_len) << "STAT NAME" << ": ";
    ss << std::right;
    ss << std::setw(8) << "TOTAL" << " ";
    ss << std::setw(7) << "RATIO" << " ";
    ss << std::setw(6) << "CALLS" << " ";
    ss << std::setw(8) << "AVERAGE" << " ";
    ss << std::setw(8) << "p50" << " ";
    ss << std::setw(8) << "p99" << " ";
    ss << std::setw(8) << "p99.9";
    ss << std::endl;
}

void addToUintMap(uint64_t value,
                    std::multimap<uint64_t,
                                  LatencyItem*,
                                  std::greater<uint64_t> >& map,
                    LatencyItem* item) {
    auto entry = map.find(value);
    if (entry != map.end()) {
        LatencyItem* item_found = entry->second;
        *item_found += *item;
        return;
    }
    map.insert( std::make_pair(value, item) );
}

std::string MapWrapper::dumpTree(LatencyCollectorDumpOptions opt) {
    std::stringstream ss;
    DumpItem root;

    // Sort by name first.
    std::map<std::string, LatencyItem*> by_name;
    for (auto& entry : map) {
        LatencyItem *item = entry.second;
        by_name.insert( std::make_pair(item->getName(), item) );
    }

    size_t max_name_len = 9;
    std::vector<DumpItem*> last_ptr(1);
    last_ptr[0] = &root;
    for (auto& entry : by_name) {
        LatencyItem *item = entry.second;
        std::string item_name = item->getName();
        size_t level = getNumStacks(item_name);
        if (!level) {
            // Not a thread-aware latency item, stop.
            return dump(opt);
        }

        DumpItem* parent = last_ptr[level-1];
        assert(parent); // Must exist

        DumpItemP dump_item(new DumpItem(level, item, parent->itself));
        if (level >= last_ptr.size()) {
            last_ptr.resize(level*2);
        }
        last_ptr[level] = dump_item.get();
        parent->child.push_back(std::move(dump_item));

        size_t actual_name_len = getActualFunction(item_name).size();
        if (actual_name_len > max_name_len) {
            max_name_len = actual_name_len;
        }
    }

    addDumpTitle(ss, max_name_len);
    dumpRecursive(ss, &root, max_name_len);

    return ss.str();
}

std::string MapWrapper::dump(LatencyCollectorDumpOptions opt) {
    std::stringstream ss;
    if (!getSize()) {
        ss << "# stats: " << getSize() << std::endl;
        return ss.str();
    }

    std::multimap<uint64_t,
                  LatencyItem*,
                  std::greater<uint64_t> > map_uint64_t;
    std::map<std::string, LatencyItem*> map_string;
    size_t max_name_len = 9; // reserved for "STAT NAME" 9 chars

    // Deduplication
    for (auto& entry: map) {
        LatencyItem *item = entry.second;
        if (!item->getNumCalls()) {
            continue;
        }
        std::string actual_name = getActualFunction(item->getName(), false);

        auto existing = map_string.find(actual_name);
        if (existing != map_string.end()) {
            LatencyItem* item_found = existing->second;
            *item_found += *item;
        } else {
            map_string.insert( std::make_pair(actual_name, item) );
        }

        if (actual_name.size() > max_name_len) {
            max_name_len = actual_name.size();
        }
    }

    ss << "# stats: " << map_string.size() << std::endl;

    for (auto& entry: map_string) {
        LatencyItem *item = entry.second;
        if (!item->getNumCalls()) {
            continue;
        }

        switch (opt.sort_by) {
        case LatencyCollectorDumpOptions::SortBy::NAME: {
            // Do nothing
            break;
        }

        // Otherwise: dealing with uint64_t, map_uint64_t.
        case LatencyCollectorDumpOptions::SortBy::TOTAL_TIME:
            addToUintMap(item->getTotalTime(), map_uint64_t, item);
            break;

        case LatencyCollectorDumpOptions::SortBy::NUM_CALLS:
            addToUintMap(item->getNumCalls(), map_uint64_t, item);
            break;

        case LatencyCollectorDumpOptions::SortBy::AVG_LATENCY:
            addToUintMap(item->getAvgLatency(), map_uint64_t, item);
            break;
        }
    }

    addDumpTitle(ss, max_name_len);

    if (opt.sort_by == LatencyCollectorDumpOptions::SortBy::NAME) {
        // Name (string)
        for (auto& entry: map_string) {
            LatencyItem *item = entry.second;
            if (item->getNumCalls()) {
                ss << item->dump(max_name_len, 0, false)
                   << std::endl;
            }
        }
    } else {
        // Otherwise (number)
        for (auto& entry: map_uint64_t) {
            LatencyItem *item = entry.second;
            if (item->getNumCalls()) {
                ss << item->dump(max_name_len, 0, false)
                   << std::endl;
            }
        }
    }

    return ss.str();
}

std::string LatencyCollector::dump(LatencyCollectorDumpOptions opt) {
    std::string str;
    MapWrapperSP cur_map_p = latestMap;
    MapWrapper* cur_map = cur_map_p.get();

    if (opt.view_type == LatencyCollectorDumpOptions::ViewType::TREE) {
        str = cur_map->dumpTree(opt);
    } else {
        str = cur_map->dump(opt);
    }

    return str;
}



