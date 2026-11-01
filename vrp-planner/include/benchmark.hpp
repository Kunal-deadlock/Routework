#pragma once
#include "types.hpp"
#include <string>
#include <vector>
namespace vrp {
bool hasPlan(const Solution& s);
Solution runSolver(const Instance& in,const std::string& algorithm,const Options& options={});
std::vector<std::string> algorithms();
void printSolution(const Instance& in,const Solution& s,double mst,double baseline);
void printComparison(const Instance& in,const Options& options);
struct BenchOptions {
    std::vector<int> sizes;
    int runs=10;
    unsigned seed=42;
    std::string out="results/results.csv",experiments="all",type="uniform";
    Options solver;
};
void benchmark(const BenchOptions& options);
}
