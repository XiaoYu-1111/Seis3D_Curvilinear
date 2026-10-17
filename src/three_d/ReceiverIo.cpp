#include "Seis3D/ReceiverIo.hpp"
#include <fstream>
#include <iomanip>

namespace Seis3D::Curvilinear {
bool writeReceiversCsv(const ISimulationBackend3D& simulation,const std::filesystem::path& path,std::string& error,bool allVelocityComponents) {
    if(!simulation.initialized()) { error="Initialize a simulation before exporting traces."; return false; }
    std::error_code ec;
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(),ec);
    if(ec) { error=ec.message(); return false; }
    std::ofstream out(path);
    if(!out) { error="Could not open receiver CSV."; return false; }
    out<<std::setprecision(17)<<(allVelocityComponents?"# Vx,Vy,Vz":"# Vz")<<" in m/s; time in seconds; coordinates in metres; depth positive down\n";
    const auto& traces=simulation.receivers();
    const auto& p=simulation.parameters();const auto direction=SeisWave::unitSourceDirection(p.source);
    out<<"# source_kind="<<static_cast<int>(p.source.kind)<<" (0=directional_force,1=isotropic_moment); force_N="<<p.sourceForce<<"; diagonal_moment_Nm="<<p.sourceMoment<<"; direction="<<direction[0]<<','<<direction[1]<<','<<direction[2]<<"; Ricker_f0_Hz="<<p.dominantFrequency<<'\n';
    const auto source=sourceIndices3D(simulation.parameters());const auto& g=simulation.grid();const auto& xyz=g.coordinates[g.index(source[0]+g.ghost,source[1]+g.ghost,source[2]+g.ghost)];
    out<<"# source XYZ (m): "<<xyz[0]<<','<<xyz[1]<<','<<xyz[2]<<"; grid IJK (zero-based): "<<source[0]<<','<<source[1]<<','<<source[2]<<'\n';
    for(std::size_t r=0;r<traces.size();++r) out<<"# receiver "<<r+1<<": "<<traces[r].position[0]<<','<<traces[r].position[1]<<','<<traces[r].position[2]<<'\n';
    out<<"time_s";
    for(std::size_t r=0;r<traces.size();++r) {
        if(allVelocityComponents)for(const char* c:{"Vx","Vy","Vz"})out<<",receiver_"<<r+1<<'_'<<c;
        else out<<",receiver_"<<r+1;
    }
    out<<'\n';
    for(int sample=0;sample<=simulation.currentStep();++sample) {
        out<<sample*simulation.dt();
        for(const auto& trace:traces) {
            if(allVelocityComponents)out<<','<<trace.samplesVx.at(static_cast<std::size_t>(sample))<<','<<trace.samplesVy.at(static_cast<std::size_t>(sample));
            out<<','<<trace.samples.at(static_cast<std::size_t>(sample));
        }
        out<<'\n';
    }
    if(!out.good()) { error="Receiver CSV write failed."; return false; }
    return true;
}
}
