#include "benchmark.hpp"
#include "io.hpp"
#include "mst.hpp"
#include "json.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace {
const char* help=R"(VRP planner (C++17)
  vrp solve --input FILE --vehicles K --capacity Q --algo cw+2opt
  vrp compare --input FILE --vehicles K --capacity Q
  vrp gen --n 30 --type clustered --seed 7 --out data/gen_30.csv
  vrp bench --runs 10 --seed 42 --out results/results.csv
Options:
  --circuity 1.3             positive Haversine road multiplier
  --sort std|heap            Clarke-Wright savings sort
  --node-limit 1000000       expanded/live search node limit
  --time-limit-ms 5000       BT/B&B search time limit
  --format text|json        solve/compare output format
  solve: --routes routes.csv --svg routes.svg
  bench: --sizes 5,8,10,... --experiments all|E1,E2,...,E8
Algorithms: baseline, nn, cw, sweep, bt, bb-fifo, bb-lifo, bb-lc,
            hk (one route), exact (CVRP subset DP)
Constructive algorithms accept +2opt or +local.
Time columns are metadata; this MVP does not enforce time windows.
)";
long long integer(const std::string& s) {std::size_t used=0;long long v=std::stoll(s,&used);if(used!=s.size() || v<0) throw std::runtime_error("expected nonnegative integer: "+s);return v;}
int smallInt(const std::string& s) {auto v=integer(s);if(v>1000000) throw std::runtime_error("integer exceeds supported range");return static_cast<int>(v);}
double real(const std::string& s) {std::size_t used=0;double v=std::stod(s,&used);if(used!=s.size() || !std::isfinite(v) || v<=0) throw std::runtime_error("expected positive finite number: "+s);return v;}
void parent(const std::string& path) {auto p=std::filesystem::path(path).parent_path();if(!p.empty()) std::filesystem::create_directories(p);}
}
int main(int argc,char** argv) {try {
    if(argc<2 || std::string(argv[1])=="--help" || std::string(argv[1])=="help") {std::cout<<help;return 0;}
    std::string cmd=argv[1];std::set<std::string> allowed={"--sort","--node-limit","--time-limit-ms"};
    auto add=[&](std::initializer_list<const char*> flags){for(auto f:flags) allowed.insert(f);};
    if(cmd=="solve" || cmd=="compare") add({"--input","--vehicles","--capacity","--circuity","--format"});
    if(cmd=="solve") add({"--algo","--routes","--svg"});
    else if(cmd=="gen") add({"--n","--type","--seed","--out","--vehicles","--capacity"});
    else if(cmd=="bench") add({"--sizes","--runs","--seed","--out","--experiments","--type"});
    else if(cmd!="compare") throw std::runtime_error("unknown command: "+cmd);
    std::map<std::string,std::string> args;
    for(int i=2;i<argc;i+=2) {std::string key=argv[i];if(!allowed.count(key)) throw std::runtime_error("unknown option: "+key);if(i+1==argc) throw std::runtime_error("missing value for "+key);if(!args.emplace(key,argv[i+1]).second) throw std::runtime_error("duplicate option: "+key);}
    auto get=[&](const std::string& k,const std::string& fallback){auto i=args.find(k);return i==args.end()?fallback:i->second;};
    vrp::Options options;std::string sort=get("--sort","std");if(sort!="std" && sort!="heap") throw std::runtime_error("sort must be std or heap");options.heapSort=sort=="heap";options.nodeLimit=static_cast<std::uint64_t>(integer(get("--node-limit","1000000")));options.timeLimitMs=real(get("--time-limit-ms","5000"));
    if(cmd=="gen") {
        int n=smallInt(get("--n","30"));auto type=get("--type","uniform");
        if(type=="hard") throw std::runtime_error("hard near-equal metrics are available in E8 bench; CSV stores coordinates only");
        auto in=vrp::generate(n,type,static_cast<unsigned>(integer(get("--seed","42"))),smallInt(get("--vehicles","0")),smallInt(get("--capacity","20")));auto path=get("--out","data/generated.csv");parent(path);vrp::writeInstance(in,path);std::cout<<"Wrote "<<path<<'\n';return 0;
    }
    if(cmd=="bench") {
        vrp::BenchOptions b;b.solver=options;b.runs=smallInt(get("--runs","10"));b.seed=static_cast<unsigned>(integer(get("--seed","42")));b.out=get("--out","results/results.csv");b.experiments=get("--experiments","all");b.type=get("--type","uniform");
        if(args.count("--sizes")) {std::istringstream stream(args["--sizes"]);std::string item;while(std::getline(stream,item,',')) {int n=smallInt(item);if(n<1 || n>5000) throw std::runtime_error("sizes must be 1..5000");b.sizes.push_back(n);}if(b.sizes.empty()) throw std::runtime_error("empty sizes");}
        vrp::benchmark(b);return 0;
    }
    if(!args.count("--input")) throw std::runtime_error("--input is required");
    auto in=vrp::readCSV(args["--input"],smallInt(get("--vehicles","3")),smallInt(get("--capacity","20")),real(get("--circuity","1.3")));
    auto format=get("--format","text");if(format!="text" && format!="json") throw std::runtime_error("format must be text or json");
    if(cmd=="compare") {
        if(format=="json") {std::vector<vrp::Solution> solutions;for(const auto& algo:vrp::algorithms()) solutions.push_back(vrp::runSolver(in,algo,options));vrp::writeJSON(std::cout,in,solutions);}
        else vrp::printComparison(in,options);
        return 0;
    }
    auto s=vrp::runSolver(in,get("--algo","cw+2opt"),options),baseline=vrp::runSolver(in,"baseline",options);
    if(format=="json") {vrp::writeJSON(std::cout,in,{s,baseline});return vrp::hasPlan(s)?(s.status=="limit"?4:0):(s.status=="unsupported"?2:3);}
    vrp::printSolution(in,s,vrp::prim(in),vrp::hasPlan(baseline)?baseline.total:-1);
    if(!vrp::hasPlan(s)) return s.status=="unsupported"?2:3;
    auto routes=get("--routes","routes.csv"),svg=get("--svg","routes.svg");parent(routes);parent(svg);vrp::writeRoutes(in,s,routes);vrp::writeSVG(in,s,svg);
    std::cout<<"Verified; wrote "<<routes<<" and "<<svg<<'\n';return s.status=="limit"?4:0;
} catch(const std::exception& e) {std::cerr<<"Error: "<<e.what()<<'\n';return 1;}}
