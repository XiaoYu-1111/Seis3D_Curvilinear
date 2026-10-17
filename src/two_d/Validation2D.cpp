#include "PointKernels2D.hpp"
#include "PointKernels3D.hpp"
#include "Seis2D/CurvilinearSolver2D.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Seis2D::Curvilinear;
namespace {
void check(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
void compare(const std::vector<double> &a, const std::vector<double> &b) {
    check(a.size() == b.size(), "2D CPU/CUDA size mismatch");
    double error = 0, peak = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        check(std::isfinite(a[i]) && std::isfinite(b[i]), "Nonfinite 2D field");
        peak = std::max(peak, std::abs(a[i]));
        error = std::max(error, std::abs(a[i] - b[i]));
    }
    check(error < 1e-9 * std::max(peak, 1e-25), "2D CPU/CUDA numerical mismatch");
}
void reduction() {
    Parameters p;
    p.nx = 20;
    p.nz = 24;
    p.sourceZ = 5;
    p.relief = 3;
    p.spacing = 5;
    p.receiverCount = 8;
    p.receiverStride = 2;
    Simulation s;
    std::string error;
    check(s.initialize(p, error), error.c_str());
    const auto &g = s.grid();
    const std::size_t count = g.coordinates.size();
    std::vector<double> lambda(count), mu(count), zero(count), normal2(2 * count);
    for (std::size_t q = 0; q < count; ++q) {
        mu[q] = g.density[q] * g.vs[q] * g.vs[q];
        lambda[q] = g.density[q] * g.vp[q] * g.vp[q] - 2 * mu[q];
        normal2[2 * q] = g.normals[q][0];
        normal2[2 * q + 1] = g.normals[q][1];
    }
    Point::Mesh mesh{g.nx,
                     g.nz,
                     count,
                     g.h,
                     g.metricA.data(),
                     g.metricB.data(),
                     g.jacobian.data(),
                     normal2.data(),
                     lambda.data(),
                     mu.data(),
                     g.density.data(),
                     zero.data(),
                     zero.data()};
    std::array<std::vector<double>, 5> input2, out2;
    Point::Fields w2, r2;
    for (int f = 0; f < 5; ++f) {
        input2[f].resize(count);
        out2[f].resize(count);
        w2.f[f] = input2[f].data();
        r2.f[f] = out2[f].data();
        for (std::size_t q = 0; q < count; ++q)
            input2[f][q] = std::sin(.13 * (q % g.nx) + .07 * (q / g.nx) + f) * (.2 + f);
    }
    for (int i = 3; i < g.nx - 3; ++i)
        Point::project(w2, mesh, i);
    namespace P3 = Seis3D::Curvilinear::Point;
    P3::Mesh m3;
    m3.nx = g.nx;
    m3.ny = 9;
    m3.nz = g.nz;
    m3.count = count * 9;
    m3.h = g.h;
    m3.lambda = lambda[0];
    m3.mu = mu[0];
    m3.density = p.density;
    std::vector<double> metrics(9 * m3.count), normals(3 * m3.count), jac(m3.count), zeros(m3.count);
    std::array<std::vector<double>, 9> input3, out3;
    P3::Fields w3, r3;
    for (int f = 0; f < 9; ++f) {
        input3[f].assign(m3.count, 0);
        out3[f].assign(m3.count, 0);
        w3.f[f] = input3[f].data();
        r3.f[f] = out3[f].data();
    }
    const int mapping[]{0, 2, 3, 5, 7};
    for (int k = 0; k < g.nz; ++k)
        for (int j = 0; j < 9; ++j)
            for (int i = 0; i < g.nx; ++i) {
                const auto q = g.index(i, k), q3 = P3::index(m3, i, j, k);
                metrics[9 * q3] = metrics[9 * q3 + 4] = 1;
                metrics[9 * q3 + 6] = g.metricA[q];
                metrics[9 * q3 + 8] = g.metricB[q];
                normals[3 * q3] = g.normals[q][0];
                normals[3 * q3 + 2] = g.normals[q][1];
                jac[q3] = g.jacobian[q];
                for (int f = 0; f < 5; ++f)
                    input3[mapping[f]][q3] = input2[f][q];
            }
    m3.metrics = metrics.data();
    m3.normals = normals.data();
    m3.jacobian = jac.data();
    m3.source = m3.damping = zeros.data();
    double worst = 0;
    for (int step = 0; step < 8; ++step)
        for (int stage = 0; stage < 4; ++stage)
            for (int depth : {0, 1, 2, 3, 7})
                for (int i = 3; i < g.nx - 3; ++i) {
                    const auto q = g.index(i, depth + 3), q3 = P3::index(m3, i, 4, depth + 3);
                    Point::rhs(w2, r2, mesh, q, 0, step, stage);
                    P3::rhs(w3, r3, m3, q3, 0, step, stage);
                    for (int f = 0; f < 5; ++f)
                        worst = std::max(worst, std::abs(out2[f][q] - out3[mapping[f]][q3]) /
                                                    std::max(1.0, std::abs(out3[mapping[f]][q3])));
                }
    check(worst < 1e-11, "P-SV operators do not match the y-invariant 3D five-field restriction");
    std::cout << "PASS 2D/3D P-SV RHS reduction: 8 directions x 4 stages, surface and interior, error="
              << worst << '\n';
    // Independent plane-wave stress RHS: for a P wave vx=sin(k*x), the exact
    // sxx rate is rho*Vp^2*k*cos(k*x). For SV vz=sin(k*x), sxz rate is mu*k*cos(k*x).
    for (auto &f : input2)
        std::fill(f.begin(), f.end(), 0);
    const double wave = 2 * 3.14159265358979323846 / (128 * g.h);
    double pError = 0, sError = 0;
    for (int k = 0; k < g.nz; ++k)
        for (int i = 0; i < g.nx; ++i) {
            input2[0][g.index(i, k)] = std::sin(wave * (i - 3) * g.h);
            input2[1][g.index(i, k)] = input2[0][g.index(i, k)];
        }
    // Use a Cartesian local metric here to isolate the physical P/S derivative.
    std::vector<double> a(count, 0), b(count, 1);
    auto flat = mesh;
    flat.a = a.data();
    flat.b = b.data();
    for (int i = 5; i < g.nx - 5; ++i) {
        const auto q = g.index(i, 12);
        Point::rhs(w2, r2, flat, q, 0, 0, 0);
        const double expected = wave * std::cos(wave * (i - 3) * g.h);
        pError = std::max(pError, std::abs(out2[2][q] / (lambda[q] + 2 * mu[q]) - expected) / wave);
        sError = std::max(sError, std::abs(out2[4][q] / mu[q] - expected) / wave);
    }
    check(pError < .005 && sError < .005, "2D P/S analytic interior derivative failed");
}
} // namespace
int main(int argc, char **argv) {
    try {
        reduction();
        std::string error;
        const auto cuda = Seis3D::Curvilinear::queryBackend3D(BackendKind3D::Cuda);
        {
            const auto folder = std::filesystem::path(SEIS3D_OUTPUT_ROOT) / "validation/terrain-2d";
            std::filesystem::create_directories(folder);
            const auto file = folder / "profile.csv";
            {
                std::ofstream out(file);
                out << "x_m,elevation_m\n0,100\n50,103\n100,100\n";
            }
            SurfaceSamples samples;
            check(readSurfaceProfile(file, samples, error), error.c_str());
            const auto original = samples;
            {
                std::ofstream out(file);
                out << "0,100\n0,101\n";
            }
            check(!readSurfaceProfile(file, samples, error) && samples == original,
                  "Invalid terrain replaced profile");
            {
                std::ofstream out(file);
                out << "1,oops\n2,100\n3,101\n";
            }
            check(!readSurfaceProfile(file, samples, error), "Malformed first sample accepted as a header");
            {
                std::ofstream out(file);
                out << "x_m,elevation_m\n0,100\n50,103\n100,100\n";
            }
            SurfaceProfile profile(samples);
            for (int i = 0; i <= 1000; ++i) {
                const auto v = profile.sample(i * .1);
                check(v[0] >= 100 && v[0] <= 103, "PCHIP elevation overshot its extrema");
            }
            check(std::abs(profile.sample(50 - 1e-6)[1] - profile.sample(50 + 1e-6)[1]) < 1e-7,
                  "Terrain derivative is discontinuous");
            Parameters p;
            p.nx = 40;
            p.nz = 48;
            p.sourceZ = 12;
            p.receiverCount = 18;
            p.maxSteps = 80;
            p.duration = .1;
            p.frequency = 30;
            p.relief = 0;
            p.importedSurface = true;
            p.surfaceSamples = samples;
            p.surfaceDatum = 101.5;
            Simulation cpu, gpu;
            check(cpu.initialize(p, error), error.c_str());
            const double importedDt = cpu.dt();
            const auto &g = cpu.grid();
            for (int i = 0; i < p.nx; ++i) {
                const auto value = profile.sample(i * p.spacing);
                const auto q = g.index(i + ghost, ghost);
                check(std::abs(g.coordinates[q][1] - (p.surfaceDatum - value[0])) < 1e-12,
                      "Imported surface coordinates incorrect");
                check(std::abs(g.normals[q][0] - value[1] / std::hypot(value[1], 1.)) < 1e-12,
                      "Imported free-surface normal incorrect");
            }
            check(cpu.advance(80, error) && cpu.diagnostics().finite, error.c_str());
            if (cuda.available) {
                check(gpu.initialize(p, error, BackendKind3D::Cuda, {1, cuda.cudaDevice}) &&
                          gpu.advance(80, error),
                      error.c_str());
                const auto a = cpu.snapshot(), b = gpu.snapshot();
                for (int f = 0; f < 5; ++f)
                    compare(a.fields[f], b.fields[f]);
            }
            auto bad = p;
            bad.surfaceSamples.back()[0] = 90;
            check(!cpu.initialize(bad, error) && cpu.dt() == importedDt && cpu.step() == 80,
                  "Terrain coverage failure discarded session");
            p.importedSurface = false;
            check(cpu.initialize(p, error) && cpu.dt() != importedDt,
                  "Terrain / frequency rebuild did not update dt");
            std::cout << "PASS imported terrain: CSV validation, PCHIP extrema/C1, coordinates, normals, dt, "
                         "CPU/CUDA="
                      << cuda.available << '\n';
        }
        // Both axes exceed the old 1000-node ceiling. Cap resource usage to a
        // short propagation; the 16384-square capacity is not an allocation promise.
        for (auto kind : {BackendKind3D::Cpu, BackendKind3D::Cuda}) {
            if (kind == BackendKind3D::Cuda && !cuda.available)
                continue;
            Parameters p;
            p.nx = 1200;
            p.nz = 1100;
            p.relief = 0;
            p.maxSteps = 2;
            p.duration = .01;
            p.spacing = 10;
            p.frequency = 12;
            Simulation large;
            check(large.initialize(p, error, kind, {4, cuda.cudaDevice}) && large.advance(2, error),
                  error.c_str());
            check(large.diagnostics().finite && large.step() == 2, "Large 2D propagation failed");
            p.nx = maxAxisNodes + 1;
            check(!large.initialize(p, error, kind) && large.step() == 2,
                  "Large-grid validation discarded active session");
            std::cout << "PASS 1200x1100 2D grid: backend=" << static_cast<int>(kind) << '\n';
        }
        for (bool layered : {false, true})
            for (double relief : {0.0, 4.0}) {
                Parameters p;
                p.nx = 40;
                p.nz = 48;
                p.spacing = 5;
                p.sourceZ = 12;
                p.frequency = 20;
                p.duration = .18;
                p.maxSteps = 0;
                p.relief = relief;
                p.wavelength = 200;
                p.receiverCount = 36;
                p.receiverStride = 1;
                p.layered = layered;
                Simulation cpu, gpu;
                check(cpu.initialize(p, error, BackendKind3D::Cpu, {1}), error.c_str());
                const auto dt = cpu.dt();
                if (cuda.available)
                    check(gpu.initialize(p, error, BackendKind3D::Cuda, {1, cuda.cudaDevice}), error.c_str());
                check(cpu.advance(cpu.totalSteps(), error), error.c_str());
                const auto a = cpu.snapshot();
                check(cpu.diagnostics().finite && cpu.diagnostics().energy > 0 &&
                          cpu.diagnostics().tractionRelative < 1e-8,
                      "2D propagation/traction failed");
                check(cpu.receivers().back().vz.size() == static_cast<std::size_t>(cpu.step() + 1),
                      "2D receiver timeline failed");
                if (cuda.available) {
                    check(gpu.dt() == dt && gpu.advance(gpu.totalSteps(), error), error.c_str());
                    const auto b = gpu.snapshot();
                    for (int f = 0; f < 5; ++f)
                        compare(a.fields[f], b.fields[f]);
                    for (std::size_t r = 0; r < cpu.receivers().size(); ++r) {
                        compare(cpu.receivers()[r].vx, gpu.receivers()[r].vx);
                        compare(cpu.receivers()[r].vz, gpu.receivers()[r].vz);
                    }
                    std::vector<float> fa, fb;
                    check(cpu.readFrame(0, fa, error) && gpu.readFrame(0, fb, error) &&
                              fa.size() == static_cast<std::size_t>(p.nx) * p.nz,
                          error.c_str());
                    for (std::size_t q = 0; q < fa.size(); ++q)
                        check(std::abs(fa[q] - fb[q]) <= 1e-6 * std::max(1e-20f, std::abs(fa[q])),
                              "2D compact CUDA frame differs");
                    check(gpu.reset(error) && gpu.step() == 0 &&
                              gpu.snapshot().fields[0] ==
                                  std::vector<double>(cpu.grid().coordinates.size(), 0),
                          "2D CUDA clear failed");
                }
                auto bad = p;
                bad.receiverCount = 1025;
                const int oldStep = cpu.step();
                check(!cpu.initialize(bad, error) && cpu.step() == oldStep && cpu.dt() == dt,
                      "Rejected 2D acquisition discarded a session");
                p.spacing /= 2;
                p.frequency *= 2;
                p.maxSteps = 4;
                check(cpu.initialize(p, error) && cpu.dt() < dt && cpu.advance(100, error) && cpu.step() == 4,
                      "2D dt did not update on rebuild / cap failed");
                check(cpu.reset(error) && cpu.step() == 0 && cpu.dt() > 0 &&
                          cpu.receivers().front().vx == std::vector<double>{0},
                      "2D in-place clear failed");
                std::cout << "PASS 2D propagation: relief=" << relief << " layered=" << layered
                          << " CPU/CUDA=" << cuda.available << '\n';
            }
        for (auto backend : {BackendKind3D::Cpu, BackendKind3D::Cuda}) {
            if (backend == BackendKind3D::Cuda && !cuda.available)
                continue;
            Parameters p;
            p.nx = 1000;
            p.nz = 16;
            p.sourceZ = 4;
            p.relief = 0;
            p.receiverStart = 0;
            p.receiverStride = 1;
            p.receiverCount = 1000;
            p.maxSteps = 2;
            Simulation s;
            check(s.initialize(p, error, backend, {1, cuda.cudaDevice}) && s.advance(10, error),
                  error.c_str());
            check(s.receivers().size() == 1000 && s.receivers().back().vz.size() == 3,
                  "2D last receiver block was truncated");
        }
        // Cross the OpenMP activation threshold and compare a true parallel run to
        // the serial reference; tiny propagation cases above intentionally do not.
#ifdef SEIS3D_HAS_OPENMP
        {
            Parameters p;
            p.nx = 160;
            p.nz = 128;
            p.relief = 5;
            p.frequency = 30;
            p.maxSteps = 4;
            p.receiverCount = 78;
            Simulation serial, parallel;
            check(serial.initialize(p, error, BackendKind3D::Cpu, {1}) &&
                      parallel.initialize(p, error, BackendKind3D::Cpu, {4}),
                  error.c_str());
            check(serial.advance(4, error) && parallel.advance(4, error), error.c_str());
            const auto a = serial.snapshot(), b = parallel.snapshot();
            for (int f = 0; f < 5; ++f)
                compare(a.fields[f], b.fields[f]);
            check(parallel.backend().workerThreads == 4, "2D OpenMP worker selection failed");
            std::cout << "PASS 2D serial/OpenMP: 160x128, 1 versus 4 workers\n";
        }
#endif
        if (argc > 1 && std::string(argv[1]) == "--extended") {
            for (double frequency : {30., 50.}) {
                Parameters p;
                p.frequency = frequency;
                p.spacing = frequency == 30 ? 2.5 : 1.25;
                p.relief = frequency == 30 ? 10 : 5;
                p.duration = frequency == 30 ? 1.0 : .25;
                p.maxSteps = 0;
                Simulation cpu, gpu;
                check(cpu.initialize(p, error, BackendKind3D::Cpu, {1}), error.c_str());
                check(cpu.advance(cpu.totalSteps(), error), error.c_str());
                check(cpu.diagnostics().finite && cpu.diagnostics().energy > 0 &&
                          cpu.diagnostics().tractionRelative < 1e-8,
                      "2D extended propagation failed");
                if (cuda.available) {
                    check(gpu.initialize(p, error, BackendKind3D::Cuda, {1, cuda.cudaDevice}) &&
                              gpu.advance(gpu.totalSteps(), error),
                          error.c_str());
                    const auto a = cpu.snapshot(), b = gpu.snapshot();
                    for (int f = 0; f < 5; ++f)
                        compare(a.fields[f], b.fields[f]);
                }
                std::cout << "PASS 2D extended: " << frequency << " Hz, h=" << p.spacing
                          << ", t=" << p.duration << ", steps=" << cpu.step()
                          << ", PPW=" << cpu.diagnostics().ppw << ", CPU/CUDA=" << cuda.available << '\n';
            }
        }
        std::cout << "PASS: genuine five-field 2D storage, source-window propagation, dt rebuild, "
                     "transactional failure, clear, final receiver sampling\n";
    } catch (const std::exception &ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
}
