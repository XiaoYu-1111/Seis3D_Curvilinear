#pragma once
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
namespace Seis3D::Curvilinear::Ui {
inline std::string formatMeasurement3D(double value) {
    if(!std::isfinite(value))return "--";
    std::ostringstream out;out.imbue(std::locale::classic());
    const double magnitude=std::abs(value);
    const int precision=magnitude>0&&magnitude<1?std::clamp(3-static_cast<int>(std::floor(std::log10(magnitude))),3,15):3;
    out<<std::fixed<<std::setprecision(precision)<<value;auto text=out.str();
    while(text.find('.')!=std::string::npos&&!text.empty()&&text.back()=='0')text.pop_back();
    if(!text.empty()&&text.back()=='.')text.pop_back();if(text=="-0")text="0";
    return text;
}
}
