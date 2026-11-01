#include "solvers/baseline.hpp"
#include "distance.hpp"
namespace vrp {
// O(n) time and space. Listed order is retained even when it needs extra vehicles.
Solution baseline(const Instance& in) {
    Solution s;s.algo="baseline";Route r{{0}};
    for(int i=1;i<=in.n;++i) {
        if(static_cast<long long>(r.load)+in.demand[i]>in.capacity) {r.nodes.push_back(0);s.routes.push_back(r);r=Route{{0}};}
        r.nodes.push_back(i);r.load+=in.demand[i];
    }
    r.nodes.push_back(0);s.routes.push_back(r);evaluate(in,s);
    if(s.routes.size()>static_cast<std::size_t>(in.numVehicles)) {s.status="heuristic_failed";s.message="listed order requires more vehicles; instance may still be feasible";s.routes.clear();s.total=0;}
    return s;
}
}
