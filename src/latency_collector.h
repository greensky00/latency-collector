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

#include <inttypes.h>
#include <stdint.h>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

struct LatencyBin {
    LatencyBin()
        : latSum(0),
          latNum(0),
          latMax(0),
          latMin(std::numeric_limits<uint64_t>::max()) {
    }
    std::atomic<uint64_t> latSum;
    std::atomic<uint64_t> latNum;
    std::atomic<uint64_t> latMax;
    std::atomic<uint64_t> latMin;
};

class LatencyItem {
public:
    LatencyItem(std::string _name) : statName(_name) {
    }
    ~LatencyItem() {
    }

    std::string getName() const {
        return statName;
    }

    void addLatency(uint64_t latency) {
        bin.latSum.fetch_add(latency, std::memory_order_relaxed);
        bin.latNum.fetch_add(1, std::memory_order_relaxed);

        int retry = MAX_STAT_UPDATE_RETRIES;
        do {
            uint64_t lat_max = bin.latMax.load(std::memory_order_relaxed);
            if (lat_max < latency) {
                if (!bin.latMax.compare_exchange_weak(lat_max, latency)) {
                    continue;
                }
            }
            break;
        } while (--retry);

        retry = MAX_STAT_UPDATE_RETRIES;
        do {
            uint64_t lat_min = bin.latMin.load(std::memory_order_relaxed);
            if (lat_min > latency) {
                if (!bin.latMin.compare_exchange_weak(lat_min, latency)) {
                    continue;
                }
            }
            break;
        } while (--retry);
    }

    uint64_t getAvgLatency() const {
        return (bin.latNum) ? (bin.latSum / bin.latNum) : 0;
    }

    uint64_t getTotalTime() const {
        return bin.latSum;
    }

    uint64_t getNumCalls() const {
        return bin.latNum;
    }

    uint64_t getMaxLatency() const {
        return bin.latMax;
    }

    uint64_t getMinLatency() const {
        return bin.latMin;
    }

    std::string dump(size_t max_filename_field = 0);

private:
    static const size_t MAX_STAT_UPDATE_RETRIES = 16;
    std::string statName;
    LatencyBin bin;
};

enum class LatencyCollectorDumpSortBy {
    NAME,
    TOTAL_TIME,
    NUM_CALLS,
    AVG_LATENCY
};

struct LatencyCollectorDumpOptions {
    LatencyCollectorDumpOptions()
        : sort_by(LatencyCollectorDumpSortBy::AVG_LATENCY) {
    }
    LatencyCollectorDumpSortBy sort_by;
};

class MapWrapper {
public:
    MapWrapper() {
    }

    MapWrapper(const MapWrapper &src) {
        copyFrom(src);
    }

    ~MapWrapper() {
    }

    size_t getSize() const {
        size_t ret = 0;
        for (auto& entry: map) {
            if (entry.second->getNumCalls()) {
                ret++;
            }
        }
        return ret;
    }

    void copyFrom(const MapWrapper &src) {
        // Make a clone (but the map will point to same LatencyItems)
        map = src.map;
    }

    LatencyItem* addNew(std::string bin_name) {
        LatencyItem* item = new LatencyItem(bin_name);
        map.insert( std::make_pair(bin_name, item) );
        return item;
    }

    LatencyItem* get(std::string bin_name) {
        LatencyItem* item = nullptr;
        auto entry = map.find(bin_name);
        if (entry != map.end()) {
            item = entry->second;
        }
        return item;
    }

    std::string dump(LatencyCollectorDumpOptions opt);

    void freeAllItems() {
        for (auto& entry : map) {
            delete entry.second;
        }
    }

private:
    std::unordered_map<std::string, LatencyItem*> map;
};

using MapWrapperSharedPtr = std::shared_ptr<MapWrapper>;

class LatencyCollector {
public:
    LatencyCollector() {
        latestMap = MapWrapperSharedPtr(new MapWrapper());
    }

    ~LatencyCollector() {
        latestMap->freeAllItems();
    }

    size_t getNumItems() const {
        return latestMap->getSize();
    }

    void addStatName(std::string lat_name) {
        MapWrapperSharedPtr cur_map = latestMap;
        if (!cur_map->get(lat_name)) {
            cur_map->addNew(lat_name);
        } // Otherwise: already exists.
    }

