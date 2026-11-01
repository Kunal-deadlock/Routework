#include "distance.hpp"
#include "metrics.hpp"
#include <algorithm>
#include <cmath>
namespace vrp {
double haversine(double a,double b,double c,double d) {
    const double r=std::acos(-1.0)/180;
    double x=std::sin((c-a)*r/2), y=std::sin((d-b)*r/2);
    double h=x*x+std::cos(a*r)*std::cos(c*r)*y*y;
    return 12742*std::asin(std::sqrt(std::clamp(h,0.0,1.0)));
}
void buildMatrix(Instance& in) {
    in.d.assign(in.n+1,std::vector<double>(in.n+1));
    for(int i=0;i<=in.n;++i) for(int j=0;j<i;++j)
        in.d[i][j]=in.d[j][i]=in.circuity*haversine(in.lat[i],in.lon[i],in.lat[j],in.lon[j]);
}
void evaluate(const Instance& in,Solution& s) {
    s.total=0;
    for(auto& r:s.routes) {
        r.load=0; r.dist=0;
        for(std::size_t i=1;i<r.nodes.size();++i) {
            r.dist+=edge(in,r.nodes[i-1],r.nodes[i]);
            if(r.nodes[i]) r.load+=in.demand[r.nodes[i]];
        }
        s.total+=r.dist;
    }
}
}
