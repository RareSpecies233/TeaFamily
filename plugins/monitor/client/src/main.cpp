/**
 * Monitor Plugin - Client Side (runs on HoneyTea)
 *
 * Collects local system metrics (CPU, memory, disk, temperature)
 * and sends them to the server-side plugin via parent stdio.
 *
 * Supports macOS and Linux.
 */

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <array>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#elif defined(__linux__)
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#endif

using json = nlohmann::json;

static std::mutex g_out_mutex;
static std::atomic<bool> g_collecting{false};
static std::atomic<int> g_interval_ms{1000};

static void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_out_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

static std::string execCommand(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

static double getCpuUsage() {
#ifdef __APPLE__
    static uint64_t prev_idle = 0, prev_total = 0;
    host_cpu_load_info_data_t cpuInfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        reinterpret_cast<host_info_t>(&cpuInfo), &count) != KERN_SUCCESS) {
        return -1;
    }
    uint64_t idle = cpuInfo.cpu_ticks[CPU_STATE_IDLE];
    uint64_t total = 0;
    for (int i = 0; i < CPU_STATE_MAX; i++) {
        total += cpuInfo.cpu_ticks[i];
    }
    uint64_t d_idle = idle - prev_idle;
    uint64_t d_total = total - prev_total;
    prev_idle = idle;
    prev_total = total;
    if (d_total == 0) return 0;
    return (1.0 - static_cast<double>(d_idle) / static_cast<double>(d_total)) * 100.0;
#elif defined(__linux__)
    static long prev_idle = 0, prev_total = 0;
    std::ifstream ifs("/proc/stat");
    std::string line;
    std::getline(ifs, line);
    std::istringstream iss(line);
    std::string cpu;
    iss >> cpu;
    long user, nice, system, idle, iowait, irq, softirq;
    iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
    long total = user + nice + system + idle + iowait + irq + softirq;
    long d_idle = idle - prev_idle;
    long d_total = total - prev_total;
    prev_idle = idle;
    prev_total = total;
    if (d_total == 0) return 0;
    return (1.0 - static_cast<double>(d_idle) / static_cast<double>(d_total)) * 100.0;
#else
    return -1;
#endif
}

static json getMemoryInfo() {
    json mem;
#ifdef __APPLE__
    int64_t physMem;
    size_t len = sizeof(physMem);
    sysctlbyname("hw.memsize", &physMem, &len, nullptr, 0);

    vm_statistics64_data_t vmStat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    host_statistics64(mach_host_self(), HOST_VM_INFO64,
                      reinterpret_cast<host_info64_t>(&vmStat), &count);
    int64_t pageSize = vm_kernel_page_size;
    int64_t used = (vmStat.active_count + vmStat.wire_count) * pageSize;

    mem["total_mb"] = physMem / (1024 * 1024);
    mem["used_mb"] = used / (1024 * 1024);
    mem["usage_percent"] = static_cast<double>(used) / physMem * 100.0;
#elif defined(__linux__)
    struct sysinfo si;
    sysinfo(&si);
    long total = si.totalram * si.mem_unit;
    long used = (si.totalram - si.freeram - si.bufferram) * si.mem_unit;
    mem["total_mb"] = total / (1024 * 1024);
    mem["used_mb"] = used / (1024 * 1024);
    mem["usage_percent"] = static_cast<double>(used) / total * 100.0;
#endif
    return mem;
}

