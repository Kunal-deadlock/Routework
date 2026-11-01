#pragma once
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>
namespace vrp {
// Array-backed binary min heap: O(log n) push/pop, O(1) top, O(n) storage.
template<class T,class Less=std::less<T>> class MinHeap {
    std::vector<T> data;Less less;
public:
    explicit MinHeap(Less cmp=Less{}):less(cmp) {}
    bool empty() const {return data.empty();}
    std::size_t size() const {return data.size();}
    const T& top() const {if(empty()) throw std::runtime_error("empty heap");return data.front();}
    void push(T value) {
        data.push_back(std::move(value));std::size_t i=data.size()-1;
        while(i) {auto p=(i-1)/2;if(!less(data[i],data[p])) break;std::swap(data[i],data[p]);i=p;}
    }
    T pop() {
        if(empty()) throw std::runtime_error("empty heap");
        T out=std::move(data.front());data.front()=std::move(data.back());data.pop_back();
        std::size_t i=0;
        while(i*2+1<data.size()) {auto c=i*2+1;if(c+1<data.size() && less(data[c+1],data[c])) ++c;if(!less(data[c],data[i])) break;std::swap(data[c],data[i]);i=c;}
        return out;
    }
};
// In-place heapsort, O(n log n) time and O(1) auxiliary space.
template<class T,class Less=std::less<T>> void heapSort(std::vector<T>& a,Less less=Less{}) {
    auto down=[&](std::size_t i,std::size_t end) {
        while(i*2+1<end) {auto c=i*2+1;if(c+1<end && less(a[c],a[c+1])) ++c;if(!less(a[i],a[c])) break;std::swap(a[i],a[c]);i=c;}
    };
    for(std::size_t i=a.size()/2;i>0;--i) down(i-1,a.size());
    for(std::size_t end=a.size();end>1;--end) {std::swap(a[0],a[end-1]);down(0,end-1);}
}
}