    void addLatency(std::string lat_name, uint64_t lat_value) {
        MapWrapperSharedPtr cur_map = nullptr;

        size_t ticks_allowed = MAX_ADD_NEW_ITEM_RETRIES;
        do {
            cur_map = latestMap;
            LatencyItem *item = cur_map->get(lat_name);
            if (item) {
                // Found existing latency.
                item->addLatency(lat_value);
                return;
            }

            // Not found,
            // 1) Create a new map containing new stat in an MVCC manner, and
            // 2) Replace 'latestMap' pointer atomically.

            // Note:
            // Below insertion process happens only when a new stat item
            // is added. Generally the number of stats is not pretty big (<100),
            // and adding new stats will be finished at the very early stage.
            // Once all stats are populated in the map, below codes will never
            // be called, and adding new latency will be done without blocking
            // anything.

            // Copy from the current map.
            MapWrapperSharedPtr new_map = std::make_shared<MapWrapper>(*cur_map);

            // Add a new item.
            item = new_map->addNew(lat_name);
            item->addLatency(lat_value);

            // Atomic CAS, from current map to new map
            //
            // Note:
            // * according to C++11 standard, we should be able to use
            //   `atomic_compare_exchange_...` here, but it is mistakenly
            //   omitted in stdc++ library. We need to use mutex instead
            //   until it is fixed.
            // * load/store shared_ptr is atomic. Don't need to think about
            //   readers.

            /*
            // == Original code using atomic operation:
            MapWrapperSharedPtr expected = cur_map;
            if (std::atomic_compare_exchange_strong(
                        &latestMap, &expected, new_map)) {
                // Succeeded.
                return;
            }
            */

            // == Alternative code using mutex
            {
                std::lock_guard<std::mutex> l(lock);
                if (latestMap == cur_map) {
                    latestMap = new_map;
                    // Succeeded.
                    return;
                }
            }

            // Failed, other thread updated the map at the same time.
            // Retry.
        } while (ticks_allowed--);

        // Update failed, ignore the given latency at this time.
    }

    uint64_t getAvgLatency(std::string lat_name) {
        MapWrapperSharedPtr cur_map = latestMap;
        LatencyItem *item = cur_map->get(lat_name);
        return (item)? item->getAvgLatency() : 0;
    }

    uint64_t getMinLatency(std::string lat_name) {
        MapWrapperSharedPtr cur_map = latestMap;
        LatencyItem *item = cur_map->get(lat_name);
        return (item && item->getNumCalls()) ? item->getMinLatency() : 0;
    }

    uint64_t getMaxLatency(std::string lat_name) {
        MapWrapperSharedPtr cur_map = latestMap;
        LatencyItem *item = cur_map->get(lat_name);
        return (item) ? item->getMaxLatency() : 0;
    }

    uint64_t getTotalTime(std::string lat_name) {
        MapWrapperSharedPtr cur_map = latestMap;
        LatencyItem *item = cur_map->get(lat_name);
        return (item) ? item->getTotalTime() : 0;
    }

    uint64_t getNumCalls(std::string lat_name) {
        MapWrapperSharedPtr cur_map = latestMap;
        LatencyItem *item = cur_map->get(lat_name);
        return (item) ? item->getNumCalls() : 0;
    }

    std::string dump(
            LatencyCollectorDumpOptions opt = LatencyCollectorDumpOptions()) {
        std::string str;
        MapWrapperSharedPtr cur_map = latestMap;
        str = cur_map->dump(opt);
        return str;
    }

private:
    static const size_t MAX_ADD_NEW_ITEM_RETRIES = 16;
    // Mutex for Compare-And-Swap of latestMap.
    std::mutex lock;
    MapWrapperSharedPtr latestMap;
};

struct ThreadTrackerItem {
    ThreadTrackerItem() : numStacks(0) {}

    void pushStackName(std::string cur_stack_name) {
        aggrStackName += " ## ";
        aggrStackName += cur_stack_name;
        numStacks++;
    }

    size_t popLastStack() {
        if (--numStacks == 0) {
            aggrStackName.clear();
            return numStacks;
        }
        size_t n = aggrStackName.rfind(" ## ");
        aggrStackName = aggrStackName.substr(0, n);
        return numStacks;
    }

    std::string getAggrStackName() const { return aggrStackName; }

    size_t numStacks;
    std::string aggrStackName;
};

struct LatencyCollectWrapper {
    LatencyCollectWrapper(LatencyCollector *_lat,
                          std::string _func_name) {
        lat = _lat;
        if (lat) {
            start = std::chrono::system_clock::now();

            thread_local ThreadTrackerItem thr_item;
            cur_tracker = &thr_item;
            cur_tracker->pushStackName(_func_name);
        }
    }

    ~LatencyCollectWrapper() {
        if (lat) {
            std::chrono::time_point<std::chrono::system_clock> end;
            end = std::chrono::system_clock::now();

            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                    end - start);
            lat->addLatency(cur_tracker->getAggrStackName(), us.count());
            cur_tracker->popLastStack();
        }
    }

    LatencyCollector *lat;
    ThreadTrackerItem *cur_tracker;
    std::chrono::time_point<std::chrono::system_clock> start;
};

#if defined(WIN32) || defined(_WIN32)
#define collectFuncLatency(lat) \
    LatencyCollectWrapper __func_latency__((lat), __FUNCTION__)
#else
#define collectFuncLatency(lat) \
    LatencyCollectWrapper __func_latency__((lat), __func__)
#endif

#define collectBlockLatency(lat, name) \
    LatencyCollectWrapper __block_latency__((lat), name)

