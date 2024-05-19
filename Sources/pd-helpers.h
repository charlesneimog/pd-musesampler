#pragma once

#include <functional>
#include <iostream>
#include <m_pd.h>
#include <sstream>
#include <string>
#include <unordered_map>

class Version {
  public:
    Version(int major, int minor, int patch) : major(major), minor(minor), patch(patch) {}
    int major;
    int minor;
    int patch;
    std::string toString() {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
    int majorVersion() { return major; }
    bool operator<(const Version &other) {
        if (major < other.major || minor < other.minor || patch < other.patch) {
            return true;
        }
        return false;
    };
};

static Version minimumSupported(0, 6, 0);

class LogStream {
  public:
    template <typename T> LogStream &operator<<(const T &value) {
        buffer << value;
        return *this;
    }

    ~LogStream() {
        // Convert the log message to a C string
        std::string message = buffer.str();
        const char *c_message = message.c_str();

        // Print the log message to pd_error()
        pd_error(nullptr, "%s", c_message);
    }

  private:
    std::ostringstream buffer;
};

#define LOGE() LogStream()
#define LOGI()                                                                                               \
    if (false)                                                                                               \
    std::cerr

// Helpers
inline void *loadLib(const std::string path);
inline void closeLib(void *libHandle);
inline void *getLibFunc(void *libHandle, const char *funcName);
