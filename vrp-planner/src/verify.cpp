#include "verify.hpp"
#include <cmath>
#include <stdexcept>
namespace vrp {
void verify(const Instance& in,const Solution& s) {
    auto fail=[](const std::string& x){throw std::runtime_error("Verifier: "+x);};
    if(s.status!="ok" && s.status!="limit") fail("no feasible solution to verify");
    if(s.routes.size()>static_cast<std::size_t>(in.numVehicles)) fail("too many vehicles");
    std::vector<int> seen(in.n+1); double total=0;
    for(const auto& r:s.routes) {
        if(r.nodes.size()<3 || r.nodes.front()!=0 || r.nodes.back()!=0) fail("route must start/end at depot and serve a stop");
        long long load=0; double dist=0;
        for(std::size_t i=1;i<r.nodes.size();++i) {
            int v=r.nodes[i], u=r.nodes[i-1];
            if(v<0 || v>in.n) fail("unknown customer");
            if(i+1<r.nodes.size()) { if(v==0 || ++seen[v]!=1) fail("duplicate/depot inside route"); load+=in.demand[v]; }
            dist+=in.d[u][v];
        }
        if(load>in.capacity || load!=r.load) fail("capacity/load mismatch");
        if(!std::isfinite(r.dist) || std::abs(dist-r.dist)>1e-7*(1+dist)) fail("route distance mismatch");
        total+=dist;
    }
    for(int i=1;i<=in.n;++i) if(seen[i]!=1) fail("missing customer");
    if(!std::isfinite(s.total) || std::abs(total-s.total)>1e-7*(1+total)) fail("total distance mismatch");
}
}
