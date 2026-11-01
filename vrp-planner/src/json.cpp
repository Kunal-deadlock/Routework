#include "json.hpp"
#include "benchmark.hpp"
#include "mst.hpp"
#include "verify.hpp"
#include <iomanip>
#include <ostream>
namespace vrp {
namespace {
void string(std::ostream& out,const std::string& value) {
    out<<'"';
    const char* hex="0123456789abcdef";
    for(unsigned char c:value) {
        switch(c) {
            case '"': out<<"\\\"";break;
            case '\\': out<<"\\\\";break;
            case '\n': out<<"\\n";break;
            case '\r': out<<"\\r";break;
            case '\t': out<<"\\t";break;
            default: if(c<32) out<<"\\u00"<<hex[c>>4]<<hex[c&15];else out<<static_cast<char>(c);
        }
    }
    out<<'"';
}
}
void writeJSON(std::ostream& out,const Instance& in,const std::vector<Solution>& solutions) {
    out<<std::setprecision(15);
    out<<"{\"instance\":{\"vehicles\":"<<in.numVehicles<<",\"capacity\":"<<in.capacity<<",\"circuity\":"<<in.circuity<<",\"stops\":[";
    for(int i=0;i<=in.n;++i) {
        if(i) out<<',';
        out<<"{\"index\":"<<i<<",\"id\":"<<in.id[i]<<",\"name\":";string(out,in.name[i]);
        out<<",\"lat\":"<<in.lat[i]<<",\"lon\":"<<in.lon[i]<<",\"demand\":"<<in.demand[i]<<",\"service\":"<<in.service[i]<<'}';
    }
    out<<"]},\"mst\":"<<prim(in)<<",\"solutions\":[";
    bool first=true;
    for(const auto& s:solutions) {
        if(hasPlan(s)) verify(in,s);
        if(!first) out<<',';
        first=false;
        out<<"{\"algo\":";string(out,s.algo);out<<",\"status\":";string(out,s.status);out<<",\"message\":";string(out,s.message);
        out<<",\"verified\":"<<(hasPlan(s)?"true":"false")<<",\"optimal\":"<<(s.optimal?"true":"false")<<",\"total\":";
        if(hasPlan(s)) out<<s.total;else out<<"null";
        out<<",\"ms\":"<<s.ms<<",\"ops\":"<<s.ops<<",\"comparisons\":"<<s.comparisons<<",\"nodesExpanded\":"<<s.nodesExpanded<<",\"passes\":"<<s.passes<<",\"routes\":[";
        for(std::size_t r=0;r<s.routes.size();++r) {
            if(r) out<<',';
            const auto& route=s.routes[r];out<<"{\"load\":"<<route.load<<",\"dist\":"<<route.dist<<",\"nodes\":[";
            for(std::size_t j=0;j<route.nodes.size();++j) {if(j) out<<',';out<<route.nodes[j];}
            out<<"],\"visits\":[";double distance=0,arrival=0;
            for(std::size_t j=0;j<route.nodes.size();++j) {
                int index=route.nodes[j];
                if(j) {int previous=route.nodes[j-1];double leg=in.d[previous][index];distance+=leg;arrival+=in.service[previous]+leg*3;out<<',';}
                out<<"{\"index\":"<<index<<",\"km\":"<<distance<<",\"arrival\":"<<arrival<<'}';
            }
            out<<"]}";
        }
        out<<"]}";
    }
    out<<"]}\n";
}
}
