#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace SeisWave {
struct TraceView {
    std::array<double, 3> position{}, source{};
    std::array<std::span<const double>, 3> channels;
    double startTime{};
};
// Non-owning, dimension-independent raw record interface. Components retain
// their own physical meaning; the viewer derives magnitude only on demand.
struct RecordDataView {
    std::vector<TraceView> traces;
    std::vector<std::string> components;
    double dt{};
    std::uint64_t revision{};
    std::string units{"m/s"};
    std::size_t sampleCount(std::size_t r) const {
        return traces[r].channels[0].size();
    }
    double value(std::size_t r, int c, std::size_t t) const;
};
inline double RecordDataView::value(std::size_t r, int c, std::size_t t) const {
    if (c < static_cast<int>(components.size()))
        return traces[r].channels[c][t];
    double sum = 0;
    for (std::size_t k = 0; k < components.size(); ++k) {
        const auto v = traces[r].channels[k][t];
        sum += v * v;
    }
    return std::sqrt(sum);
}
struct ImportedTrace {
    std::array<double, 3> position{}, source{};
    double startTime{};
    std::vector<double> samples;
};
struct ImportedRecords {
    double dt{};
    std::string component{"Amplitude"}, units{"raw"}, description;
    std::vector<ImportedTrace> traces;
    RecordDataView view(std::uint64_t revision) const;
};
inline RecordDataView ImportedRecords::view(std::uint64_t revision) const {
    RecordDataView result;
    result.dt = dt;
    result.revision = revision;
    result.units = units;
    result.components = {component};
    for (const auto &trace : traces) {
        TraceView t;
        t.position = trace.position;
        t.source = trace.source;
        t.startTime = trace.startTime;
        t.channels[0] = trace.samples;
        result.traces.push_back(t);
    }
    return result;
}
} // namespace SeisWave
