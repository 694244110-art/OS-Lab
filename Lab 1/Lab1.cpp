// Lab1 - แสดงข้อมูลของระบบปฏิบัติการ
// 1. ชื่อ CPU        2. หน่วยความจำ      3. ชื่อและ version ของ OS
// 4. พื้นที่จัดเก็บข้อมูล                5. Process ที่ทำงานอยู่ในระบบ
//
// รองรับทั้ง Linux (GPL/GCC) และ Windows (MSVC / MinGW)
// Linux  : g++ -std=c++17 -Wall -Wextra Lab1.cpp -o Lab1
// Windows: g++ -std=c++17 Lab1.cpp -o Lab1.exe   (หรือสร้างโปรเจกต์ Visual Studio)

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace {

struct ProcessInfo {
    long long pid;
    std::string state;
    std::string name;
    std::string command;
};

const int kMaxProcesses = 25;  // แสดงสูงสุด 25 process เพื่อไม่ให้หน้าจอยาวเกินไป

std::string trim(const std::string& text) {
    const std::string spaces = " \t\r\n";
    const auto first = text.find_first_not_of(spaces);
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(spaces);
    return text.substr(first, last - first + 1);
}

std::string afterSeparator(const std::string& line, char separator) {
    const auto pos = line.find(separator);
    return pos == std::string::npos ? "" : trim(line.substr(pos + 1));
}

std::string unquote(std::string value) {
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

// แปลงหน่วยไบต์ให้อ่านง่าย (B / KB / MB / GB / TB)
std::string formatBytes(unsigned long long bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value << ' ' << units[unit];
    return out.str();
}

std::string percent(unsigned long long part, unsigned long long total) {
    if (total == 0) {
        return "0.00%";
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << (100.0 * static_cast<double>(part) / static_cast<double>(total)) << '%';
    return out.str();
}

void printHeader(const std::string& title) {
    std::cout << "\n========================================\n"
              << title << "\n"
              << "========================================\n";
}

void printProcesses(const std::vector<ProcessInfo>& processes, long long total) {
    std::cout << "จำนวน Process ทั้งหมด: " << total << '\n';
    std::cout << std::left << std::setw(9) << "PID"
              << std::setw(9) << "State"
              << std::setw(22) << "Name"
              << "Command Line\n"
              << std::string(72, '-') << '\n';

    const std::size_t limit = std::min<std::size_t>(processes.size(), kMaxProcesses);
    for (std::size_t i = 0; i < limit; ++i) {
        const ProcessInfo& p = processes[i];
        std::string command = p.command;
        if (command.size() > 60) {
            command = command.substr(0, 57) + "...";
        }
        std::cout << std::left << std::setw(9) << p.pid
                  << std::setw(9) << p.state
                  << std::setw(22) << p.name
                  << command << '\n';
    }
    if (total > static_cast<long long>(limit)) {
        std::cout << "... แสดง " << limit << " รายการแรกจากทั้งหมด " << total << " รายการ\n";
    }
}

#ifdef _WIN32
// ---------------------------------------------------------------------------
// Windows implementation
// ---------------------------------------------------------------------------

std::string registryString(HKEY root, const char* subKey, const char* valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExA(root, subKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return "";
    }
    char buffer[512] = {};
    DWORD size = sizeof(buffer);
    const LONG status = RegQueryValueExA(key, valueName, nullptr, nullptr,
                                        reinterpret_cast<LPBYTE>(buffer), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return "";
    }
    buffer[sizeof(buffer) - 1] = '\0';
    return std::string(buffer);
}

void showCpu() {
    const std::string name = registryString(
        HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        "ProcessorNameString");
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    std::cout << "ชื่อ CPU (CPU Model)     : "
              << (name.empty() ? "ไม่พบข้อมูล" : name) << '\n';
    std::cout << "จำนวน Logical Processor : " << system.dwNumberOfProcessors << '\n';
}

void showMemory() {
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory)) {
        std::cout << "ไม่สามารถอ่านข้อมูลหน่วยความจำได้\n";
        return;
    }
    std::cout << "หน่วยความจำทั้งหมด (Total)     : "
              << formatBytes(memory.ullTotalPhys) << '\n';
    std::cout << "หน่วยความจำที่ใช้ได้ (Available) : "
              << formatBytes(memory.ullAvailPhys) << '\n';
    std::cout << "หน่วยความจำที่ใช้ไป (Used)      : "
              << formatBytes(memory.ullTotalPhys - memory.ullAvailPhys) << '\n';
    std::cout << "หน่วยความจำเสมือน (Virtual)     : "
              << formatBytes(memory.ullTotalVirtual) << " / ใช้ได้ "
              << formatBytes(memory.ullAvailVirtual) << '\n';
    std::cout << "การใช้งานหน่วยความจำ           : " << memory.dwMemoryLoad << "%\n";
}

void showOs() {
    const std::string product = registryString(
        HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
    const std::string version = registryString(
        HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion");
    const std::string build = registryString(
        HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild");

    std::cout << "ชื่อระบบปฏิบัติการ (OS) : "
              << (product.empty() ? "Windows" : product) << '\n';
    std::cout << "Version                : "
              << (version.empty() ? build : version + " (Build " + build + ")") << '\n';
    std::cout << "Architecture           : "
#ifdef _WIN64
              << "x86-64\n";
#else
              << "x86\n";
#endif
}

void showStorageFor(char drive) {
    const std::string root = std::string(1, drive) + ":\\";
    ULARGE_INTEGER available = {}, total = {}, freeBytes = {};
    if (!GetDiskFreeSpaceExA(root.c_str(), &available, &total, &freeBytes)) {
        return;
    }
    const unsigned long long used = total.QuadPart - freeBytes.QuadPart;
    std::cout << "  " << root << "  ทั้งหมด " << formatBytes(total.QuadPart)
              << " | ใช้ไป " << formatBytes(used) << " (" << percent(used, total.QuadPart) << ")"
              << " | ใช้ได้ " << formatBytes(available.QuadPart) << '\n';
}

void showStorage() {
    std::cout << "พื้นที่จัดเก็บข้อมูลตามไดรฟ์ (Disk):\n";
    const DWORD drives = GetLogicalDrives();
    bool found = false;
    for (int i = 0; i < 26; ++i) {
        if ((drives & (1UL << i)) != 0) {
            showStorageFor(static_cast<char>('A' + i));
            found = true;
        }
    }
    if (!found) {
        std::cout << "  ไม่พบไดรฟ์\n";
    }
}

void showProcesses() {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cout << "ไม่สามารถอ่านรายการ Process ได้\n";
        return;
    }

    std::vector<ProcessInfo> processes;
    PROCESSENTRY32A entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstA(snapshot, &entry)) {
        do {
            ProcessInfo info;
            info.pid = static_cast<long long>(entry.th32ProcessID);
            info.state = "-";
            info.name = entry.szExeFile;
            info.command = entry.szExeFile;
            processes.push_back(info);
        } while (Process32NextA(snapshot, &entry));
    }
    CloseHandle(snapshot);

    std::sort(processes.begin(), processes.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) { return a.pid < b.pid; });
    printProcesses(processes, static_cast<long long>(processes.size()));
}

#else
// ---------------------------------------------------------------------------
// Linux implementation
// ---------------------------------------------------------------------------

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

std::string valueOfLine(const std::string& content, const std::string& key, char separator) {
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind(key, 0) == 0) {
            return afterSeparator(line, separator);
        }
    }
    return "";
}

