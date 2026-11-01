#include "io.hpp"
#include "distance.hpp"
#include "verify.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
namespace vrp {
namespace {
std::vector<std::string> parse(const std::string& line) {
    std::vector<std::string> fields; std::string field; bool quoted=false,closed=false;
    for(std::size_t i=0;i<line.size();++i) {
        char c=line[i];
        if(quoted) { if(c=='"') { if(i+1<line.size() && line[i+1]=='"') {field+='"';++i;} else {quoted=false;closed=true;} } else field+=c; }
        else if(c==',') {fields.push_back(field);field.clear();closed=false;}
        else if(c=='"' && field.empty() && !closed) quoted=true;
        else {if(closed || c=='"') throw std::runtime_error("malformed CSV quoting");field+=c;}
    }
    if(quoted) throw std::runtime_error("unterminated CSV quote");
    fields.push_back(field);return fields;
}
double number(const std::string& s) {
    std::size_t used=0; double v=std::stod(s,&used);
    if(used!=s.size() || !std::isfinite(v)) throw std::runtime_error("invalid number: "+s);
    return v;
}
int integer(const std::string& s) {
    double v=number(s);
    if(v!=std::floor(v) || v<std::numeric_limits<int>::min() || v>std::numeric_limits<int>::max()) throw std::runtime_error("invalid integer: "+s);
    return static_cast<int>(v);
}
std::ofstream output(const std::string& path) {
    std::ofstream f(path);if(!f) throw std::runtime_error("cannot write "+path); f<<std::setprecision(12);return f;
}
}
std::string csvQuote(const std::string& s) { std::string t="\"";for(char c:s) {t+=c;if(c=='"') t+='"';}return t+'"'; }
void validate(const Instance& in) {
    if(in.n<1) throw std::runtime_error("input must contain a depot and at least one customer");
    if(in.capacity<=0 || in.numVehicles<=0) throw std::runtime_error("capacity and vehicles must be positive");
    if(!std::isfinite(in.circuity) || in.circuity<=0) throw std::runtime_error("circuity must be positive and finite");
    std::size_t sz=static_cast<std::size_t>(in.n+1);
    if(in.id.size()!=sz || in.name.size()!=sz || in.lat.size()!=sz || in.lon.size()!=sz || in.demand.size()!=sz || in.twOpen.size()!=sz || in.twClose.size()!=sz || in.service.size()!=sz) throw std::runtime_error("inconsistent instance fields");
    std::set<int> ids;long long sum=0;
    for(int i=0;i<=in.n;++i) {
        if(!ids.insert(in.id[i]).second) throw std::runtime_error("duplicate id: "+std::to_string(in.id[i]));
        if(!std::isfinite(in.lat[i]) || !std::isfinite(in.lon[i]) || std::abs(in.lat[i])>90 || std::abs(in.lon[i])>180) throw std::runtime_error("bad coordinates");
        if(in.demand[i]<0 || in.demand[i]>in.capacity) throw std::runtime_error("demand outside [0, capacity]");
        if(in.twOpen[i]<0 || in.twClose[i]<in.twOpen[i] || in.service[i]<0) throw std::runtime_error("invalid time metadata");
        sum+=in.demand[i];
    }
    if(in.demand[0]!=0) throw std::runtime_error("depot demand must be zero");
    if(sum>static_cast<long long>(in.capacity)*in.numVehicles) throw std::runtime_error("total demand exceeds fleet capacity");
}
Instance readCSV(const std::string& path,int vehicles,int capacity,double circuity) {
    std::ifstream f(path);if(!f) throw std::runtime_error("cannot read "+path);
    Instance in;in.numVehicles=vehicles;in.capacity=capacity;in.circuity=circuity;
    std::string line;int row=0;bool depot=false;
    while(std::getline(f,line)) {
        ++row;if(!line.empty() && line.back()=='\r') line.pop_back();
        if(row==1 && line.size()>=3 && static_cast<unsigned char>(line[0])==239) line.erase(0,3);
        if(line.empty()) continue;
        try {
            auto a=parse(line);
            if(in.name.empty() && a[0]=="type") {
                if(a!=std::vector<std::string>{"type","id","name","lat","lon","demand","tw_open","tw_close","service"}) throw std::runtime_error("unexpected CSV header");
                continue;
            }
            if(a.size()!=9) throw std::runtime_error("expected 9 columns");
            if(a[0]=="depot") {if(depot || !in.name.empty()) throw std::runtime_error("exactly one depot must come first");depot=true;}
            else if(a[0]!="customer" || !depot) throw std::runtime_error("expected depot then customer rows");
            in.id.push_back(integer(a[1]));in.name.push_back(a[2]);in.lat.push_back(number(a[3]));in.lon.push_back(number(a[4]));in.demand.push_back(integer(a[5]));in.twOpen.push_back(integer(a[6]));in.twClose.push_back(integer(a[7]));in.service.push_back(integer(a[8]));
        } catch(const std::exception& e) {throw std::runtime_error("CSV row "+std::to_string(row)+": "+e.what());}
    }
    in.n=static_cast<int>(in.name.size())-1;validate(in);buildMatrix(in);return in;
}
Instance generate(int n,const std::string& type,unsigned seed,int vehicles,int capacity) {
    if(n<1 || n>5000) throw std::runtime_error("generator n must be 1..5000");
    if(type!="uniform" && type!="clustered" && type!="hard") throw std::runtime_error("type must be uniform, clustered or hard");
    if(capacity<1) throw std::runtime_error("capacity must be positive");
    Instance in;in.n=n;in.capacity=capacity;in.numVehicles=vehicles>0?vehicles:std::max(1,(n+4)/5);
    std::mt19937 rng(seed);
    auto unit=[&](){return static_cast<double>(rng())/4294967296.0;};
    for(int i=0;i<=n;++i) {
        double x=0,y=0;
        if(i) {
            x=(unit()-.5)*.12;y=(unit()-.5)*.12;
            if(type=="clustered") {int c=(i-1)%3;x=x*.15+(.04*std::cos(c*2.094));y=y*.15+(.04*std::sin(c*2.094));}
            if(type=="hard") {x=(unit()-.5)*.00002;y=(unit()-.5)*.00002;}
        }
        in.id.push_back(i);in.name.push_back(i?"Stop "+std::to_string(i):"Depot");in.lat.push_back(19.9975+x);in.lon.push_back(73.7898+y);in.demand.push_back(i?1+static_cast<int>(rng()%static_cast<unsigned>(std::min(3,capacity))):0);in.twOpen.push_back(0);in.twClose.push_back(1440);in.service.push_back(i?3:0);
    }
    // Hard metric: all off-diagonal distances nearly equal; coordinates remain illustrative.
    validate(in);buildMatrix(in);
    if(type=="hard") for(int i=0;i<=n;++i) for(int j=0;j<i;++j) in.d[i][j]=in.d[j][i]=1.0+unit()*.0001;
    return in;
}
void writeInstance(const Instance& in,const std::string& path) {
    auto f=output(path);f<<"type,id,name,lat,lon,demand,tw_open,tw_close,service\n";
    for(int i=0;i<=in.n;++i) f<<(i?"customer":"depot")<<','<<in.id[i]<<','<<csvQuote(in.name[i])<<','<<in.lat[i]<<','<<in.lon[i]<<','<<in.demand[i]<<','<<in.twOpen[i]<<','<<in.twClose[i]<<','<<in.service[i]<<'\n';
    if(!f) throw std::runtime_error("instance write failed");
}
void writeRoutes(const Instance& in,const Solution& s,const std::string& path) {
    verify(in,s);auto f=output(path);f<<"vehicle,sequence,stop_id,name,cumulative_km,arrival_minutes\n";
    for(std::size_t v=0;v<s.routes.size();++v) {double km=0,time=0;const auto& r=s.routes[v];
        for(std::size_t j=0;j<r.nodes.size();++j) {int i=r.nodes[j];if(j) {double d=in.d[r.nodes[j-1]][i];km+=d;time+=in.service[r.nodes[j-1]]+d*3;} f<<v+1<<','<<j<<','<<in.id[i]<<','<<csvQuote(in.name[i])<<','<<km<<','<<time<<'\n';}
    }
    if(!f) throw std::runtime_error("routes write failed");
}
void writeSVG(const Instance& in,const Solution& s,const std::string& path) {
    verify(in,s);auto f=output(path);
    auto xlim=std::minmax_element(in.lon.begin(),in.lon.end()),ylim=std::minmax_element(in.lat.begin(),in.lat.end());
    auto x=[&](int i){return 40+720*(in.lon[i]-*xlim.first)/std::max(1e-9,*xlim.second-*xlim.first);};
    auto y=[&](int i){return 560-520*(in.lat[i]-*ylim.first)/std::max(1e-9,*ylim.second-*ylim.first);};
    f<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\" viewBox=\"0 0 800 600\"><rect width=\"800\" height=\"600\" fill=\"white\"/>\n";
    const char* colors[]={"#be123c","#0369a1","#15803d","#7e22ce","#b45309"};
    for(std::size_t j=0;j<s.routes.size();++j) {f<<"<polyline fill=\"none\" stroke=\""<<colors[j%5]<<"\" stroke-width=\"2\" points=\"";for(int i:s.routes[j].nodes) f<<x(i)<<','<<y(i)<<' ';f<<"\"/>\n";}
    for(int i=0;i<=in.n;++i) f<<"<circle cx=\""<<x(i)<<"\" cy=\""<<y(i)<<"\" r=\""<<(i?4:7)<<"\"/><text x=\""<<x(i)+8<<"\" y=\""<<y(i)-5<<"\" font-size=\"12\">"<<in.id[i]<<"</text>\n";
    f<<"</svg>\n";if(!f) throw std::runtime_error("SVG write failed");
}
}
