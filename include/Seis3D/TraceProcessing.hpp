#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

// Stateless CPU processing shared by viewers; simulation contexts and recorded samples stay separate.
namespace Seis3D::Curvilinear::TraceProcessing {
struct AgcSettings {
    bool enabled=false;
    float windowMs=120,strength=1,maxBoost=50,noiseFloorPercent=.01f;
    bool operator==(const AgcSettings&) const = default;
};
struct TracePreview {
    std::vector<double> times,values;
    double peak=0;
};
// Apply centered RMS boost to a display copy, suppressing quiet windows and bounding maximum gain.
inline void applyAgc(std::vector<double>& values,const std::vector<double>& times,const AgcSettings& settings) {
    if(values.size()!=times.size()) throw std::invalid_argument("Trace samples and timestamps differ in length.");
    for(std::size_t i=0;i<values.size();++i)
        if(!std::isfinite(values[i])||!std::isfinite(times[i])||(i&&times[i]<times[i-1]))
            throw std::invalid_argument("Trace contains invalid samples or timestamps.");
    if(settings.enabled&&(!std::isfinite(settings.windowMs)||!std::isfinite(settings.strength)||
        !std::isfinite(settings.maxBoost)||!std::isfinite(settings.noiseFloorPercent)))
        throw std::invalid_argument("AGC settings must be finite.");
    if(!settings.enabled||values.empty()||settings.strength<=0||settings.maxBoost<=1) return;
    const double window=std::clamp(double(settings.windowMs),1.0,5000.0)*.001;
    const double strength=std::clamp(double(settings.strength),0.0,1.0),cap=std::clamp(double(settings.maxBoost),1.0,1000.0);
    double peak=0,total=0;for(double value:values) peak=std::max(peak,std::abs(value));
    if(peak==0) return;
    for(double value:values) {const double normalized=value/peak;total+=normalized*normalized;}
    const double target=std::sqrt(total/values.size()),floor=std::clamp(double(settings.noiseFloorPercent),0.0,10.0)*.01;
    std::vector<double> corrected(values.size());std::size_t left=0,right=0;double energy=0;
    for(std::size_t i=0;i<values.size();++i) {
        const double start=times[i]-window*.5,end=times[i]+window*.5;
        while(right<times.size()&&times[right]<=end) {const double value=values[right++]/peak;energy+=value*value;}
        while(left<right&&times[left]<start) {
            const double value=values[left++]/peak,removed=value*value;energy-=removed;
            if(energy<removed*1e-10) {energy=0;for(std::size_t j=left;j<right;++j) {const double current=values[j]/peak;energy+=current*current;}}
        }
        const double rms=std::sqrt(std::max(0.0,energy/double(std::max(std::size_t(1),right-left))));
        const double boost=std::clamp(std::pow(std::max(1.0,target/std::max(rms,std::max(floor,1e-30))),strength),1.0,cap);
        const double gate=floor>0?std::clamp((rms-floor)/floor,0.0,1.0):1.0;
        corrected[i]=values[i]*(1+gate*(boost-1));
    }
    values.swap(corrected);
}
// Preserve signed bucket extrema and endpoints in a preview bounded to 2048 points.
inline TracePreview reduceExtrema(const std::vector<double>& values,const std::vector<double>& times) {
    if(values.size()!=times.size()) throw std::invalid_argument("Trace samples and timestamps differ in length.");
    TracePreview result;if(values.empty()) return result;
    int last=-1;
    const auto append=[&](int i){if(i!=last) {result.values.push_back(values[i]);result.times.push_back(times[i]);last=i;}};
    for(double value:values) result.peak=std::max(result.peak,std::abs(value));
    if(values.size()<=2048) {result.values=values;result.times=times;return result;}
    append(0);const int count=int(values.size()),bucket=(count+1022)/1023;
    for(int begin=0;begin<count;begin+=bucket) {
        int low=begin,high=begin;
        for(int i=begin+1;i<std::min(begin+bucket,count);++i) {if(values[i]<values[low]) low=i;if(values[i]>values[high]) high=i;}
        append(std::min(low,high));append(std::max(low,high));
    }
    append(count-1);return result;
}

// Build a bounded time-by-channel density from processed previews, preserving signed bucket peaks and delays.
template<class Times,class Values,class Peak>
inline std::vector<double> buildTraceDensity(int columns,int rows,double timeMin,double timeMax,double commonPeak,
    bool normalizeEach,Times timesFor,Values valuesFor,Peak peakFor) {
    if(columns<1||rows<1||!std::isfinite(timeMin)||!std::isfinite(timeMax)||timeMax<=timeMin) return {};
    std::vector<double> density(std::size_t(columns)*rows,0);
    for(int column=0;column<columns;++column) {
        const auto& times=timesFor(column);const auto& values=valuesFor(column);
        if(times.empty()||times.size()!=values.size()) continue;
        const double scale=normalizeEach?peakFor(column):commonPeak;
        if(scale<=0) continue;
        std::size_t cursor=0;
        for(int row=0;row<rows;++row) {
            const double start=timeMin+(timeMax-timeMin)*row/rows;
            const double finish=timeMin+(timeMax-timeMin)*(row+1)/rows;
            while(cursor<times.size()&&times[cursor]<start) ++cursor;
            double value=0;bool found=false;
            while(cursor<times.size()&&(times[cursor]<finish||(row==rows-1&&times[cursor]<=finish))) {
                if(!found||std::abs(values[cursor])>std::abs(value)) value=values[cursor];
                found=true;++cursor;
            }
            const double center=(start+finish)*.5;
            if(!found&&center>=times.front()&&center<=times.back()) {
                if(cursor==0) value=values.front();
                else if(cursor>=times.size()) value=values.back();
                else {
                    const double span=times[cursor]-times[cursor-1];
                    const double fraction=span>0?std::clamp((center-times[cursor-1])/span,0.0,1.0):0;
                    value=values[cursor-1]+fraction*(values[cursor]-values[cursor-1]);
                }
            }
            density[std::size_t(row)*columns+column]=value/scale;
        }
    }
    return density;
}
}