void showCpu() {
    const std::string content = readFile("/proc/cpuinfo");
    std::string model;
    for (const char* key : {"model name", "Hardware", "Processor", "model"}) {
        model = valueOfLine(content, key, ':');
        if (!model.empty()) {
            break;
        }
    }
    if (model.empty()) {
        utsname system{};
        if (uname(&system) == 0) {
            model = system.machine;
        }
    }
    const long processors = sysconf(_SC_NPROCESSORS_ONLN);
    std::cout << "ชื่อ CPU (CPU Model)     : "
              << (model.empty() ? "ไม่พบข้อมูล" : model) << '\n';
    std::cout << "จำนวน Logical Processor : " << processors << '\n';
}

void showMemory() {
    struct sysinfo info {};
    if (sysinfo(&info) != 0) {
        std::cout << "ไม่สามารถอ่านข้อมูลหน่วยความจำได้\n";
        return;
    }
    const auto unit = static_cast<unsigned long long>(info.mem_unit);
    const unsigned long long total =
        static_cast<unsigned long long>(info.totalram) * unit;
    const unsigned long long swapTotal =
        static_cast<unsigned long long>(info.totalswap) * unit;
    const unsigned long long swapFree =
        static_cast<unsigned long long>(info.freeswap) * unit;

    // MemAvailable จาก /proc/meminfo เป็นค่าที่ใช้ได้จริง (หน่วย kB)
    unsigned long long available =
        static_cast<unsigned long long>(info.freeram) * unit;
    const std::string meminfo = readFile("/proc/meminfo");
    const std::string availableText = valueOfLine(meminfo, "MemAvailable", ':');
    if (!availableText.empty()) {
        const std::string number = availableText.substr(0, availableText.find(' '));
        available = std::strtoull(number.c_str(), nullptr, 10) * 1024ULL;
    }
    const unsigned long long used = total > available ? total - available : 0ULL;

    std::cout << "หน่วยความจำทั้งหมด (Total)     : " << formatBytes(total) << '\n';
    std::cout << "หน่วยความจำที่ใช้ได้ (Available) : " << formatBytes(available) << '\n';
    std::cout << "หน่วยความจำที่ใช้ไป (Used)      : " << formatBytes(used)
              << " (" << percent(used, total) << ")\n";
    std::cout << "Swap ทั้งหมด / ใช้ได้          : "
              << formatBytes(swapTotal) << " / " << formatBytes(swapFree) << '\n';
}

