#pragma once
#include "types.hpp"
namespace vrp {
void validate(const Instance& in);
Instance readCSV(const std::string& path,int vehicles,int capacity,double circuity=1.3);
Instance generate(int n,const std::string& type,unsigned seed,int vehicles=0,int capacity=20);
void writeInstance(const Instance& in,const std::string& path);
void writeRoutes(const Instance& in,const Solution& s,const std::string& path);
void writeSVG(const Instance& in,const Solution& s,const std::string& path);
std::string csvQuote(const std::string& s);
}
