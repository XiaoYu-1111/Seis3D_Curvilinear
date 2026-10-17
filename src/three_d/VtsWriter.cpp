#include "Seis3D/VtsWriter.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>

namespace Seis3D::Curvilinear {

bool writeVts(const SimulationResult& result, const std::filesystem::path& path, std::string& error) {
    const Grid3D& grid = result.grid;
    const int g = grid.ghost;
    const int nx = grid.nx - 2 * g, ny = grid.ny - 2 * g, nz = grid.nz - 2 * g;
    if(nx<1 || ny<1 || nz<1) { error="Cannot export an empty grid."; return false; }
    const std::size_t padded=static_cast<std::size_t>(grid.nx)*grid.ny*grid.nz;
    if(grid.coordinates.size()!=padded) { error="Coordinate buffer size does not match the grid."; return false; }
    for(const auto* values:{&result.vx,&result.vy,&result.vz,&result.sxx,&result.syy,&result.szz,&result.sxy,&result.sxz,&result.syz})
        if(values->size()!=padded) { error="Field buffer size does not match the grid."; return false; }
    for(const auto* values:{&result.materialVp,&result.materialVs,&result.materialDensity})if(!values->empty()&&values->size()!=padded){error="Material buffer size does not match the grid.";return false;}
    if(!result.layerIds.empty()&&result.layerIds.size()!=padded){error="Layer buffer size does not match the grid.";return false;}
    if(!result.allFinite) { error="Refusing to export non-finite simulation fields."; return false; }
    std::error_code ec;
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(),ec);
    if(ec) { error="Could not create export folder: "+ec.message(); return false; }
    std::ofstream out(path, std::ios::binary);
    if (!out) { error = "Could not open VTS output file: " + path.string(); return false; }
    out << std::setprecision(9);
    const auto direction=SeisWave::unitSourceDirection(result.sourceMechanism);
    out << "<?xml version=\"1.0\"?>\n"
        << "<VTKFile type=\"StructuredGrid\" version=\"1.0\" byte_order=\"LittleEndian\">\n"
        << "  <StructuredGrid WholeExtent=\"0 " << nx-1 << " 0 " << ny-1 << " 0 " << nz-1 << "\">\n"
        << "    <FieldData><DataArray type=\"Float64\" Name=\"TIME_VALUE\" NumberOfTuples=\"1\" format=\"ascii\">" << result.steps*result.dt << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SimulationDt\" NumberOfTuples=\"1\" format=\"ascii\">" << result.dt << "</DataArray>"
        << "<DataArray type=\"Int32\" Name=\"SimulationStep\" NumberOfTuples=\"1\" format=\"ascii\">" << result.steps << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceX\" NumberOfTuples=\"1\" format=\"ascii\">" << result.sourcePosition[0] << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceY\" NumberOfTuples=\"1\" format=\"ascii\">" << result.sourcePosition[1] << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceZ\" NumberOfTuples=\"1\" format=\"ascii\">" << result.sourcePosition[2] << "</DataArray>"
        << "<DataArray type=\"Int32\" Name=\"SourceKind\" NumberOfTuples=\"1\" format=\"ascii\">" << static_cast<int>(result.sourceMechanism.kind) << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceForceN\" NumberOfTuples=\"1\" format=\"ascii\">" << result.sourceForce << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceMomentNm\" NumberOfTuples=\"1\" format=\"ascii\">" << result.sourceMoment << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceFrequencyHz\" NumberOfTuples=\"1\" format=\"ascii\">" << result.sourceFrequency << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceDirectionX\" NumberOfTuples=\"1\" format=\"ascii\">" << direction[0] << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceDirectionY\" NumberOfTuples=\"1\" format=\"ascii\">" << direction[1] << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"SourceDirectionZ\" NumberOfTuples=\"1\" format=\"ascii\">" << direction[2] << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"GridSpacing\" NumberOfTuples=\"1\" format=\"ascii\">" << grid.dx << "</DataArray>"
        << "<DataArray type=\"Float64\" Name=\"TerrainTransitionDepth\" NumberOfTuples=\"1\" format=\"ascii\">" << grid.terrainTransitionDepth << "</DataArray></FieldData>\n"
        << "    <Piece Extent=\"0 " << nx-1 << " 0 " << ny-1 << " 0 " << nz-1 << "\">\n"
        << "      <PointData Scalars=\"Vz\" Vectors=\"Velocity\">\n";
    auto scalar = [&](const char* name, auto valueAt) {
        out << "        <DataArray type=\"Float32\" Name=\"" << name << "\" format=\"ascii\">\n          ";
        std::size_t written = 0;
        for (int k = g; k < grid.nz-g; ++k) for (int j = g; j < grid.ny-g; ++j) for (int i = g; i < grid.nx-g; ++i) {
            const std::size_t q = grid.index(i,j,k);
            out << valueAt(q) << ' ';
            if (++written % 6 == 0) out << "\n          ";
        }
        out << "\n        </DataArray>\n";
    };
    scalar("Vz", [&](std::size_t q) { return result.vz[q]; });
    scalar("Vp", [&](std::size_t q) { return result.materialVp.empty()?result.modelVp:result.materialVp[q]; });
    scalar("Vs", [&](std::size_t q) { return result.materialVs.empty()?result.modelVs:result.materialVs[q]; });
    scalar("Density", [&](std::size_t q) { return result.materialDensity.empty()?result.modelDensity:result.materialDensity[q]; });
    if(!result.layerIds.empty())scalar("LayerId",[&](std::size_t q){return result.layerIds[q];});
    scalar("VelocityMagnitude", [&](std::size_t q) { return std::sqrt(result.vx[q]*result.vx[q]+result.vy[q]*result.vy[q]+result.vz[q]*result.vz[q]); });
    scalar("MeanNormalStress", [&](std::size_t q) { return -(result.sxx[q]+result.syy[q]+result.szz[q])/3.0; });
    scalar("Sxx", [&](std::size_t q) { return result.sxx[q]; });
    scalar("Syy", [&](std::size_t q) { return result.syy[q]; });
    scalar("Szz", [&](std::size_t q) { return result.szz[q]; });
    scalar("Sxy", [&](std::size_t q) { return result.sxy[q]; });
    scalar("Sxz", [&](std::size_t q) { return result.sxz[q]; });
    scalar("Syz", [&](std::size_t q) { return result.syz[q]; });
    out << "        <DataArray type=\"Float32\" Name=\"Velocity\" NumberOfComponents=\"3\" format=\"ascii\">\n          ";
    std::size_t written = 0;
    for (int k = g; k < grid.nz-g; ++k) for (int j = g; j < grid.ny-g; ++j) for (int i = g; i < grid.nx-g; ++i) {
        const std::size_t q = grid.index(i,j,k);
        out << result.vx[q] << ' ' << result.vy[q] << ' ' << result.vz[q] << ' ';
        if (++written % 2 == 0) out << "\n          ";
    }
    out << "\n        </DataArray>\n      </PointData>\n      <CellData/>\n      <Points>\n"
        << "        <DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"ascii\">\n          ";
    written = 0;
    for (int k = g; k < grid.nz-g; ++k) for (int j = g; j < grid.ny-g; ++j) for (int i = g; i < grid.nx-g; ++i) {
        const auto& xyz = grid.coordinates[grid.index(i,j,k)];
        out << xyz[0] << ' ' << xyz[1] << ' ' << xyz[2] << ' ';
        if (++written % 2 == 0) out << "\n          ";
    }
    out << "\n        </DataArray>\n      </Points>\n    </Piece>\n  </StructuredGrid>\n</VTKFile>\n";
    if (!out.good()) { error = "Failed while writing VTS output: " + path.string(); return false; }
    return true;
}

} // namespace Seis3D::Curvilinear