void showOs() {
    utsname system{};
    if (uname(&system) != 0) {
        std::cout << "ไม่สามารถอ่านข้อมูลระบบปฏิบัติการได้\n";
        return;
    }

    const std::string release = readFile("/etc/os-release");
    const std::string pretty = unquote(valueOfLine(release, "PRETTY_NAME", '='));
    const std::string version = unquote(valueOfLine(release, "VERSION_ID", '='));

    std::cout << "ชื่อระบบปฏิบัติการ (OS) : "
              << (pretty.empty() ? system.sysname : pretty) << '\n';
    std::cout << "Version                : "
              << (version.empty() ? system.release : version) << '\n';
    std::cout << "Kernel                 : " << system.sysname << ' ' << system.release << '\n';
    std::cout << "Architecture           : " << system.machine << '\n';
}

void showStorageFor(const std::string& mountPoint, bool detailed) {
    struct statvfs info {};
    if (statvfs(mountPoint.c_str(), &info) != 0) {
        return;
    }
    const auto blockSize = static_cast<unsigned long long>(info.f_frsize);
    const unsigned long long total = static_cast<unsigned long long>(info.f_blocks) * blockSize;
    const unsigned long long freeBytes = static_cast<unsigned long long>(info.f_bfree) * blockSize;
    const unsigned long long available = static_cast<unsigned long long>(info.f_bavail) * blockSize;
    const unsigned long long used = total > freeBytes ? total - freeBytes : 0ULL;

    if (detailed) {
        std::cout << "  " << mountPoint << "  ทั้งหมด " << formatBytes(total) << '\n'
                  << "  ใช้ไป   " << formatBytes(used) << " (" << percent(used, total) << ")\n"
                  << "  ใช้ได้   " << formatBytes(available) << " (" << percent(available, total) << ")\n";
    } else {
        std::cout << "  " << std::left << std::setw(24) << mountPoint
                  << "ทั้งหมด " << formatBytes(total)
                  << " | ใช้ไป " << percent(used, total)
                  << " | ใช้ได้ " << formatBytes(available) << '\n';
    }
}