static json getDiskInfo() {
    json disks = json::array();
    const char* paths[] = {"/", nullptr};

#ifdef __APPLE__
    for (int i = 0; paths[i]; i++) {
        struct statfs sf;
        if (statfs(paths[i], &sf) == 0) {
            json d;
            d["mount"] = paths[i];
            d["total_gb"] = static_cast<double>(sf.f_blocks) * sf.f_bsize / (1024.0 * 1024 * 1024);
            d["free_gb"] = static_cast<double>(sf.f_bavail) * sf.f_bsize / (1024.0 * 1024 * 1024);
            d["used_gb"] = d["total_gb"].get<double>() - d["free_gb"].get<double>();
            d["usage_percent"] = d["total_gb"].get<double>() > 0
                ? d["used_gb"].get<double>() / d["total_gb"].get<double>() * 100.0 : 0;
            disks.push_back(d);
        }
    }
#elif defined(__linux__)
    for (int i = 0; paths[i]; i++) {
        struct statvfs sv;
        if (statvfs(paths[i], &sv) == 0) {
            json d;
            d["mount"] = paths[i];
            d["total_gb"] = static_cast<double>(sv.f_blocks) * sv.f_frsize / (1024.0 * 1024 * 1024);
            d["free_gb"] = static_cast<double>(sv.f_bavail) * sv.f_frsize / (1024.0 * 1024 * 1024);
            d["used_gb"] = d["total_gb"].get<double>() - d["free_gb"].get<double>();
            d["usage_percent"] = d["total_gb"].get<double>() > 0
                ? d["used_gb"].get<double>() / d["total_gb"].get<double>() * 100.0 : 0;
            disks.push_back(d);
        }
    }
#endif
    return disks;
}

static double getTemperature() {
#ifdef __APPLE__
    // macOS: use powermetrics or osx-cpu-temp if available, else return -1
    std::string result = execCommand("which osx-cpu-temp > /dev/null 2>&1 && osx-cpu-temp 2>/dev/null | head -1");
    if (!result.empty()) {
        try { return std::stod(result); } catch (...) {}
    }
    return -1;
#elif defined(__linux__)
    // Linux: read from thermal zone
    std::ifstream ifs("/sys/class/thermal/thermal_zone0/temp");
    if (ifs) {
        int temp;
        ifs >> temp;
        return temp / 1000.0;
    }
    return -1;
#else
    return -1;
#endif
}

static void collectorThread() {
    // Initial CPU read to prime the delta calculation
    getCpuUsage();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (g_collecting) {
        json metrics;
        metrics["action"] = "metrics";

        auto now = std::chrono::system_clock::now();
        auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        metrics["timestamp"] = epoch;

        metrics["cpu_percent"] = getCpuUsage();
        metrics["memory"] = getMemoryInfo();
        metrics["disks"] = getDiskInfo();
        metrics["temperature"] = getTemperature();

        sendMessage(metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(g_interval_ms));
    }
}

static void handleMessage(const json& msg) {
    std::string action = msg.value("action", "");

    if (action == "start_collecting") {
        g_interval_ms = msg.value("interval_ms", 1000);
        if (!g_collecting) {
            g_collecting = true;
            std::thread(collectorThread).detach();
            spdlog::info("Started metrics collection (interval={}ms)", g_interval_ms.load());
        }

    } else if (action == "stop_collecting") {
        g_collecting = false;
        spdlog::info("Stopped metrics collection");

    } else if (action == "request_metrics") {
        // One-shot metrics request
        json metrics;
        metrics["action"] = "metrics";
        auto now = std::chrono::system_clock::now();
        metrics["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        metrics["cpu_percent"] = getCpuUsage();
        metrics["memory"] = getMemoryInfo();
        metrics["disks"] = getDiskInfo();
        metrics["temperature"] = getTemperature();
        sendMessage(metrics);

    } else if (action == "ping") {
        json pong;
        pong["action"] = "pong";
        sendMessage(pong);
    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Monitor plugin client started");

    json ready;
    ready["action"] = "ready";
    ready["plugin"] = "monitor";
    ready["role"] = "client";
    sendMessage(ready);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            json msg = json::parse(line);
            handleMessage(msg);
        } catch (const json::parse_error& e) {
            spdlog::error("JSON parse error: {}", e.what());
        }
    }

    g_collecting = false;
    spdlog::info("Monitor plugin client exiting");
    return 0;
}
