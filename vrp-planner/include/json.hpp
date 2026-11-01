#pragma once
#include "types.hpp"
#include <iosfwd>
namespace vrp {
// Machine-readable, independently verified solver results for the local web UI.
void writeJSON(std::ostream& out,const Instance& in,const std::vector<Solution>& solutions);
}
