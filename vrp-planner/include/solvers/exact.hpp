#pragma once
#include "types.hpp"
namespace vrp {
enum class Strategy { FIFO, LIFO, LC };
double heldKarp(const Instance& in,const std::vector<int>& customers,std::vector<int>& order);
Solution heldKarpRoute(const Instance& in);
Solution subsetDP(const Instance& in);
Solution backtracking(const Instance& in,const Options& options={});
Solution branchAndBound(const Instance& in,Strategy strategy,const Options& options={});
}