bool isPseudoFs(const std::string& type) {
    static const char* pseudo[] = {
        "proc", "sysfs", "devtmpfs", "tmpfs", "devpts", "cgroup", "cgroup2",
        "securityfs", "pstore", "bpf", "tracefs", "debugfs", "mqueue",
        "hugetlbfs", "fusectl", "configfs", "binfmt_misc", "nsfs", "autofs",
        "squashfs", "ramfs", "none"};
    for (const char* item : pseudo) {
        if (type == item) {
            return true;
        }
    }
    return false;
}

void showStorage() {
    std::cout << "พื้นที่จัดเก็บหลัก (Root filesystem /):\n";
    showStorageFor("/", true);

    std::cout << "\nพื้นที่จัดเก็บอื่น ๆ ที่ mount อยู่:\n";
    std::vector<std::string> seen {"/"};
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while (std::getline(mounts, line)) {
        std::istringstream fields(line);
        std::string device, type, mountPoint, options, rest;
        if (!(fields >> device >> type >> mountPoint >> options)) {
            continue;
        }
        if (isPseudoFs(type) || std::find(seen.begin(), seen.end(), mountPoint) != seen.end()) {
            continue;
        }
        // ตรวจสอบว่า statvfs อ่านได้ก่อน จึงค่อยแสดงผล
        struct statvfs info {};
        if (statvfs(mountPoint.c_str(), &info) != 0 || info.f_blocks == 0) {
            continue;
        }
        showStorageFor(mountPoint, false);
        seen.push_back(mountPoint);
    }
    if (seen.size() == 1) {
        std::cout << "  (ไม่มี filesystem อื่นนอกจาก /)\n";
    }
}

void showProcesses() {
    DIR* directory = opendir("/proc");
    if (directory == nullptr) {
        std::cout << "ไม่สามารถอ่าน /proc ได้\n";
        return;
    }

    std::vector<ProcessInfo> processes;
    dirent* entry = nullptr;
    while ((entry = readdir(directory)) != nullptr) {
        const std::string name = entry->d_name;
        if (name.empty() || !std::all_of(name.begin(), name.end(),
                                         [](unsigned char c) { return c >= '0' && c <= '9'; })) {
            continue;
        }
        const std::string base = "/proc/" + name;
        const std::string status = readFile(base + "/status");
        if (status.empty()) {
            continue;  // process อาจจบไปแล้วระหว่างอ่าน
        }

        ProcessInfo info;
        info.pid = std::strtoll(name.c_str(), nullptr, 10);
        info.name = valueOfLine(status, "Name:", ':');
        std::string state = valueOfLine(status, "State:", ':');
        info.state = state.substr(0, state.find(' '));
        info.command = readFile(base + "/cmdline");
        // /proc/<pid>/cmdline คั่นอาร์กิวเมนต์ด้วย '\0' และอาจมี '\n' ปนอยู่ ให้แปลงเป็นบรรทัดเดียว
        std::string command;
        for (char c : info.command) {
            if (c == '\0' || c == '\n' || c == '\r' || c == '\t') {
                c = ' ';
            }
            if (c != ' ' || command.empty() || command.back() != ' ') {
                command.push_back(c);
            }
        }
        info.command = trim(command);
        if (info.command.empty()) {
            info.command = "[" + info.name + "]";
        }
        processes.push_back(info);
    }
    closedir(directory);

    std::sort(processes.begin(), processes.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) { return a.pid < b.pid; });
    printProcesses(processes, static_cast<long long>(processes.size()));
}

#endif

}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // ให้แสดงภาษาไทยได้ในหน้าต่าง Console
#endif

    std::cout << "########################################\n"
              << "#  ข้อมูลระบบ (System Information)     #\n"
              << "########################################\n";

    printHeader("1. ชื่อ CPU (CPU Name)");
    showCpu();

    printHeader("2. จำนวนหน่วยความจำ (Memory)");
    showMemory();

    printHeader("3. ชื่อและ Version ของระบบปฏิบัติการ (OS)");
    showOs();

    printHeader("4. พื้นที่จัดเก็บข้อมูล (Storage)");
    showStorage();

    printHeader("5. Process ที่ทำงานอยู่ในระบบ (Running Processes)");
    showProcesses();

    std::cout << '\n';
    return 0;
}
