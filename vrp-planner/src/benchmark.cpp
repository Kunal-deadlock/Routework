#include "benchmark.hpp"
#include "io.hpp"
#include "metrics.hpp"
#include "mst.hpp"
#include "solvers/baseline.hpp"
#include "solvers/constructive.hpp"
#include "solvers/exact.hpp"
#include "solvers/improve.hpp"
#include "verify.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace vrp {
bool hasPlan(const Solution& s) {return (s.status=="ok" || s.status=="limit") && !s.routes.empty();}
std::vector<std::string> algorithms() {return {"baseline","baseline+2opt","baseline+local","nn","nn+2opt","nn+local","cw","cw+2opt","cw+local","sweep","sweep+2opt","sweep+local","bt","bb-fifo","bb-lifo","bb-lc","hk","exact"};}
Solution runSolver(const Instance& in,const std::string& algorithm,const Options& options) {
    operations=0;comparisons=0;Timer timer;Solution s;
    auto plus=algorithm.find('+');std::string base=algorithm.substr(0,plus),suffix=plus==std::string::npos?"":algorithm.substr(plus);
    if(!suffix.empty() && (base!="nn" && base!="cw" && base!="sweep" && base!="baseline")) throw std::runtime_error("improvement suffix requires a constructive algorithm");
    if(!suffix.empty() && suffix!="+2opt" && suffix!="+local") throw std::runtime_error("unknown improvement: "+suffix);
    if(base=="baseline") s=baseline(in);
    else if(base=="nn") s=nearestNeighbour(in);
    else if(base=="cw") s=clarkeWright(in,options.heapSort);
    else if(base=="sweep") s=sweep(in);
    else if(base=="bt") s=backtracking(in,options);
    else if(base=="bb-fifo") s=branchAndBound(in,Strategy::FIFO,options);
    else if(base=="bb-lifo") s=branchAndBound(in,Strategy::LIFO,options);
    else if(base=="bb-lc") s=branchAndBound(in,Strategy::LC,options);
    else if(base=="hk") s=heldKarpRoute(in);
    else if(base=="exact") s=subsetDP(in);
    else throw std::runtime_error("unknown algorithm: "+algorithm);
    if(hasPlan(s) && suffix=="+2opt") improveTwoOpt(in,s);
    if(hasPlan(s) && suffix=="+local") improveLocal(in,s);
    s.ms=timer.ms();s.ops=operations;s.comparisons=comparisons;s.algo=algorithm;
    if(hasPlan(s)) verify(in,s); // independent, uncounted and outside solver timer
    return s;
}
void printSolution(const Instance& in,const Solution& s,double mst,double baselineKm) {
    std::cout<<std::fixed<<std::setprecision(6)<<s.algo<<": "<<s.status;
    if(!s.message.empty()) std::cout<<" ("<<s.message<<')';
    std::cout<<'\n';if(!hasPlan(s)) return;
    int vehicle=0;
    for(const auto& r:s.routes) {std::cout<<"Vehicle "<<++vehicle<<": ";for(std::size_t j=0;j<r.nodes.size();++j) {if(j) std::cout<<" -> ";std::cout<<in.id[r.nodes[j]];}std::cout<<" | load "<<r.load<<'/'<<in.capacity<<" | "<<r.dist<<" km\n";}
    std::cout<<"Total "<<s.total<<" km | "<<s.ms<<" ms | "<<s.ops<<" lookups | "<<s.nodesExpanded<<" nodes | optimal="<<(s.optimal?"yes":"no")<<"\nMST "<<mst<<" km";
    if(mst>0) std::cout<<" | ratio "<<s.total/mst;
    if(baselineKm>=0) {std::cout<<" | saved "<<baselineKm-s.total<<" km";if(baselineKm>0) std::cout<<" ("<<100*(baselineKm-s.total)/baselineKm<<"%)";}
    std::cout<<'\n';
}
void printComparison(const Instance& in,const Options& options) {
    auto optimum=runSolver(in,"exact",options),base=runSolver(in,"baseline",options);double bound=prim(in);
    std::cout<<"MST lower bound: "<<bound<<" km\n"<<std::left<<std::setw(16)<<"algorithm"<<std::setw(20)<<"status"<<std::setw(14)<<"km"<<std::setw(12)<<"ms"<<std::setw(16)<<"lookups"<<std::setw(12)<<"nodes"<<std::setw(12)<<"gap%"<<std::setw(8)<<"fleet"<<std::setw(14)<<"longest_km"<<std::setw(12)<<"MST_ratio"<<"saved%\n";
    for(const auto& algo:algorithms()) {
        auto s=algo=="exact"?optimum:algo=="baseline"?base:runSolver(in,algo,options);
        if(hasPlan(s) && hasPlan(optimum) && s.total+1e-7<optimum.total) throw std::runtime_error("solver beats exact optimum");
        std::cout<<std::left<<std::setw(16)<<algo<<std::setw(20)<<s.status<<std::setw(14)<<(hasPlan(s)?std::to_string(s.total):"NA")<<std::setw(12)<<s.ms<<std::setw(16)<<s.ops<<std::setw(12)<<s.nodesExpanded;
        auto numeric=[](double v){std::ostringstream stream;stream<<std::setprecision(6)<<v;return stream.str();};
        std::cout<<std::setw(12)<<(hasPlan(s) && hasPlan(optimum)?numeric(optimum.total>0?100*(s.total-optimum.total)/optimum.total:0):"NA");
        double longest=0;for(const auto& r:s.routes) longest=std::max(longest,r.dist);
        std::cout<<std::setw(8)<<(hasPlan(s)?std::to_string(s.routes.size()):"NA")<<std::setw(14)<<(hasPlan(s)?numeric(longest):"NA")<<std::setw(12)<<(hasPlan(s) && bound>0?numeric(s.total/bound):"NA");
        std::cout<<(hasPlan(s) && hasPlan(base)?numeric(base.total>0?100*(base.total-s.total)/base.total:0):"NA");
        std::cout<<'\n';
    }
}
namespace {
const double NA=std::numeric_limits<double>::quiet_NaN();
struct Record {
    std::string exp,instance,type,sort;int n,run;unsigned seed;Solution s;
    double mst=NA,optimum=NA,baseline=NA,before=NA,extraOps=NA;
    double longest() const {double d=0;for(const auto& r:s.routes) d=std::max(d,r.dist);return d;}
    double gap() const {return hasPlan(s) && std::isfinite(optimum)?(optimum>0?100*(s.total-optimum)/optimum:0):NA;}
    double savings() const {return hasPlan(s) && std::isfinite(baseline)?(baseline>0?100*(baseline-s.total)/baseline:0):NA;}
    double ratio() const {return hasPlan(s) && mst>0?s.total/mst:NA;}
};
void num(std::ostream& f,double v) {if(std::isfinite(v)) f<<v;}
void writeRecords(const std::vector<Record>& records,const std::string& path) {
    std::ofstream f(path);if(!f) throw std::runtime_error("cannot write "+path);f<<std::setprecision(12);
    f<<"experiment,instance,n,seed,type,run,algorithm,sort,status,optimal,distance_km,vehicles,longest_km,ms,ops,comparisons,nodes,passes,mst_km,mst_ratio,optimum_km,gap_pct,baseline_km,savings_pct,before_km,delta_km,extra_ops,message\n";
    for(const auto& r:records) {
        const auto& s=r.s;bool plan=hasPlan(s);
        f<<r.exp<<','<<csvQuote(r.instance)<<','<<r.n<<','<<r.seed<<','<<r.type<<','<<r.run<<','<<s.algo<<','<<r.sort<<','<<s.status<<','<<s.optimal<<',';
        if(plan) f<<s.total;
        f<<',';if(plan) f<<s.routes.size();f<<',';if(plan) f<<r.longest();
        f<<','<<s.ms<<','<<s.ops<<','<<s.comparisons<<','<<s.nodesExpanded<<','<<s.passes<<',';num(f,r.mst);f<<',';num(f,r.ratio());f<<',';num(f,r.optimum);f<<',';num(f,r.gap());f<<',';num(f,r.baseline);f<<',';num(f,r.savings());f<<',';num(f,r.before);f<<',';num(f,plan?r.before-s.total:NA);f<<',';num(f,r.extraOps);f<<','<<csvQuote(s.message)<<'\n';
    }
    if(!f) throw std::runtime_error("results write failed");
}
struct Stat {int n=0;double mean=0,m2=0;void add(double x) {if(!std::isfinite(x)) return;++n;double delta=x-mean;mean+=delta/n;m2+=delta*(x-mean);}double sd() const {return n>1?std::sqrt(m2/(n-1)):0;}};
double chartValue(const Record& r) {
    if(r.exp=="E1" || r.exp=="E3" || r.exp=="E6") return r.s.ms;
    if(r.exp=="E2") return r.gap();
    if(r.exp=="E4") return hasPlan(r.s) && r.before>0?100*(r.before-r.s.total)/r.before:NA;
    if(r.exp=="E5") return hasPlan(r.s)?r.s.total:NA;
    if(r.exp=="E7") return r.ratio();
    return static_cast<double>(r.s.nodesExpanded);
}
std::string xml(const std::string& s) {std::string out;for(char c:s) {if(c=='&') out+="&amp;";else if(c=='<') out+="&lt;";else if(c=='"') out+="&quot;";else out+=c;}return out;}
void summaries(const std::vector<Record>& records,const std::filesystem::path& dir) {
    std::map<std::string,std::vector<Stat>> groups;
    for(const auto& r:records) {
        std::string key=r.exp+","+std::to_string(r.n)+","+r.type+","+r.s.algo+","+r.sort+","+r.s.status;
        auto& v=groups[key];if(v.empty()) v.resize(11);
        v[0].add(r.s.ms);v[1].add(hasPlan(r.s)?r.s.total:NA);v[2].add(static_cast<double>(r.s.ops));v[3].add(static_cast<double>(r.s.comparisons));v[4].add(static_cast<double>(r.s.nodesExpanded));v[5].add(r.gap());v[6].add(r.ratio());v[7].add(r.savings());v[8].add(hasPlan(r.s)?static_cast<double>(r.s.routes.size()):NA);v[9].add(hasPlan(r.s)?r.longest():NA);v[10].add(static_cast<double>(r.s.passes));
    }
    std::ofstream f(dir/"summary.csv");if(!f) throw std::runtime_error("cannot write summary");f<<"experiment,n,type,algorithm,sort,status";
    for(auto metric:{"ms","distance_km","ops","comparisons","nodes","gap_pct","mst_ratio","savings_pct","vehicles","longest_km","passes"}) f<<','<<metric<<"_count,"<<metric<<"_mean,"<<metric<<"_sd";
    f<<'\n';for(const auto& g:groups) {f<<g.first;for(const auto& st:g.second) {f<<','<<st.n<<',';if(st.n) f<<st.mean;f<<',';if(st.n) f<<st.sd();}f<<'\n';}
    const char* colors[]={"#0369a1","#be123c","#15803d","#7e22ce","#b45309","#0891b2","#db2777","#475569","#65a30d","#9333ea","#ea580c","#0f766e","#1e40af","#a16207"};
    for(int e=1;e<=8;++e) {
        std::string exp="E"+std::to_string(e);std::map<std::string,std::map<int,Stat>> series;
        double maxY=0;int minN=100000,maxN=0;bool log=e==1 || e==8;
        auto scale=[&](double v){return log?std::log10(1+std::max(0.0,v)):v;};
        for(const auto& r:records) if(r.exp==exp) {double value=chartValue(r);if(!std::isfinite(value) || r.s.status=="unsupported") continue;std::string label=r.s.algo+"/"+r.type+(e==6?"/"+r.sort:"");series[label][r.n].add(value);minN=std::min(minN,r.n);maxN=std::max(maxN,r.n);}
        if(series.empty()) continue;
        for(const auto& s:series) for(const auto& p:s.second) maxY=std::max(maxY,scale(p.second.mean));
        maxY=std::max(maxY,1e-6)*1.1;
        std::string unit=e==1 || e==3 || e==6?"milliseconds":e==2?"gap to optimum (%)":e==4?"distance reduction (%)":e==5?"distance (km)":e==7?"solution / MST":"nodes expanded";
        std::ofstream g(dir/(exp+".svg"));if(!g) throw std::runtime_error("cannot write graph");
        g<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1100\" height=\"650\" viewBox=\"0 0 1100 650\"><rect width=\"1100\" height=\"650\" fill=\"white\"/><g font-family=\"sans-serif\" font-size=\"13\" fill=\"#222\"><text x=\"70\" y=\"30\" font-size=\"21\">"<<exp<<": "<<unit<<(log?" (log10(1+y) axis)":"")<<"</text><text x=\"70\" y=\"53\">Means of recorded runs; search-limit rows are censored measurements. See results.csv for status.</text>";
        for(int tick=0;tick<=5;++tick) {double y=570-tick*95;double val=tick*maxY/5;g<<"<path d=\"M70 "<<y<<" H770\" stroke=\"#ddd\"/><text x=\"5\" y=\""<<y+4<<"\">"<<std::setprecision(4)<<(log?std::pow(10,val)-1:val)<<"</text>";}
        auto x=[&](int n){return 70+700.0*(n-minN)/std::max(1,maxN-minN);};auto y=[&](double val){return 570-475*scale(val)/maxY;};
        std::set<int> ticks;int color=0;
        for(const auto& s:series) {const char* c=colors[color%14];g<<"<polyline fill=\"none\" stroke=\""<<c<<"\" stroke-width=\"2\" points=\"";for(const auto& p:s.second) g<<x(p.first)<<','<<y(p.second.mean)<<' ';g<<"\"/>";
            for(const auto& p:s.second) {ticks.insert(p.first);g<<"<circle cx=\""<<x(p.first)<<"\" cy=\""<<y(p.second.mean)<<"\" r=\"3\" fill=\""<<c<<"\"/>";}
            g<<"<text x=\"800\" y=\""<<90+color*22<<"\" fill=\""<<c<<"\">"<<xml(s.first)<<"</text>";++color;
        }
        for(int n:ticks) g<<"<text x=\""<<x(n)<<"\" y=\"595\">"<<n<<"</text>";
        g<<"<text x=\"350\" y=\"625\">number of customers</text></g></svg>\n";
    }
}
}
void benchmark(const BenchOptions& o) {
    if(o.runs<1 || o.runs>1000) throw std::runtime_error("runs must be 1..1000");
    std::filesystem::path out=o.out,dir=out.parent_path();if(dir.empty()) dir=".";std::filesystem::create_directories(dir);
    std::set<std::string> selected;
    if(o.experiments=="all") for(int i=1;i<=8;++i) selected.insert("E"+std::to_string(i));
    else {std::istringstream stream(o.experiments);std::string item;while(std::getline(stream,item,',')) {if(item.size()!=2 || item[0]!='E' || item[1]<'1' || item[1]>'8') throw std::runtime_error("experiments must be E1,...,E8 or all");selected.insert(item);}}
    std::vector<Record> records;
    const std::vector<std::vector<int>> defaults={{},{5,6,7,8,9,10,12,15},{8,10,12,15},{20,30,50,100,200,500},{8,15,30,100},{15,30},{50,100,200,500,1000},{8,12,15,30,100},{5,8,10,12}};
    for(int exp=1;exp<=8;++exp) {
        std::string tag="E"+std::to_string(exp);if(!selected.count(tag)) continue;
        auto sizes=o.sizes.empty()?defaults[exp]:o.sizes;
        for(int n:sizes) {
            if((exp==1 || exp==2) && n>15) continue;
            if(exp==8 && n>12) continue;
            if(exp==5 && n!=15 && n!=30) continue;
            for(int run=0;run<o.runs;++run) {
                unsigned seed=exp==5?7:o.seed+static_cast<unsigned>(run);std::vector<std::string> types=exp==8?std::vector<std::string>{"clustered","hard"}:exp==5?std::vector<std::string>{"illustrative_nashik"}:std::vector<std::string>{o.type};
                for(const auto& type:types) {
                    Instance in;std::string instance;
                    if(exp==5) {instance="data/nashik_tiffin_"+std::to_string(n)+".csv";in=readCSV(instance,n==15?3:5,20);}
                    else {in=generate(n,type,seed,exp==1?1:0,exp==1?std::max(20,3*n):20);instance="generated/"+type+"/"+std::to_string(n)+"/"+std::to_string(seed);}
                    double mst=prim(in),optimum=NA,base=NA;
                    if(n<=15) {auto exact=runSolver(in,"exact",o.solver);if(hasPlan(exact)) optimum=exact.total;}
                    auto b=runSolver(in,"baseline",o.solver);if(hasPlan(b)) base=b.total;
                    auto record=[&](const std::string& algo,bool heap=false,double before=NA) {
                        Options options=o.solver;options.heapSort=heap;Solution s=algo=="savings-sort"?benchmarkSavingsSort(in,heap):runSolver(in,algo,options);
                        if(hasPlan(s)) {verify(in,s);if(s.total+1e-7<mst || (std::isfinite(optimum) && s.total+1e-7<optimum)) throw std::runtime_error("benchmark consistency failure");}
                        records.push_back({tag,instance,type,heap?"heap":"std",n,run,seed,std::move(s),mst,optimum,base,before,NA});
                    };
                    if(exp==1) for(auto algo:{"bt","bb-fifo","bb-lifo","bb-lc","hk","exact"}) record(algo,o.solver.heapSort);
                    if(exp==2 || exp==3 || exp==7) for(auto algo:{"nn","cw","sweep","nn+2opt","cw+2opt","sweep+2opt"}) record(algo,o.solver.heapSort);
                    if(exp==4) for(auto algo:{"nn","cw","sweep"}) {auto s=runSolver(in,algo,o.solver);double before=hasPlan(s)?s.total:NA;std::uint64_t beforeOps=s.ops;record(algo,o.solver.heapSort);record(std::string(algo)+"+2opt",o.solver.heapSort,before);auto& r=records.back();r.extraOps=static_cast<double>(r.s.ops)-static_cast<double>(beforeOps);}
                    if(exp==5) for(auto algo:{"baseline","nn+2opt","cw+2opt","sweep+2opt","cw+local"}) record(algo,o.solver.heapSort);
                    if(exp==6) {record("savings-sort",false);record("savings-sort",true);}
                    if(exp==8) for(auto algo:{"bb-fifo","bb-lifo","bb-lc"}) record(algo,o.solver.heapSort);
                }
            }
            std::cout<<tag<<" n="<<n<<" complete ("<<records.size()<<" rows)\n"<<std::flush;
            writeRecords(records,o.out);
        }
    }
    if(records.empty()) throw std::runtime_error("no experiment sizes selected");
    writeRecords(records,o.out);summaries(records,dir);
    std::ofstream meta(dir/"run-info.txt");meta<<"C++17, standard library only; steady_clock solver timer; independent verifier outside timer.\nBase seed: "<<o.seed<<"\nRuns per size: "<<o.runs<<"\nSearch node/live-node limit: "<<o.solver.nodeLimit<<"\nSearch time limit ms: "<<o.solver.timeLimitMs<<"\nBuild: -std=c++17 -O2\nTime values depend on hardware and load. Standard deviation is sample SD.\nHard E8 instances use a synthetic near-equal symmetric metric, not Haversine.\nE1 uses one vehicle so Held-Karp and VRP solve the same problem.\nE5 repeats a fixed illustrative synthetic Nashik dataset (not independently sampled runs).\n";
    std::cout<<"Wrote "<<records.size()<<" rows to "<<o.out<<"; summary.csv and SVG graphs alongside it.\n";
}
}
