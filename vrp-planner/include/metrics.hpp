#pragma once
#include <chrono>
#include <cstdint>
#include "types.hpp"
namespace vrp {
inline thread_local std::uint64_t operations=0, comparisons=0;
inline double edge(const Instance& in,int a,int b) { ++operations; return in.d[a][b]; }
struct Timer {
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    double ms() const { return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); }
};
}
