#include "RecordView3D.hpp"
#include "Presentation3D.hpp"
namespace Seis3D::Curvilinear::Ui {
void RecordView3D::draw(const ISimulationBackend3D& simulation,int& selected,bool* open){
    SeisWave::RecordDataView data;data.dt=simulation.dt();data.revision=simulation.revision();data.components={"Vz","Vx","Vy"};
    for(const auto& trace:simulation.receivers()){SeisWave::TraceView t;t.position=trace.position;t.channels={trace.samples,trace.samplesVx,trace.samplesVy};data.traces.push_back(t);}
    SeisWave::Ui::RecordView::draw(data,selected,label3D("Receiver records"),open);
}
}
