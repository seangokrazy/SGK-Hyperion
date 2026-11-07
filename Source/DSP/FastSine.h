#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace sgk::research {
// Worker-prepared shared table. Linear interpolation error is below 7.4e-8
// against sin(2*pi*phase); callback use has no allocation or initialization.
struct FastSine {
    static constexpr unsigned points=8192;
    std::array<double,points+1> values{};
    FastSine(){for(unsigned i=0;i<=points;++i)values[i]=std::sin(6.28318530717958647692*i/points);}
    static const double* prepare(){static const FastSine table;return table.values.data();}
    static double at(const double* table,double phase)noexcept{
        const double position=phase*points;const auto index=std::min(unsigned(position),points-1);const double f=position-index;
        return table[index]+f*(table[index+1]-table[index]);
    }
};
}
