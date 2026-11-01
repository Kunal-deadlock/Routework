#include "mst.hpp"
#include "metrics.hpp"
#include <algorithm>
#include <limits>
#include <numeric>
namespace vrp {
// Prim on a dense matrix: O(n^2) time, O(n) extra memory.
double prim(const Instance& in) {
    std::vector<double> key(in.n+1,std::numeric_limits<double>::infinity());std::vector<bool> used(in.n+1);key[0]=0;double sum=0;
    for(int step=0;step<=in.n;++step) {
        int v=-1;for(int i=0;i<=in.n;++i) if(!used[i] && (v<0 || key[i]<key[v])) v=i;
        used[v]=true;sum+=key[v];
        for(int j=0;j<=in.n;++j) if(!used[j]) key[j]=std::min(key[j],edge(in,v,j));
    }
    return sum;
}
// Kruskal: O(n^2 log n) time, O(n^2) edge storage; union by rank/path compression.
double kruskal(const Instance& in) {
    struct Edge {int a,b;double w;};std::vector<Edge> edges;
    for(int i=0;i<=in.n;++i) for(int j=0;j<i;++j) edges.push_back({i,j,edge(in,i,j)});
    std::sort(edges.begin(),edges.end(),[](const Edge& a,const Edge& b){if(a.w!=b.w) return a.w<b.w;return a.a!=b.a?a.a<b.a:a.b<b.b;});
    std::vector<int> parent(in.n+1),rank(in.n+1);std::iota(parent.begin(),parent.end(),0);
    auto root=[&](int x){while(parent[x]!=x) {parent[x]=parent[parent[x]];x=parent[x];}return x;};
    double sum=0;int count=0;
    for(const auto& e:edges) {int a=root(e.a),b=root(e.b);if(a==b) continue;if(rank[a]<rank[b]) std::swap(a,b);parent[b]=a;if(rank[a]==rank[b]) ++rank[a];sum+=e.w;if(++count==in.n) break;}
    return sum;
}
}
