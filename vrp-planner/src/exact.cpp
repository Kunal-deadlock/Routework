#include "solvers/exact.hpp"
#include "solvers/constructive.hpp"
#include "solvers/baseline.hpp"
#include "distance.hpp"
#include "metrics.hpp"
#include "heap.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
namespace vrp {
namespace {
constexpr double INF=std::numeric_limits<double>::infinity();
struct Tours {
    int m,full;std::vector<double> dp,tour;std::vector<int> parent,last;std::vector<int> idx;
    Tours(const Instance& in,const std::vector<int>& customers):m(static_cast<int>(customers.size())),full(1<<m),dp(full*m,INF),tour(full,INF),parent(full*m,-1),last(full,-1),idx(customers) {
        tour[0]=0;
        for(int j=0;j<m;++j) dp[(1<<j)*m+j]=edge(in,0,idx[j]);
        for(int mask=1;mask<full;++mask) for(int j=0;j<m;++j) if((mask&(1<<j)) && std::isfinite(dp[mask*m+j])) {
            double end=dp[mask*m+j]+edge(in,idx[j],0);
            if(end<tour[mask]) {tour[mask]=end;last[mask]=j;}
            for(int k=0;k<m;++k) if(!(mask&(1<<k))) {
                double cost=dp[mask*m+j]+edge(in,idx[j],idx[k]);int at=(mask|(1<<k))*m+k;
                if(cost<dp[at]) {dp[at]=cost;parent[at]=j;}
            }
        }
    }
    std::vector<int> order(int mask) const {
        std::vector<int> r;int j=last[mask];
        while(j>=0) {r.push_back(idx[j]);int p=parent[mask*m+j];mask^=1<<j;j=p;}
        std::reverse(r.begin(),r.end());return r;
    }
};
Solution refused(const std::string& why) {Solution s;s.status="unsupported";s.message=why;return s;}
Route wrap(const std::vector<int>& order) {Route r{{0}};r.nodes.insert(r.nodes.end(),order.begin(),order.end());r.nodes.push_back(0);return r;}
std::vector<int> indices(int n) {std::vector<int> r;for(int i=1;i<=n;++i) r.push_back(i);return r;}
struct Node {
    unsigned visited=0;int load=0,last=0,closed=0,anchor=1;
    double cost=0,bound=0;std::vector<int> path{0};
};
int firstUnvisited(unsigned mask,int n) {for(int i=1;i<=n;++i) if(!(mask&(1u<<(i-1)))) return i;return n+1;}
// Assignment relaxation of the remaining TSP path: row then column reduction.
// Ignoring subtours can only underestimate a valid completion, so it is admissible.
double reducedMatrixBound(const Instance& in,const Node& x) {
    std::vector<int> source{x.last},destination;
    for(int i=1;i<=in.n;++i) if(!(x.visited&(1u<<(i-1)))) {source.push_back(i);destination.push_back(i);}
    destination.push_back(0);std::size_t m=source.size();
    std::vector<std::vector<double>> matrix(m,std::vector<double>(m,INF));
    for(std::size_t i=0;i<m;++i) for(std::size_t j=0;j<m;++j)
        if(source[i]!=destination[j] && !(i==0 && destination[j]==0 && m>1)) matrix[i][j]=edge(in,source[i],destination[j]);
    double bound=0;
    for(auto& row:matrix) {
        double v=*std::min_element(row.begin(),row.end());if(!std::isfinite(v)) return INF;bound+=v;
        for(double& a:row) if(std::isfinite(a)) a-=v;
    }
    for(std::size_t j=0;j<m;++j) {double v=INF;for(std::size_t i=0;i<m;++i) v=std::min(v,matrix[i][j]);if(!std::isfinite(v)) return INF;bound+=v;}
    return bound;
}
double remainingBound(const Instance& in,const Node& x) {
    if(in.numVehicles==1) return reducedMatrixBound(in,x);
    double b=0;
    for(int i=1;i<=in.n;++i) if(i==x.last || !(x.visited&(1u<<(i-1)))) {
        double cheapest=edge(in,i,0);
        for(int j=1;j<=in.n;++j) if(j!=i && !(x.visited&(1u<<(j-1)))) cheapest=std::min(cheapest,edge(in,i,j));
        b+=cheapest;
    }
    return b;
}
std::vector<Node> children(const Instance& in,const Node& x) {
    std::vector<Node> out;
    long long demand=0;for(int i=1;i<=in.n;++i) if(!(x.visited&(1u<<(i-1)))) demand+=in.demand[i];
    if(demand>static_cast<long long>(in.numVehicles-x.closed)*in.capacity-x.load) return out;
    for(int i=1;i<=in.n;++i) if(!(x.visited&(1u<<(i-1))) && static_cast<long long>(x.load)+in.demand[i]<=in.capacity) {
        Node y=x;y.visited|=1u<<(i-1);y.load+=in.demand[i];y.cost+=edge(in,x.last,i);y.last=i;y.path.push_back(i);out.push_back(std::move(y));
    }
    // Canonical route order: each completed route contains the smallest remaining
    // customer at its opening. Customers inside that route may have any order.
    if(x.last && x.closed+1<in.numVehicles && (x.visited&(1u<<(x.anchor-1)))) {
        Node y=x;y.path.push_back(0);y.cost+=edge(in,x.last,0);y.last=0;y.load=0;++y.closed;y.anchor=firstUnvisited(y.visited,in.n);out.push_back(std::move(y));
    }
    return out;
}
Solution fromNode(const Instance& in,Node x) {
    Solution s;Route r{{0}};
    x.path.push_back(0);
    for(std::size_t i=1;i<x.path.size();++i) {r.nodes.push_back(x.path[i]);if(x.path[i]==0) {s.routes.push_back(r);r=Route{{0}};}}
    evaluate(in,s);return s;
}
// Search over customer permutations and route boundaries. Exponential/factorial
// worst case; B&B bounds cost O(n^2) per node, storage O(live nodes * n).
Solution search(const Instance& in,bool bounded,Strategy strategy,const Options& opt) {
    if(in.n>(bounded?12:9)) return refused(bounded?"branch and bound supports n <= 12":"backtracking supports n <= 9");
    Solution best=nearestNeighbour(in);auto base=baseline(in);
    if(base.status=="ok" && (best.status!="ok" || base.total<best.total)) best=base;
    if(bounded) {auto cw=clarkeWright(in,opt.heapSort);if(cw.status=="ok" && (best.status!="ok" || cw.total<best.total)) best=cw;}
    double upper=best.status=="ok"?best.total:INF;
    unsigned full=(1u<<in.n)-1;Timer timer;std::uint64_t expanded=0;bool limited=false;
    auto complete=[&](const Node& x) {
        double cost=x.cost+edge(in,x.last,0);
        if(cost<upper) {best=fromNode(in,x);upper=best.total;}
    };
    auto stop=[&](){return expanded>=opt.nodeLimit || timer.ms()>=opt.timeLimitMs;};
    if(!bounded) {
        std::function<void(const Node&)> visit=[&](const Node& x) {
            if(limited) return;
            if(stop()) {limited=true;return;}
            ++expanded;
            if(x.visited==full) {complete(x);return;}
            if(x.cost>=upper) return;
            for(auto& y:children(in,x)) visit(y);
        };
        visit(Node{});
    } else {
        auto cmp=[](const Node& a,const Node& b){return a.bound<b.bound;};
        MinHeap<Node,decltype(cmp)> heap(cmp);std::deque<Node> deque;
        auto push=[&](Node y){if(strategy==Strategy::LC) heap.push(std::move(y));else deque.push_back(std::move(y));};
        auto count=[&](){return strategy==Strategy::LC?heap.size():deque.size();};
        Node root;root.bound=remainingBound(in,root);push(root);
        while(count()) {
            if(stop() || count()>opt.nodeLimit) {limited=true;break;}
            Node x;
            if(strategy==Strategy::LC) {x=heap.pop();}
            else if(strategy==Strategy::FIFO) {x=std::move(deque.front());deque.pop_front();}
            else {x=std::move(deque.back());deque.pop_back();}
            if(x.bound>=upper) continue;
            ++expanded;
            if(x.visited==full) {complete(x);continue;}
            for(auto& y:children(in,x)) {
                y.bound=y.cost+remainingBound(in,y);
                if(y.bound<upper) push(std::move(y));
            }
        }
    }
    best.nodesExpanded=expanded;best.optimal=!limited;
    if(std::isfinite(upper)) {best.status=limited?"limit":"ok";best.message=limited?"search limit reached; feasible incumbent, optimality unproven":"";}
    else {best.routes.clear();best.total=0;best.status=limited?"limit_no_solution":"infeasible";best.message=limited?"search limit reached before finding a feasible plan":"no capacity-feasible partition exists";}
    return best;
}
}
// Held-Karp: O(m^2 2^m) time, O(m 2^m) space, including reconstruction.
double heldKarp(const Instance& in,const std::vector<int>& customers,std::vector<int>& order) {
    if(customers.size()>15) throw std::runtime_error("Held-Karp supports at most 15 customers (exponential memory)");
    if(customers.empty()) {order.clear();return 0;}
    std::vector<bool> seen(in.n+1);
    for(int i:customers) {if(i<1 || i>in.n || seen[i]) throw std::runtime_error("invalid Held-Karp customer subset");seen[i]=true;}
    Tours table(in,customers);order=table.order(table.full-1);return table.tour.back();
}
Solution heldKarpRoute(const Instance& in) {
    if(in.n>15) return refused("Held-Karp supports n <= 15 (O(n * 2^n) memory)");
    long long load=0;for(int i=1;i<=in.n;++i) load+=in.demand[i];
    if(load>in.capacity) return refused("Held-Karp solves one route; use exact for a multi-vehicle plan");
    std::vector<int> order;heldKarp(in,indices(in.n),order);Solution s;s.routes={wrap(order)};evaluate(in,s);s.optimal=true;return s;
}
// O(K * 3^n + n^2 * 2^n) time, O((n+K) * 2^n) memory.
Solution subsetDP(const Instance& in) {
    if(in.n>15) return refused("subset DP supports n <= 15 (exponential time and memory)");
    Tours table(in,indices(in.n));int full=table.full,K=std::min(in.n,in.numVehicles);
    std::vector<long long> demand(full);
    for(int mask=1;mask<full;++mask) {int bit=0;while(!(mask&(1<<bit))) ++bit;demand[mask]=demand[mask^(1<<bit)]+in.demand[bit+1];}
    std::vector<double> dp((K+1)*full,INF);std::vector<int> choice((K+1)*full,-1);dp[0]=0;
    for(int k=1;k<=K;++k) {
        dp[k*full]=0;
        for(int mask=1;mask<full;++mask) {
            int anchor=mask&-mask;
            for(int sub=mask;sub;sub=(sub-1)&mask) if((sub&anchor) && demand[sub]<=in.capacity) {
                double cost=table.tour[sub]+dp[(k-1)*full+(mask^sub)];
                if(cost<dp[k*full+mask]) {dp[k*full+mask]=cost;choice[k*full+mask]=sub;}
            }
        }
    }
    Solution s;s.optimal=true;int mask=full-1;
    if(!std::isfinite(dp[K*full+mask])) {s.status="infeasible";s.message="no partition fits the fleet";return s;}
    for(int k=K;mask && k>0;--k) {int sub=choice[k*full+mask];s.routes.push_back(wrap(table.order(sub)));mask^=sub;}
    evaluate(in,s);return s;
}
Solution backtracking(const Instance& in,const Options& o) {return search(in,false,Strategy::LIFO,o);}
Solution branchAndBound(const Instance& in,Strategy s,const Options& o) {return search(in,true,s,o);}
}
