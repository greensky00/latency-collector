
#include <inttypes.h>
#include <stdint.h>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

struct LatencyBin {
    LatencyBin()
        : latSum(0),
          latNum(0),
          latMax(0),
          latMin(std::numeric_limits<uint64_t>::max()) {}
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
                if (!bin.latMax.compare_exchange_strong(lat_max, latency)) {
                    continue;
                }
            }
            break;
        } while (--retry);

        retry = MAX_STAT_UPDATE_RETRIES;
        do {
            uint64_t lat_min = bin.latMin.load(std::memory_order_relaxed);
            if (lat_min > latency) {
                if (!bin.latMin.compare_exchange_strong(lat_min, latency)) {
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

    std::string dump(size_t max_filename_field = 0) {
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
            // min
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

    std::string dump(LatencyCollectorDumpOptions opt) {
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

    void freeAllItems() {
        for (auto& entry : map) {
            delete entry.second;
        }
    }

private:
    std::unordered_map<std::string, LatencyItem*> map;
};

using MapWrapperSharedPtr = std::shared_ptr<MapWrapper>;
//using MapWrapperSharedPtr = MapWrapper*; // for debugging

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

#if defined(WIN32) || defined(_WIN32)
#define collectFuncLatency(lat) \
    LatencyCollectWrapper __func_latency__((lat), __FUNCTION__)
#else
#define collectFuncLatency(lat) \
    LatencyCollectWrapper __func_latency__((lat), __func__)
#endif

#define collectBlockLatency(lat, name) \
    LatencyCollectWrapper __block_latency__((lat), name)

