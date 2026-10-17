#pragma once
#include "Seis3D/CurvilinearSolver.hpp"
#include "RecordView.hpp"
namespace Seis3D::Curvilinear::Ui {
using RecordSettings3D=SeisWave::Ui::RecordSettings;
class RecordView3D:public SeisWave::Ui::RecordView {
public:void draw(const ISimulationBackend3D&,int& selected,bool* open=nullptr);
};
}
