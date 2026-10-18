#include "SeisCommon/SegyIo.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value, const std::string &error) {
    if (!value)
        throw std::runtime_error(error);
}
} // namespace
int main(int argc, char **argv) {
    try {
        const auto folder = std::filesystem::path(SEIS3D_OUTPUT_ROOT) / "segy-validation";
        std::filesystem::create_directories(folder);
        std::string error;
        if (argc >= 6 && std::string(argv[1]) == "--legacy") {
            SeisWave::ImportedRecords records;
            check(SeisWave::readSegy(std::filesystem::path(argv[2]), records, error), error);
            SeisWave::SegyExportOptions options;
            check(std::string(argv[4]) == "ibm" || std::string(argv[4]) == "ieee", "Select ibm or ieee");
            options.profile = std::string(argv[4]) == "ibm" ? SeisWave::SegyExportProfile::LegacyIbm32
                                                         : SeisWave::SegyExportProfile::CompatibleIeee32;
            options.sampleGain = std::stod(argv[5]);
            options.sampleIntervalSeconds = argc > 6 ? std::stod(argv[6]) * .001 : 0;
            check(SeisWave::writeSegy(records.view(1), 0, std::filesystem::path(argv[3]), error, options), error);
            std::cout << "Converted legacy SEG-Y; gain=" << options.sampleGain << '\n';
            return 0;
        }
        if (argc > 1) {
            SeisWave::ImportedRecords records;
            check(SeisWave::readSegy(std::filesystem::path(argv[1]), records, error), error);
            std::cout << records.description << " traces=" << records.traces.size() << " dt=" << records.dt
                      << " first=" << records.traces.front().samples.front() << '\n';
            return 0;
        }
        const std::vector<double> a{.125, -.5, 1.25, 0}, b{-.25, 1, -2.5, 0};
        SeisWave::RecordDataView data;
        data.components = {"Vz", "Vx"};
        data.dt = .000423370123456789;
        for (int r = 0; r < 2; ++r) {
            SeisWave::TraceView t;
            t.position = {-1.234 + r * 2, 23.456, 10.234};
            t.source = {12.345, -6.789, 8.765};
            t.startTime = r * .005;
            t.channels[0] = a;
            t.channels[1] = b;
            data.traces.push_back(t);
        }
        for (bool ieee64 : {false, true})
            for (int component : {0, 1}) {
                const auto path = folder / (std::string(ieee64 ? "ieee64_" : "ieee32_") +
                                            data.components[component] + ".sgy");
                check(SeisWave::writeSegy(data, component, path, error, ieee64), error);
                SeisWave::ImportedRecords loaded;
                check(SeisWave::readSegy(path, loaded, error), error);
                check(std::abs(loaded.dt - data.dt) < 1e-18 && loaded.traces.size() == 2 &&
                          loaded.component == data.components[component] && loaded.units == "m/s",
                      "SEG-Y timing/component metadata changed");
                for (int r = 0; r < 2; ++r) {
                    check(loaded.traces[r].startTime == data.traces[r].startTime, "SEG-Y trace delay lost");
                    for (int k = 0; k < 3; ++k)
                        check(std::abs(loaded.traces[r].position[k] - data.traces[r].position[k]) < 1e-10 &&
                                  std::abs(loaded.traces[r].source[k] - data.traces[r].source[k]) < 1e-10,
                              "SEG-Y scaled geometry changed");
                    for (int t = 0; t < 4; ++t)
                        check(loaded.traces[r].samples[t] == data.value(r, component, t),
                              "SEG-Y raw component changed");
                }
                std::ofstream broken(folder / "broken.sgy", std::ios::binary);
                broken << "not segy";
                broken.close();
                check(!SeisWave::readSegy(folder / "broken.sgy", loaded, error) &&
                          loaded.traces.size() == 2 && loaded.dt == data.dt,
                      "Rejected import changed the active dataset");
            }
        auto legacyData = data;
        legacyData.dt = .0005;
        for (const auto profile : {SeisWave::SegyExportProfile::LegacyIbm32, SeisWave::SegyExportProfile::CompatibleIeee32})
            for (double gain : {1., 1e6}) {
                SeisWave::SegyExportOptions options;
                options.profile = profile;
                options.sampleGain = gain;
                const auto path = folder / ("legacy-" + std::to_string(static_cast<int>(profile)) + "-" + std::to_string(static_cast<int>(gain)) + ".sgy");
                check(SeisWave::writeSegy(legacyData, 0, path, error, options), error);
                SeisWave::ImportedRecords loaded;
                check(SeisWave::readSegy(path, loaded, error), error);
                check(loaded.dt == .0005 && loaded.component == "Vz" &&
                          loaded.units == (gain == 1 ? "m/s" : "scaled(m/s)"), "Legacy component/units/dt lost");
                for (int r = 0; r < 2; ++r)
                    for (int t = 0; t < 4; ++t)
                        check(loaded.traces[r].samples[t] == a[t] * gain, "Legacy IBM/IEEE amplitude round trip failed");
                options.sampleGain = -1;
                check(!SeisWave::writeSegy(legacyData, 0, path, error, options), "Negative export gain accepted");
                options.sampleGain = 1;
                check(!SeisWave::writeSegy(data, 0, path, error, options), "Fractional-microsecond legacy output accepted without resampling");
                options.sampleIntervalSeconds = .0005;
                check(SeisWave::writeSegy(data, 0, path, error, options), error);
                check(SeisWave::readSegy(path, loaded, error) && loaded.dt == .0005 && loaded.traces.front().samples.size() == 3,
                      "Legacy uniform output resampling failed");
                for (int t = 0; t < 3; ++t) {
                    const double u = t * .0005 / data.dt;
                    const auto i = static_cast<std::size_t>(u);
                    const double expected = std::lerp(a[i], a[std::min(i+1,a.size()-1)], u-i);
                    check(std::abs(loaded.traces.front().samples[t] - expected) < 2e-6, "Legacy resampling values differ");
                }
                // Preserve the exact known-sample fixtures for independent byte-level validation.
                options.sampleIntervalSeconds = 0;
                options.sampleGain = gain;
                check(SeisWave::writeSegy(legacyData, 0, path, error, options), error);
            }
        auto invalid = data;
        invalid.dt = 0;
        const auto existing = folder / "ieee32_Vz.sgy";
        const auto before = std::filesystem::file_size(existing);
        check(!SeisWave::writeSegy(invalid, 0, existing, error) &&
                  std::filesystem::file_size(existing) == before,
              "Invalid export damaged an existing file");
        std::cout << "PASS SEG-Y: two components, IEEE32/64, exact fractional-us dt, signed mm geometry, "
                     "trace delays, transactional failure; legacy rev1 IBM/IEEE, EBCDIC/ASCII, gain units, uniform resampling\n";
    } catch (const std::exception &ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
}
