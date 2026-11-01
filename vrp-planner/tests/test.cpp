#include "distance.hpp"
#include "io.hpp"
#include "verify.hpp"
#include "solvers/baseline.hpp"
#include "solvers/constructive.hpp"
#include "solvers/exact.hpp"
#include "solvers/improve.hpp"
#include "heap.hpp"
#include "mst.hpp"
#include "benchmark.hpp"
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace vrp;
int checks=0;
void check(bool ok,const char* msg) {++checks;if(!ok) throw std::runtime_error(msg);}
void rejects(const std::function<void()>& f) {bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"expected rejection");}
int main() {try {
    auto in=generate(5,"uniform",42);
    for(int i=0;i<=in.n;++i) for(int j=0;j<=in.n;++j) {
        check(in.d[i][j]==in.d[j][i],"symmetry");check(in.d[i][i]==0,"diagonal");
        for(int k=0;k<=in.n;++k) check(in.d[i][j]<=in.d[i][k]+in.d[k][j]+1e-9,"triangle inequality");
    }
    writeInstance(in,"tests/roundtrip.csv");auto copy=readCSV("tests/roundtrip.csv",in.numVehicles,in.capacity);
    check(copy.id==in.id && copy.demand==in.demand,"CSV roundtrip");
    auto bad=in;bad.id[1]=bad.id[0];rejects([&]{validate(bad);});bad=in;bad.demand[1]=in.capacity+1;rejects([&]{validate(bad);});bad=in;bad.capacity=1;rejects([&]{validate(bad);});bad=in;bad.lat[1]=91;rejects([&]{validate(bad);});rejects([&]{validate(Instance{});});
    auto one=generate(1,"uniform",1,1,2);one.demand[1]=2;
    Solution s;s.routes={Route{{0,1,0}}};evaluate(one,s);verify(one,s);++checks;
    s.total+=1;rejects([&]{verify(one,s);});s.total-=1;s.routes[0].nodes={0,1,1,0};rejects([&]{verify(one,s);});
    one.lat[1]=one.lat[0];one.lon[1]=one.lon[0];buildMatrix(one);s.routes={Route{{0,1,0}}};evaluate(one,s);verify(one,s);++checks;
    for(auto solver:{baseline,nearestNeighbour}) {
        auto plan=solver(in);verify(in,plan);++checks;
        plan=solver(one);verify(one,plan);check(plan.total==0,"duplicate coordinates route");
    }
    Options unlimited;unlimited.nodeLimit=10000000;unlimited.timeLimitMs=30000;
    for(int n=1;n<=9;++n) for(unsigned seed=1;seed<=3;++seed) {
        auto tiny=generate(n,"uniform",seed,1,30);
        auto dp=subsetDP(tiny),hk=heldKarpRoute(tiny),bt=backtracking(tiny,unlimited);
        verify(tiny,dp);verify(tiny,hk);verify(tiny,bt);
        check(bt.optimal && std::abs(dp.total-bt.total)<1e-7,"DP versus BT TSP");
        check(std::abs(hk.total-bt.total)<1e-7,"Held-Karp versus BT");
        for(auto strategy:{Strategy::FIFO,Strategy::LIFO,Strategy::LC}) {auto bb=branchAndBound(tiny,strategy,unlimited);verify(tiny,bb);check(bb.optimal && std::abs(bb.total-dp.total)<1e-7,"BB versus DP TSP");}
        for(auto solver:{baseline,nearestNeighbour}) {auto plan=solver(tiny);check(plan.total+1e-7>=dp.total,"heuristic beats exact");}
    }
    for(int n=3;n<=7;++n) for(unsigned seed=1;seed<=3;++seed) {
        auto tiny=generate(n,"clustered",seed,n,5);tiny.numVehicles=std::max(1,(n+1)/2);
        auto dp=subsetDP(tiny),bt=backtracking(tiny,unlimited);
        check(dp.status==bt.status && bt.optimal,"CVRP status");
        if(dp.status=="ok") {verify(tiny,dp);verify(tiny,bt);check(std::abs(dp.total-bt.total)<1e-7,"DP versus BT CVRP");}
        for(auto strategy:{Strategy::FIFO,Strategy::LIFO,Strategy::LC}) {auto bb=branchAndBound(tiny,strategy,unlimited);check(bb.status==dp.status && bb.optimal,"BB CVRP status");if(bb.status=="ok") {verify(tiny,bb);check(std::abs(bb.total-dp.total)<1e-7,"BB versus DP CVRP");}}
    }
    auto packed=generate(3,"uniform",1,3,3);packed.demand={0,2,2,2};packed.numVehicles=2;
    check(subsetDP(packed).status=="infeasible","bin packing infeasible despite total bound");
    check(backtracking(packed,unlimited).status=="infeasible","BT infeasible");
    auto big=generate(16,"uniform",1);check(subsetDP(big).status=="unsupported","DP guard");check(backtracking(big).status=="unsupported","BT guard");check(branchAndBound(big,Strategy::LC).status=="unsupported","BB guard");
    Options stopped;stopped.nodeLimit=0;auto limited=backtracking(in,stopped);check(!limited.optimal && limited.status=="limit","limit never claims optimality");
    // A hand-checkable line: depot x=0, stops at x=1,2,3,4; optimum is 8.
    auto line=generate(4,"uniform",4,1,20);for(int i=0;i<=4;++i) for(int j=0;j<=4;++j) line.d[i][j]=std::abs(i-j);
    auto exact=subsetDP(line);verify(line,exact);check(std::abs(exact.total-8)<1e-9,"known line optimum");
    std::mt19937 rng(42);
    for(int n=0;n<100;++n) {
        std::vector<int> a;MinHeap<int> heap;for(int i=0;i<n;++i) {int v=static_cast<int>(rng()%23);a.push_back(v);heap.push(v);}
        auto b=a;std::sort(b.begin(),b.end());heapSort(a);check(a==b,"heap sort equals std sort");for(int v:b) check(heap.pop()==v,"minheap order");check(heap.empty(),"heap drained");
    }
    for(int n=1;n<=15;++n) for(unsigned seed=1;seed<=3;++seed) {
        auto sample=generate(n,"uniform",seed,n,8);sample.numVehicles=std::max(1,(n+2)/3);
        auto dp=subsetDP(sample);double bound=prim(sample);check(std::abs(bound-kruskal(sample))<1e-7,"Prim equals Kruskal");
        for(auto name:{"nn","cw","sweep"}) {
            auto plan=std::string(name)=="nn"?nearestNeighbour(sample):std::string(name)=="cw"?clarkeWright(sample):sweep(sample);
            if(plan.status!="ok") continue;
            verify(sample,plan);check(plan.total+1e-7>=bound,"MST lower bound");check(dp.status=="ok" && plan.total+1e-7>=dp.total,"heuristic versus exact");
            double before=plan.total;improveTwoOpt(sample,plan);verify(sample,plan);check(plan.total<=before+1e-7,"2-opt nonincreasing");
            before=plan.total;improveLocal(sample,plan);verify(sample,plan);check(plan.total<=before+1e-7,"local nonincreasing");
        }
        auto stdcw=clarkeWright(sample),heapcw=clarkeWright(sample,true);check(stdcw.status==heapcw.status && std::abs(stdcw.total-heapcw.total)<1e-7,"savings sort equivalence");
    }
    for(const auto& algo:algorithms()) {
        auto s=runSolver(line,algo,unlimited);verify(line,s);check(s.ops>0,"operation counter");check(s.ms>=0,"timer");
    }
    for(auto algo:{"baseline+local","nn+local","cw+local","sweep+local"}) {auto s=runSolver(in,algo);verify(in,s);++checks;}
    rejects([&]{runSolver(in,"bogus");});rejects([&]{runSolver(in,"exact+2opt");});rejects([&]{runSolver(in,"cw+bogus");});
    auto sorted=benchmarkSavingsSort(in,true);check(sorted.comparisons>0 && sorted.status=="sort_only","sort instrumentation");
    for(int n:{8,9}) {
        auto multi=generate(n,"clustered",13,n,5);multi.numVehicles=2;multi.demand.assign(n+1,1);multi.demand[0]=0;validate(multi);
        auto dp=runSolver(multi,"exact",unlimited),bt=runSolver(multi,"bt",unlimited);
        check(bt.optimal && std::abs(dp.total-bt.total)<1e-7,"n8/n9 multi-vehicle backtracking");
        for(auto algo:{"bb-fifo","bb-lifo","bb-lc"}) {auto bb=runSolver(multi,algo,unlimited);check(bb.optimal && std::abs(dp.total-bb.total)<1e-7,"n8/n9 multi-vehicle BB");}
    }
    copy.name[1]="Home, \"A\"";copy.id[1]=99;writeInstance(copy,"tests/roundtrip.csv");auto quoted=readCSV("tests/roundtrip.csv",copy.numVehicles,copy.capacity);check(quoted.name==copy.name && quoted.id==copy.id,"quoted names and external ids");
    auto csvReject=[&](const std::string& contents) {std::ofstream f("tests/invalid.csv");f<<contents;f.close();rejects([&]{readCSV("tests/invalid.csv",2,5);});};
    csvReject("");csvReject("type,id,name,lat,lon,demand,tw_open,tw_close,service\n");
    const std::string depot="depot,0,Depot,20,73,0,0,1440,0\n";
    csvReject(depot+"customer,1,A,NaN,73,1,0,1440,0\n");
    csvReject(depot+"customer,1,A,20,181,1,0,1440,0\n");
    csvReject(depot+"customer,1,A,20,73,-1,0,1440,0\n");
    csvReject(depot+"customer,1,A,20,73,1.5,0,1440,0\n");
    csvReject(depot+"customer,0,A,20,73,1,0,1440,0\n");
    csvReject(depot+"customer,1,\"A,20,73,1,0,1440,0\n");
    csvReject(depot+"customer,1,A,20,73,1,0,1440\n");
    csvReject("customer,1,A,20,73,1,0,1440,0\n");
    csvReject(depot+"depot,2,B,20,73,0,0,1440,0\n");
    for(auto solverName:{"baseline","nn","cw","sweep","bt","bb-fifo","bb-lifo","bb-lc","hk","exact"}) {auto plan=runSolver(one,solverName,unlimited);verify(one,plan);check(plan.total==0,"one customer full capacity duplicate coordinates");}
    auto routePlan=runSolver(in,"cw+2opt");writeRoutes(in,routePlan,"tests/routes.csv");writeSVG(in,routePlan,"tests/routes.svg");
    std::ifstream routeFile("tests/routes.csv");std::string row;int rows=0;while(std::getline(routeFile,row)) ++rows;check(rows==in.n+2*static_cast<int>(routePlan.routes.size())+1,"routes CSV includes depot endpoints");
    std::cout<<"PASS "<<checks<<" checks\n";return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
