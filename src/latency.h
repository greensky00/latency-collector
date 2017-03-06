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
#include <chrono>
#include <ctime>
#include <string>

struct LatencyBin {
    LatencyBin() : latSum(0), latNum(0) { }

    std::atomic<uint64_t> latSum;
    std::atomic<uint64_t> latNum;
};

class LatencyItem {
public:
    LatencyItem(std::string _name) : statName(_name) { }
    ~LatencyItem() { }

    std::string getName() const {
        return statName;
    }

    void addLatency(uint64_t latency) {
        bin.latSum.fetch_add(latency, std::memory_order_relaxed);
        bin.latNum.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t getAvgLatency() {
        return bin.latSum / bin.latNum;
    }

    uint64_t getTotalTime() {
        return bin.latSum;
    }

    uint64_t getNumCalls() {
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
    MapWrapper() : refCount(0), invalid(false), prevSnapshot(nullptr)
    { }

    MapWrapper(const MapWrapper &src) {
        copyFrom(src);
    }

    ~MapWrapper() { }

    size_t getSize() const {
        return map.size();
    }

    MapWrapper* getPrev() const {
        return const_cast<MapWrapper*>(prevSnapshot);
    }

    void setPrev(const MapWrapper *prev) {
        prevSnapshot = prev;
    }

    void clearPrev() {
        prevSnapshot = nullptr;
    }

    void operator=(const MapWrapper &src) {
        copyFrom(src);
    }

    void copyFrom(const MapWrapper &src) {
        // Make a clone (but the map will point to same LatencyItems)
        map = src.map;
        refCount.store(0);
        invalid.store(false);
        prevSnapshot = nullptr;
    }

    bool isRemovable() const {
        return (invalid && refCount == 0);
    }

    uint64_t incRC() {
        if (invalid) {
            return 0;
        }
        refCount.fetch_add(1, std::memory_order_relaxed);
        return refCount;
    }

    uint64_t decRC() {
        refCount.fetch_sub(1, std::memory_order_relaxed);
        return refCount;
    }

    uint64_t getRC() const {
        return refCount.load();
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

    void markInvalid() {
        invalid.store(true);
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
    std::atomic<uint64_t> refCount;

private:
    std::unordered_map<std::string, LatencyItem*> map;
    std::atomic<bool> invalid;
    const MapWrapper *prevSnapshot;
};


class LatencyCollector {
public:
    LatencyCollector() : gcInProgress(false) {
        latestMap.store(new MapWrapper(), std::memory_order_relaxed);
    }

    ~LatencyCollector() {
        MapWrapper *prev = nullptr;

        MapWrapper *cursor = latestMap.load();
        cursor->freeAllItems();

        while (cursor) {
            prev = cursor->getPrev();
            delete cursor;
            cursor = prev;
        }
    }

    size_t getNumItems() const {
        return latestMap.load()->getSize();
    }

    // This function MUST be serialized by lock.
    void addStatName(std::string lat_name) {
        MapWrapper *cur_map = latestMap.load();
        if (!cur_map->get(lat_name)) {
            cur_map->addNew(lat_name);
        } // Otherwise: already exists.
    }

    void addLatency(std::string lat_name, uint64_t lat_value) {
        MapWrapper *cur_map = nullptr;

        size_t ticks_allowed = 64;
        do {
            if ( !(cur_map = latestMap.load())->incRC() ) {
                // Old (invalid) map, retry.
                continue;
            }

            LatencyItem *item = cur_map->get(lat_name);
            if (item) {
                // Found existing latency.
                item->addLatency(lat_value);
                cur_map->decRC();
                return;
            }

            // Not found,
            // 1) Create a new map containing new stat in an MVCC manner, and
            // 2) Replace 'latestMap' pointer atomically.

            // Note:
            // Below insertion process happens only when a new stat
            // is added. Generally the number of stats is not quite big (<100),
            // insertions will be finished at the very early stage. Once
            // all stats are populated in the map, below codes will never be
            // called, and adding new latency will be done in a lock-free manner.

            // Copy from the current map.
            MapWrapper *new_map = new MapWrapper(*cur_map);
            // Add a new item.
            item = new_map->addNew(lat_name);
            item->addLatency(lat_value);

            // Atomic CAS
            MapWrapper *expected = cur_map;
            if (latestMap.compare_exchange_strong(expected, new_map)) {
                // Succeeded.
                cur_map->markInvalid();
                cur_map->decRC();
                new_map->setPrev(cur_map);
                garbageCollect();
                return;
            }

            // Failed, other thread updated the map at the same time.
            // Free and retry.
            delete new_map;
            cur_map->decRC();
        } while (ticks_allowed--);

        // Update failed, ignore the given latency at this time.
    }

    uint64_t getAvgLatency(std::string lat_name) {
        MapWrapper *cur_map = nullptr;

        (cur_map = latestMap.load())->incRC();
        LatencyItem *item = cur_map->get(lat_name);
        cur_map->decRC();

        if (item) {
            return item->getAvgLatency();
        }
        return 0;
    }

    uint64_t getTotalTime(std::string lat_name) {
        MapWrapper *cur_map = nullptr;

        (cur_map = latestMap.load())->incRC();
        LatencyItem *item = cur_map->get(lat_name);
        cur_map->decRC();

        if (item) {
            return item->getTotalTime();
        }
        return 0;
    }

    uint64_t getNumCalls(std::string lat_name) {
        MapWrapper *cur_map = nullptr;

        (cur_map = latestMap.load())->incRC();
        LatencyItem *item = cur_map->get(lat_name);
        cur_map->decRC();

        if (item) {
            return item->getNumCalls();
        }
        return 0;
    }

    std::string dump() {
        std::string str;
        MapWrapper *cur_map = nullptr;
        (cur_map = latestMap.load())->incRC();
        str = cur_map->dump();
        cur_map->decRC();
        return str;
    }

private:
    void garbageCollect() {
        bool expected = false;
        if (gcInProgress.compare_exchange_strong(expected, true)) {
            // Allow only one thread for GC at a time.
            GCRecursive(latestMap.load(), 0);
            gcInProgress.store(false);
        }
    }

    bool GCRecursive(MapWrapper* cursor, int depth) {
        if (!cursor) {
            return false;
        }

        MapWrapper* prev = cursor->getPrev();
        bool gc_cur_wrapper = true;

        if (prev) {
            if (GCRecursive(prev, depth+1)) {
                // Prev wrapper is reclaimed, clear the pointer.
                cursor->clearPrev();
            } else {
                // Prev wrapper is still alive, stop GC.
                return false;
            }
        }

        if (depth && gc_cur_wrapper && cursor->isRemovable()) {
            // Reclaim current wrapper if removable.
            delete cursor;
            return true;
        }
        return false;
    }

    std::atomic<MapWrapper*> latestMap;
    std::atomic<bool> gcInProgress;
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

            auto us = std::chrono::duration_cast<std::chrono::microseconds>
                      (end - start);
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

