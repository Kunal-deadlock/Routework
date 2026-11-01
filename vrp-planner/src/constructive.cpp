#include "solvers/constructive.hpp"
#include "distance.hpp"
#include "metrics.hpp"
#include "heap.hpp"
#include "solvers/exact.hpp"
#include "solvers/improve.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <limits>
namespace vrp {
// O(n^2) distance scans, O(n) extra space.
Solution nearestNeighbour(const Instance& in) {
    Solution s;s.algo="nn";std::vector<bool> seen(in.n+1);int left=in.n;
    while(left) {
        Route r{{0}};int last=0;
        while(true) {
            int next=-1;double best=std::numeric_limits<double>::infinity();
            for(int i=1;i<=in.n;++i) if(!seen[i] && static_cast<long long>(r.load)+in.demand[i]<=in.capacity) {double d=edge(in,last,i);if(d<best) {best=d;next=i;}}
            if(next<0) break;
            r.nodes.push_back(next);r.load+=in.demand[next];seen[next]=true;--left;last=next;
        }
        r.nodes.push_back(0);s.routes.push_back(r);
    }
    evaluate(in,s);
    if(s.routes.size()>static_cast<std::size_t>(in.numVehicles)) {s.status="heuristic_failed";s.message="nearest-neighbour packing exceeds fleet; infeasibility is not proven";s.routes.clear();s.total=0;}
    return s;
}
namespace {
Route orderedCluster(const Instance& in,const std::vector<int>& cluster) {
    std::vector<int> order;
    if(cluster.size()<=12) heldKarp(in,cluster,order);
    else {
        auto left=cluster;int last=0;
        while(!left.empty()) {std::size_t best=0;double dist=edge(in,last,left[0]);for(std::size_t i=1;i<left.size();++i) {double d=edge(in,last,left[i]);if(d<dist) {dist=d;best=i;}}last=left[best];order.push_back(last);left.erase(left.begin()+static_cast<std::ptrdiff_t>(best));}
    }
    Route r{{0}};r.nodes.insert(r.nodes.end(),order.begin(),order.end());r.nodes.push_back(0);std::uint64_t passes=0;if(cluster.size()>12) twoOpt(r,in,passes);return r;
}
// Bounded packing repair is used only if the original clustering exceeds K.
// A failed repair is a heuristic failure, never a proof of infeasibility.
void repairFleet(const Instance& in,Solution& s) {
    if(s.routes.size()<=static_cast<std::size_t>(in.numVehicles)) {evaluate(in,s);return;}
    std::vector<int> idx(in.n);std::iota(idx.begin(),idx.end(),1);
    std::stable_sort(idx.begin(),idx.end(),[&](int a,int b){return in.demand[a]>in.demand[b];});
    int K=std::min(in.n,in.numVehicles);std::vector<int> load(K);std::vector<std::vector<int>> bins(K);std::uint64_t nodes=0;
    std::function<bool(int)> pack=[&](int pos) {
        if(pos==in.n) return true;
        if(++nodes>100000) return false;
        int previous=-1;
        for(int k=0;k<K;++k) if(load[k]!=previous && static_cast<long long>(load[k])+in.demand[idx[pos]]<=in.capacity) {
            previous=load[k];load[k]+=in.demand[idx[pos]];bins[k].push_back(idx[pos]);
            if(pack(pos+1)) return true;
            bins[k].pop_back();load[k]-=in.demand[idx[pos]];if(!load[k]) break;
        }
        return false;
    };
    s.routes.clear();
    if(!pack(0)) {s.status="heuristic_failed";s.message="clustering exceeds fleet and bounded packing repair failed; infeasibility unproven";s.total=0;return;}
    for(auto& bin:bins) if(!bin.empty()) s.routes.push_back(orderedCluster(in,bin));
    evaluate(in,s);
}
}
// O(n^2 log n) time and O(n^2) savings storage before optional packing repair.
Solution clarkeWright(const Instance& in,bool heap) {
    Solution s;s.algo="cw";
    std::vector<std::vector<int>> routes(in.n+1);std::vector<int> owner(in.n+1),load(in.n+1);
    for(int i=1;i<=in.n;++i) {routes[i]={i};owner[i]=i;load[i]=in.demand[i];}
    struct Saving {int i,j;double value;};std::vector<Saving> savings;
    for(int i=1;i<=in.n;++i) for(int j=i+1;j<=in.n;++j) savings.push_back({i,j,edge(in,0,i)+edge(in,0,j)-edge(in,i,j)});
    auto less=[](const Saving& a,const Saving& b){++comparisons;if(a.value!=b.value) return a.value>b.value;return a.i!=b.i?a.i<b.i:a.j<b.j;};
    if(heap) heapSort(savings,less);else std::sort(savings.begin(),savings.end(),less);
    for(const auto& e:savings) {
        int a=owner[e.i],b=owner[e.j];if(a==b || static_cast<long long>(load[a])+load[b]>in.capacity) continue;
        if((routes[a].front()!=e.i && routes[a].back()!=e.i) || (routes[b].front()!=e.j && routes[b].back()!=e.j)) continue;
        if(routes[a].back()!=e.i) std::reverse(routes[a].begin(),routes[a].end());
        if(routes[b].front()!=e.j) std::reverse(routes[b].begin(),routes[b].end());
        for(int c:routes[b]) {routes[a].push_back(c);owner[c]=a;}
        load[a]+=load[b];routes[b].clear();
    }
    for(int i=1;i<=in.n;++i) if(!routes[i].empty()) {Route r{{0}};r.nodes.insert(r.nodes.end(),routes[i].begin(),routes[i].end());r.nodes.push_back(0);s.routes.push_back(r);}
    repairFleet(in,s);return s;
}
// O(n log n) angle ordering plus cluster Held-Karp (m<=12), or NN and 2-opt.
Solution sweep(const Instance& in) {
    std::vector<int> idx(in.n);std::iota(idx.begin(),idx.end(),1);
    std::vector<double> angle(in.n+1);
    for(int i=1;i<=in.n;++i) angle[i]=std::atan2(in.lat[i]-in.lat[0],(in.lon[i]-in.lon[0])*std::cos(in.lat[0]*std::acos(-1.0)/180));
    std::sort(idx.begin(),idx.end(),[&](int a,int b){return angle[a]!=angle[b]?angle[a]<angle[b]:a<b;});
    Solution s;s.algo="sweep";std::vector<int> cluster;int load=0;
    for(int i:idx) {if(static_cast<long long>(load)+in.demand[i]>in.capacity) {s.routes.push_back(orderedCluster(in,cluster));cluster.clear();load=0;}cluster.push_back(i);load+=in.demand[i];}
    if(!cluster.empty()) s.routes.push_back(orderedCluster(in,cluster));
    repairFleet(in,s);return s;
}
// Isolated sorting measurement: savings generation and post-check outside timer.
Solution benchmarkSavingsSort(const Instance& in,bool heap) {
    struct Saving {int i,j;double value;};std::vector<Saving> v;
    for(int i=1;i<=in.n;++i) for(int j=i+1;j<=in.n;++j) v.push_back({i,j,in.d[0][i]+in.d[0][j]-in.d[i][j]});
    comparisons=0;
    auto less=[](const Saving& a,const Saving& b){++comparisons;if(a.value!=b.value) return a.value>b.value;return a.i!=b.i?a.i<b.i:a.j<b.j;};
    Timer t;if(heap) heapSort(v,less);else std::sort(v.begin(),v.end(),less);
    Solution s;s.ms=t.ms();s.comparisons=comparisons;s.algo="savings-sort";s.status="sort_only";
    for(std::size_t i=1;i<v.size();++i) if(v[i].value>v[i-1].value) throw std::runtime_error("savings sort failed");
    return s;
}
}
