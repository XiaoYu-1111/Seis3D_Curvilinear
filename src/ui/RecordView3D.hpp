#pragma once
#include "Seis3D/CurvilinearSolver.hpp"
#include "Seis3D/TraceProcessing.hpp"
namespace Seis3D::Curvilinear::Ui {
struct RecordSettings3D {
    int view{},component{},style{3},palette{},first{},visible{64};
    float gain{1},heatmapRange{1},opacity{.85f};
    bool normalizeEach{},autoFit{true},showAll{true};
    TraceProcessing::AgcSettings agc;
    bool operator==(const RecordSettings3D&) const = default;
};
// A presentation-only viewer. Its cache owns processed copies; raw backend samples are never modified.
class RecordView3D {
public:
    RecordSettings3D settings;
    void draw(const ISimulationBackend3D&,int& selectedReceiver,bool* open=nullptr);
    void invalidate(){revision_=0;fit_=true;}
private:
    struct Series {int receiver{},component{};double rawPeak{};TraceProcessing::TracePreview preview;};
    std::vector<Series> series_;
    std::vector<double> density_;
    std::uint64_t revision_{};
    RecordSettings3D cachedSettings_;
    int cachedSelected_{-1},first_{},count_{},densityRows_{};
    double rawPeak_{},commonPeak_{},endTime_{},dt_{};
    bool fit_{true};
    void options(int total);
    void refresh(const ISimulationBackend3D&,int selected);
    void section(const ISimulationBackend3D&,int& selected);
    void single(const ISimulationBackend3D&,int selected);
};
}
