#pragma once
#include "types.hpp"
namespace vrp {
double haversine(double lat1,double lon1,double lat2,double lon2);
void buildMatrix(Instance& in);
void evaluate(const Instance& in,Solution& s);
}
