#include "Seis2D/ModelIo2D.hpp"
#include "SeisCommon/SegyIo.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

using namespace Seis2D::Curvilinear;
namespace {
void check(bool value, const std::string &why) {
    if (!value) throw std::runtime_error(why);
}
std::string text(const std::filesystem::path &file) {
    std::ifstream in(file);
    return {std::istreambuf_iterator<char>(in), {}};
}
void replace(std::string &s, const std::string &from, const std::string &to) {
    const auto pos = s.find(from);
    check(pos != std::string::npos, "Missing corruption fixture text");
    s.replace(pos, from.size(), to);
}
Parameters fixture() {
    Parameters p;
    p.nx = 64; p.nz = 72; p.spacing = 2.5;
    p.relief = 1; p.wavelength = 400; p.sourceZ = 16;
    p.receiverCount = 8; p.receiverStride = 3; p.receiverDepth = 12;
    p.duration = .01; p.maxSteps = 13;
    p.source.direction = {2, 0, 3};
    return p;
}
void roundTrip(const std::filesystem::path &out) {
    std::string error;
    ComputeSettings3D settings; settings.cpuThreads = 1;
    for (int mode = 0; mode < 3; ++mode) {
        auto p = fixture();
        p.sampleIntervalMs = .5;
        p.layered = mode != 0;
        p.layers = {{10, 1500, 700, 2050}, {0, 3200, 1800, 2250}};
        if (mode == 1) {
            p.importedSurface = true; p.surfaceDatum = 1300;
            p.surfaceSamples = {{0, 1299}, {30, 1301}, {80, 1299.5}, {160, 1300}};
        }
        if (mode == 2) p.source.kind = SeisWave::SourceKind::IsotropicMoment;
        Simulation a;
        check(a.initialize(p, error, BackendKind3D::Cpu, settings), error);
        check(a.advance(7, error), error);
        const auto path = out / ("model-" + std::to_string(mode) + ".vts");
        check(writeModelVts(a, path, settings, error), error);
        ModelDocument document;
        check(readModelVts(path, document, error), error);
        check(document.parameters == p && document.compute == settings && document.backend == BackendKind3D::Cpu,
              "VTS metadata round-trip changed parameters");
        Simulation b;
        check(b.initialize(document.parameters, error, document.backend, document.compute), error);
        check(b.step() == 0 && b.dt() == a.dt() && b.dt() == document.referenceDt &&
                  b.grid().coordinates == a.grid().coordinates && b.grid().vp == a.grid().vp &&
                  b.grid().vs == a.grid().vs && b.grid().density == a.grid().density,
              "VTS geometry/material/dt round-trip differs");
        check(b.advance(7, error), error);
        check(b.snapshot().fields == a.snapshot().fields && b.receivers().front().vz == a.receivers().front().vz,
              "Restored model did not reproduce CPU wavefield / records");
        const auto saved = document.parameters;
        auto bad = text(path);
        replace(bad, "<DataArray type=\"Float64\" Name=\"Vp\" format=\"ascii\">\n", "<DataArray type=\"Float64\" Name=\"Vp\" format=\"ascii\">\n999 ");
        const auto invalid = out / "invalid-model.vts";
        std::ofstream(invalid) << bad;
        check(!readModelVts(invalid, document, error) && document.parameters == saved,
              "Damaged VTS was accepted or changed output document");
        bad = text(path);
        replace(bad, "SeisWaveModel2DVersion", "UnsupportedModelVersion");
        std::ofstream(invalid) << bad;
        check(!readModelVts(invalid, document, error), "Missing schema was accepted");
        bad = text(path);
        replace(bad, "Name=\"Parameter_frequency\" NumberOfComponents=\"1\" NumberOfTuples=\"1\" format=\"ascii\">30 ",
                     "Name=\"Parameter_frequency\" NumberOfComponents=\"1\" NumberOfTuples=\"1\" format=\"ascii\">31 ");
        std::ofstream(invalid) << bad;
        check(!readModelVts(invalid, document, error), "Changed metadata passed integrity verification");
        bad = text(path); bad.resize(bad.size() / 2);
        std::ofstream(invalid) << bad;
        check(!readModelVts(invalid, document, error), "Truncated VTS was accepted");
        check(writeSnapshot(a, out / "wavefield.vts", error), error);
        check(!readModelVts(out / "wavefield.vts", document, error), "Legacy wave snapshot accepted as a model");
    }
}
void sampling(const std::filesystem::path &out, BackendKind3D backend) {
    auto p = fixture();
    ComputeSettings3D settings; settings.cpuThreads = 1;
    std::string error;
    Simulation reference;
    check(reference.initialize(p, error, backend, settings), error);
    check(reference.advance(13, error), error);
    const double dt = reference.dt();
    for (double ms : {.5, 1., .73, .01}) {
        p.sampleIntervalMs = ms;
        Simulation sampled;
        check(sampled.initialize(p, error, backend, settings), error);
        check(sampled.dt() == dt, "Output interval changed solver dt");
        check(sampled.advance(5, error) && sampled.advance(8, error), error);
        const double interval = ms * .001;
        check(sampled.recordDt() == interval, "Incorrect record interval");
        const auto expected = static_cast<std::size_t>(std::floor((13 * dt + 1e-9 * dt) / interval)) + 1;
        check(sampled.receivers().front().vx.size() == expected, "Wrong uniform sample count / off-grid final sample");
        for (std::size_t r = 0; r < sampled.receivers().size(); ++r) {
            for (std::size_t n = 0; n < expected; ++n) {
                const double u = n * interval / dt;
                const auto lower = std::min(static_cast<std::size_t>(u), reference.receivers()[r].vx.size() - 2);
                const double a = std::clamp(u - lower, 0., 1.);
                for (int c = 0; c < 2; ++c) {
                    const auto &raw = c ? reference.receivers()[r].vz : reference.receivers()[r].vx;
                    const auto &record = c ? sampled.receivers()[r].vz : sampled.receivers()[r].vx;
                    const double expectedValue = std::lerp(raw[lower], raw[lower+1], a);
                    // A zero crossing can amplify relative error against the interpolated result.
                    // Scale by adjacent accepted states instead of dividing by a cancellation residual.
                    check(std::abs(record[n] - expectedValue) <
                              1e-12 * std::max({1e-25, std::abs(raw[lower]), std::abs(raw[lower+1])}),
                          "Time interpolation differs from accepted states");
                }
            }
        }
        check(sampled.snapshot().fields == reference.snapshot().fields, "Sampling changed numerical propagation");
        SeisWave::RecordDataView data;
        data.dt = sampled.recordDt(); data.components = {"Vz", "Vx"};
        for (const auto &r : sampled.receivers()) {
            SeisWave::TraceView trace;
            trace.position = {r.position[0], 0, r.position[1]};
            trace.channels[0] = r.vz; trace.channels[1] = r.vx;
            data.traces.push_back(trace);
        }
        const auto path = out / ("records-" + std::to_string(static_cast<int>(backend)) + "-" + std::to_string(ms) + ".sgy");
        check(SeisWave::writeSegy(data, 0, path, error), error);
        SeisWave::ImportedRecords imported;
        check(SeisWave::readSegy(path, imported, error), error);
        const auto view = imported.view(1);
        check(std::abs(view.dt - interval) < 1e-12 && view.traces.size() == data.traces.size() &&
                  view.traces.front().channels[0].size() == expected, "SEG-Y header does not describe sampled records");
        check(writeReceivers(sampled, out / "sampled-records.csv", error), error);
        auto csv = text(out / "sampled-records.csv");
        check(csv.find("# solver_dt=") != std::string::npos, "CSV lost solver dt provenance");
        const auto first = sampled.receivers().front().vz;
        check(sampled.reset(error) && sampled.receivers().front().vz.size() == 1, "Reset did not clear sampled records");
        check(sampled.advance(13, error) && sampled.receivers().front().vz == first, "Reset did not reset interpolation history");
        auto invalid = p; invalid.sampleIntervalMs = -.5;
        check(!sampled.initialize(invalid, error, backend, settings) && sampled.step() == 13,
              "Invalid sample interval changed active session");
    }
}
void snapshotSelection(const std::filesystem::path &out, BackendKind3D backend) {
    auto p = fixture();
    p.duration = .04; p.maxSteps = 80;
    ComputeSettings3D settings; settings.cpuThreads = 1;
    Simulation s;
    std::string error;
    check(s.initialize(p, error, backend, settings) && s.advance(80, error), error);
    const auto raw = s.snapshot();
    const auto array = [&](const std::string &xml, const char *name) {
        const auto start = xml.find(std::string("Name=\"") + name + "\" format=\"ascii\">");
        check(start != std::string::npos, "Missing selected snapshot array");
        const auto body = xml.find('>', start) + 1;
        const auto end = xml.find("</DataArray>", body);
        std::istringstream in(xml.substr(body, end-body));
        std::vector<double> values;
        double value;
        while (in >> value) values.push_back(value);
        check(values.size() == static_cast<std::size_t>(p.nx) * p.nz, "Wrong snapshot physical point count");
        return values;
    };
    const char *names[]{"Vz", "Vx", "VelocityMagnitude", "Pressure"};
    for (int component = 0; component < 4; ++component) {
        const auto path = out / ("snapshot-" + std::to_string(static_cast<int>(backend)) + "-" + names[component] + ".vts");
        const auto options = displayedSnapshotOptions(component);
        check(writeSnapshot(s, path, error, options), error);
        const auto xml = text(path);
        check(xml.find(std::string("<PointData Scalars=\"") + names[component] + "\">") != std::string::npos,
              "Snapshot active scalar does not match selected component");
        const auto begin = xml.find("<PointData "), end = xml.find("</PointData>");
        const auto second = xml.find("<DataArray", xml.find("<DataArray", begin) + 1);
        check(second > end, "Follow-display snapshot unexpectedly included other arrays");
        const auto values = array(xml, names[component]);
        std::vector<float> displayed;
        check(s.readFrame(component, displayed, error), error);
        double peak = 1e-25, mismatch = 0;
        for (std::size_t n = 0; n < values.size(); ++n) {
            const int i = static_cast<int>(n % p.nx), k = static_cast<int>(n / p.nx);
            const auto q = s.grid().index(i + ghost, k + ghost);
            if (component < 2)
                check(values[n] == raw.fields[component == 0 ? 1 : 0][q], "Snapshot lost Float64 raw velocity precision");
            if (component == 2)
                check(values[n] == std::sqrt(raw.fields[0][q] * raw.fields[0][q] + raw.fields[1][q] * raw.fields[1][q]),
                      "Snapshot speed differs from raw physical magnitude");
            if (component == 3) {
                const auto &g = s.grid();
                const double mu = g.density[q] * g.vs[q] * g.vs[q], lambda = g.density[q] * g.vp[q] * g.vp[q] - 2*mu;
                const double sum = raw.fields[2][q] + raw.fields[3][q];
                const double pressure = -(sum + lambda * sum / (2*(lambda+mu))) / 3;
                check(std::abs(values[n]-pressure) < 1e-13 * std::max(1e-25, std::abs(sum)), "Pressure omits plane-strain Syy");
            }
            peak = std::max(peak, std::abs(values[n]));
            mismatch = std::max(mismatch, std::abs(values[n]-displayed[n]));
        }
        check(mismatch < 2e-6 * peak, "Exported physical component differs from display definition");
    }
    SnapshotExportOptions manual;
    manual.components.fill(false); manual.components[1] = manual.components[6] = true;
    manual.includeMaterial = true;
    const auto path = out / ("snapshot-manual-" + std::to_string(static_cast<int>(backend)) + ".vts");
    check(writeSnapshot(s, path, error, manual), error);
    const auto xml = text(path);
    for (const char *name : {"Vz", "Sxz", "Vp", "Vs", "Density"}) array(xml, name);
    for (const char *name : {"Vx", "Sxx", "Szz", "VelocityMagnitude", "Pressure"})
        check(xml.find(std::string("Name=\"")+name+"\"") == std::string::npos, "Manual export included unselected component");
    const auto stress = array(xml, "Sxz");
    for (int k = 0; k < p.nz; ++k)
        for (int i = 0; i < p.nx; ++i)
            check(stress[i+static_cast<std::size_t>(p.nx)*k] == raw.fields[4][s.grid().index(i+ghost,k+ghost)],
                  "Selected stress component differs from solver raw state");
    manual.components.fill(false);
    check(!writeSnapshot(s, path, error, manual) && text(path) == xml, "Empty selection overwrote snapshot");
    check(s.step() == 80 && s.snapshot().fields == raw.fields, "Snapshot export changed solver state");
}
} // namespace
int main() {
    try {
        const auto out = std::filesystem::path(SEIS3D_OUTPUT_ROOT) / "validation/model-2d";
        std::filesystem::create_directories(out);
        roundTrip(out);
        {
            Simulation empty, session;
            std::string error;
            const auto path = out / "csv-nested" / "records.csv";
            check(!writeReceivers(empty, path, error), "Uninitialized CSV export was accepted");
            check(session.initialize(fixture(), error), error);
            check(!writeReceivers(session, {}, error), "Empty CSV path was accepted");
            check(writeReceivers(session, path, error), error);
            check(text(path).find("receiver,x_m,z_m,time_s,vx_m_s,vz_m_s") != std::string::npos,
                  "Nested CSV export lost its header");
        }
        sampling(out, BackendKind3D::Cpu);
        snapshotSelection(out, BackendKind3D::Cpu);
        const auto cuda = Seis3D::Curvilinear::queryBackend3D(BackendKind3D::Cuda);
        if (cuda.available) {
            sampling(out, BackendKind3D::Cuda);
            snapshotSelection(out, BackendKind3D::Cuda);
        }
        std::cout << "PASS: VTS parameter/terrain/layer/source round-trip, exact CPU reproduction, malformed-file rejection; "
                     "0.5/1/0.73/0.01 ms uniform sampling, interpolation, unchanged propagation/dt, reset, CSV and SEG-Y; "
                     "selected Float64 VTS components, display mapping, speed, plane-strain pressure, stresses/materials, empty-selection protection";
        std::cout << (cuda.available ? " on CPU and CUDA\n" : " on CPU (CUDA unavailable)\n");
        return 0;
    } catch (const std::exception &ex) { std::cerr << ex.what() << '\n'; return 1; }
}
