#pragma once
#include "types.hpp"
namespace vrp {
Solution nearestNeighbour(const Instance& in);
Solution clarkeWright(const Instance& in,bool heap=false);
Solution sweep(const Instance& in);
Solution benchmarkSavingsSort(const Instance& in,bool heap);
}
