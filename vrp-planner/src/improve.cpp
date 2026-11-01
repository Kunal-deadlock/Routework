#include "solvers/improve.hpp"
#include "distance.hpp"
#include "metrics.hpp"
#include <algorithm>
namespace vrp {
// O(r^2) edge tests per pass. Repeated until no improving reversal remains.
bool twoOpt(Route& r,const Instance& in,std::uint64_t& passes) {
    bool any=false,changed=true;
    while(changed) {changed=false;++passes;
        for(std::size_t i=1;i+2<r.nodes.size();++i) for(std::size_t k=i+1;k+1<r.nodes.size();++k) {
            double delta=edge(in,r.nodes[i-1],r.nodes[k])+edge(in,r.nodes[i],r.nodes[k+1])-edge(in,r.nodes[i-1],r.nodes[i])-edge(in,r.nodes[k],r.nodes[k+1]);
            if(delta < -1e-9) {std::reverse(r.nodes.begin()+static_cast<std::ptrdiff_t>(i),r.nodes.begin()+static_cast<std::ptrdiff_t>(k+1));any=changed=true;}
        }
    }
    return any;
}
void improveTwoOpt(const Instance& in,Solution& s) {for(auto& r:s.routes) twoOpt(r,in,s.passes);evaluate(in,s);}
// O(n^2) relocate/swap candidate tests per pass, with intra-route 2-opt after moves.
void improveLocal(const Instance& in,Solution& s) {
    improveTwoOpt(in,s);bool changed=true;
    while(changed) {
        changed=false;
        for(std::size_t a=0;a<s.routes.size() && !changed;++a) for(std::size_t b=0;b<s.routes.size() && !changed;++b) if(a!=b) {
            auto& r=s.routes[a];auto& t=s.routes[b];
            for(std::size_t i=1;i+1<r.nodes.size() && !changed;++i) {
                int u=r.nodes[i];
                if(static_cast<long long>(t.load)+in.demand[u]<=in.capacity) for(std::size_t j=1;j<t.nodes.size();++j) {
                    double delta=edge(in,r.nodes[i-1],r.nodes[i+1])-edge(in,r.nodes[i-1],u)-edge(in,u,r.nodes[i+1])+edge(in,t.nodes[j-1],u)+edge(in,u,t.nodes[j])-edge(in,t.nodes[j-1],t.nodes[j]);
                    if(delta < -1e-9) {r.nodes.erase(r.nodes.begin()+static_cast<std::ptrdiff_t>(i));t.nodes.insert(t.nodes.begin()+static_cast<std::ptrdiff_t>(j),u);changed=true;break;}
                }
                if(changed) break;
                for(std::size_t j=1;j+1<t.nodes.size();++j) {
                    int v=t.nodes[j];
                    if(static_cast<long long>(r.load)-in.demand[u]+in.demand[v]>in.capacity || static_cast<long long>(t.load)-in.demand[v]+in.demand[u]>in.capacity) continue;
                    double delta=edge(in,r.nodes[i-1],v)+edge(in,v,r.nodes[i+1])+edge(in,t.nodes[j-1],u)+edge(in,u,t.nodes[j+1])-edge(in,r.nodes[i-1],u)-edge(in,u,r.nodes[i+1])-edge(in,t.nodes[j-1],v)-edge(in,v,t.nodes[j+1]);
                    if(delta < -1e-9) {std::swap(r.nodes[i],t.nodes[j]);changed=true;break;}
                }
            }
        }
        if(changed) {s.routes.erase(std::remove_if(s.routes.begin(),s.routes.end(),[](const Route& r){return r.nodes.size()==2;}),s.routes.end());improveTwoOpt(in,s);}
    }
}
}
