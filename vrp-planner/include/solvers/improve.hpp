#pragma once
#include "types.hpp"
namespace vrp {
bool twoOpt(Route& route,const Instance& in,std::uint64_t& passes);
void improveTwoOpt(const Instance& in,Solution& s);
void improveLocal(const Instance& in,Solution& s);
}
