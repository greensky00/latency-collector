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

#include <stdint.h>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <ctime>
#include <string>
#include <memory>

struct LatencyBin {
    LatencyBin() : latSum(0), latNum(0) { }

    std::atomic<uint64_t> latSum;
    std::atomic<uint64_t> latNum;
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
    }

    uint64_t getAvgLatency() const {
        return bin.latSum / bin.latNum;
    }

    uint64_t getTotalTime() const {
        return bin.latSum;
    }

    uint64_t getNumCalls() const {
        return bin.latNum;
    }

    std::string dump() {
        std::string str = statName;
        str += ": ";
        str += std::to_string(bin.latSum) + ", ";
        str += std::to_string(bin.latNum) + ", ";
        str += std::to_string(getAvgLatency());
        return str;
    }

private:
    std::string statName;
    LatencyBin bin;
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
        return map.size();
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

    std::string dump() {
        std::string str;
        str += "# stats: " + std::to_string(map.size()) + '\n';
        for (auto& entry: map) {
            LatencyItem *item = entry.second;
            str += item->dump() + "\n";
        }
        return str;
    }

    void freeAllItems() {
        for (auto& entry : map) {
            delete entry.second;
        }
    }

private:
    std::unordered_map<std::string, LatencyItem*> map;
};

using MapWrapperPtr = std::shared_ptr<MapWrapper>;
//using MapWrapperPtr = MapWrapper*; // for debugging

class LatencyCollector {
public:
    LatencyCollector() {
        latestMap = MapWrapperPtr(new MapWrapper());
    }

    ~LatencyCollector() {
        latestMap->freeAllItems();
    }

    size_t getNumItems() const {
        return latestMap->getSize();
    }

    void addStatName(std::string lat_name) {
        MapWrapperPtr cur_map = latestMap;
        if (!cur_map->get(lat_name)) {
            cur_map->addNew(lat_name);
        } // Otherwise: already exists.
    }

    void addLatency(std::string lat_name, uint64_t lat_value) {
        MapWrapperPtr cur_map = nullptr;

        size_t ticks_allowed = 64;
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
            // Once all stats are populated in the map, below codes will never be
            // called, and adding new latency will be done in a lock-free manner.

            // Copy from the current map.
            std::shared_ptr<MapWrapper> new_map(new MapWrapper(*cur_map));
            // Add a new item.
            item = new_map->addNew(lat_name);
            item->addLatency(lat_value);

            // Atomic CAS
            //
            // Note:
            // * according to C++11 standard, we should be able to use
            //   `atomic_compare_exchange_...` here, but it is mistakenly
            //   omitted in stdc++ library. Until it is fixed, we need to use
            //   mutex.
            // * load/store shared_ptr is atomic. Don't need to think about
            //   readers.

            /*
            // == Original code using atomic operation:
            std::shared_ptr<MapWrapper> expected = cur_map;
            if (std::atomic_compare_exchange_strong(
                        &latestMap, &expected, new_map)) {
                // Succeeded.
                return;
            }
            */

            // == Alternative using mutex
            {
                std::lock_guard<std::mutex> l(lock);
                if (latestMap == cur_map) {
                    latestMap = new_map;
                    return;
                }
            }

            // Failed, other thread updated the map at the same time.
            // Retry.
        } while (ticks_allowed--);

        // Update failed, ignore the given latency at this time.
    }

    uint64_t getAvgLatency(std::string lat_name) {
        MapWrapperPtr cur_map = latestMap;
        LatencyItem *item = cur_map->get(lat_name);
        return (item)? item->getAvgLatency() : 0;
    }

    uint64_t getTotalTime(std::string lat_name) {
        MapWrapperPtr cur_map = latestMap;
        LatencyItem *item = cur_map->get(lat_name);
        return (item)? item->getTotalTime() : 0;
    }

    uint64_t getNumCalls(std::string lat_name) {
        MapWrapperPtr cur_map = latestMap;
        LatencyItem *item = cur_map->get(lat_name);
        return (item)? item->getNumCalls() : 0;
    }

    std::string dump() {
        std::string str;
        MapWrapperPtr cur_map = latestMap;
        str = cur_map->dump();
        return str;
    }

private:
    // Mutex for Compare-And-Swap of latestMap.
    std::mutex lock;
    std::shared_ptr<MapWrapper> latestMap;
};


struct LatencyCollectWrapper {
    LatencyCollectWrapper(LatencyCollector *_lat,
                          std::string _func_name) {
        lat = _lat;
        if (lat) {
            start = std::chrono::system_clock::now();
            functionName = _func_name;
        }
    }

    ~LatencyCollectWrapper() {
        if (lat) {
            std::chrono::time_point<std::chrono::system_clock> end;
            end = std::chrono::system_clock::now();

            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                    end - start);
            lat->addLatency(functionName, us.count());
        }
    }

    std::string functionName;
    LatencyCollector *lat;
    std::chrono::time_point<std::chrono::system_clock> start;
};

#define collectFuncLatency(lat) \
    LatencyCollectWrapper __func_latency__((lat), __func__)

#define collectBlockLatency(lat, name) \
    LatencyCollectWrapper __block_latency__((lat), name)

