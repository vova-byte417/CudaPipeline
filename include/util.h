#pragma once

#include <iostream>
#include <chrono>
#include <string>

// 通用错误处理
#define LOG_ERROR(msg) \
    std::cerr << "[ERROR][" << __FILE__ << ":" << __LINE__ << "] " << msg << std::endl

#define LOG_WARN(msg) \
    std::cerr << "[WARN][" << __FILE__ << ":" << __LINE__ << "] " << msg << std::endl

#define LOG_INFO(msg) \
    std::cout << "[INFO] " << msg << std::endl

// Result类型用于错误传递
template <typename T>
struct Result {
    bool ok;
    T value;
    std::string error;

    static Result Ok(const T& v) { return {true, v, ""}; }
    static Result Err(const std::string& msg) { return {false, T(), msg}; }

    operator bool() const { return ok; }
};

template <>
struct Result<void> {
    bool ok;
    std::string error;

    static Result Ok() { return {true, ""}; }
    static Result Err(const std::string& msg) { return {false, msg}; }

    operator bool() const { return ok; }
};

// 时间工具
inline uint64_t now_ns() {
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

inline uint64_t now_us() {
    return now_ns() / 1000;
}

inline uint64_t now_ms() {
    return now_ns() / 1000000;
}
