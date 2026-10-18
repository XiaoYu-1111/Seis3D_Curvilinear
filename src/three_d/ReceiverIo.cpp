#include "Seis3D/ReceiverIo.hpp"
#include <fstream>
#include <iomanip>
#include <locale>
#include <algorithm>

namespace Seis3D::Curvilinear {
namespace {
bool writeCsv(const ISimulationBackend3D& simulation,const std::filesystem::path& path,
              const std::array<bool,3>& selected,std::string& error,bool legacyVzHeader) {
    error.clear();
    if(std::none_of(selected.begin(),selected.end(),[](bool v){return v;})){error="Select at least one receiver component.";return false;}
    constexpr const char* names[]{"Vx","Vy","Vz"};
    if(!simulation.initialized()) { error="Initialize a simulation before exporting traces."; return false; }
    std::error_code ec;
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(),ec);
    if(ec) { error=ec.message(); return false; }
    std::ofstream out(path);
    if(!out) { error="Could not open receiver CSV."; return false; }
    out.imbue(std::locale::classic());out<<std::setprecision(17)<<"# ";
    bool comma=false;for(int c=0;c<3;++c)if(selected[c]){if(comma)out<<',';out<<names[c];comma=true;}
    out<<" in m/s; time in seconds; coordinates in metres; depth positive down\n";
    const auto& traces=simulation.receivers();
    const auto& p=simulation.parameters();const auto direction=SeisWave::unitSourceDirection(p.source);
    out<<"# source_kind="<<static_cast<int>(p.source.kind)<<" (0=directional_force,1=isotropic_moment); force_N="<<p.sourceForce<<"; diagonal_moment_Nm="<<p.sourceMoment<<"; direction="<<direction[0]<<','<<direction[1]<<','<<direction[2]<<"; Ricker_f0_Hz="<<p.dominantFrequency<<'\n';
    const auto source=sourceIndices3D(simulation.parameters());const auto& g=simulation.grid();const auto& xyz=g.coordinates[g.index(source[0]+g.ghost,source[1]+g.ghost,source[2]+g.ghost)];
    out<<"# source XYZ (m): "<<xyz[0]<<','<<xyz[1]<<','<<xyz[2]<<"; grid IJK (zero-based): "<<source[0]<<','<<source[1]<<','<<source[2]<<'\n';
    for(std::size_t r=0;r<traces.size();++r) out<<"# receiver "<<r+1<<": "<<traces[r].position[0]<<','<<traces[r].position[1]<<','<<traces[r].position[2]<<'\n';
    out<<"time_s";
    for(std::size_t r=0;r<traces.size();++r) {
        for(int c=0;c<3;++c)if(selected[c]){out<<",receiver_"<<r+1;if(!legacyVzHeader)out<<'_'<<names[c];}
    }
    out<<'\n';
    for(int sample=0;sample<=simulation.currentStep();++sample) {
        out<<sample*simulation.dt();
        for(const auto& trace:traces) {
            const std::array<const std::vector<double>*,3> samples{&trace.samplesVx,&trace.samplesVy,&trace.samples};
            for(int c=0;c<3;++c)if(selected[c])out<<','<<samples[c]->at(static_cast<std::size_t>(sample));
        }
        out<<'\n';
    }
    if(!out.good()) { error="Receiver CSV write failed."; return false; }
    return true;
}
} // namespace
bool writeReceiversCsv(const ISimulationBackend3D& simulation,const std::filesystem::path& path,std::string& error,bool allVelocityComponents) {
    return writeCsv(simulation,path,{allVelocityComponents,allVelocityComponents,true},error,!allVelocityComponents);
}
bool writeReceiversCsv(const ISimulationBackend3D& simulation,const std::filesystem::path& path,const std::array<bool,3>& selected,std::string& error) {
    return writeCsv(simulation,path,selected,error,false);
}
} // namespace Seis3D::Curvilinear
