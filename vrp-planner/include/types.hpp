#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace vrp {
struct Instance {
    int n=0, capacity=20, numVehicles=3;
    double circuity=1.3;
    std::vector<int> id, demand, twOpen, twClose, service;
    std::vector<std::string> name;
    std::vector<double> lat, lon;
    std::vector<std::vector<double>> d;
};
struct Route { std::vector<int> nodes; int load=0; double dist=0; };
struct Solution {
    std::vector<Route> routes;
    double total=0, ms=0;
    std::uint64_t ops=0, comparisons=0, nodesExpanded=0, passes=0;
    std::string algo, status="ok", message;
    bool optimal=false;
};
struct Options {
    bool heapSort=false;
    std::uint64_t nodeLimit=1000000;
    double timeLimitMs=5000;
};
}
