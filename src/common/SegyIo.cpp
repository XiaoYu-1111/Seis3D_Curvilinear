#include "SeisCommon/SegyIo.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace SeisWave {
namespace {
using Bytes = std::vector<unsigned char>;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::uint64_t get(const Bytes &data, std::size_t offset, int count, bool little = false) {
    std::uint64_t value = 0;
    for (int i = 0; i < count; ++i)
        value = (value << 8) | data[offset + (little ? count - i - 1 : i)];
    return value;
}
std::int16_t i16(const Bytes &b, std::size_t o, bool le) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(get(b, o, 2, le)));
}
std::int32_t i32(const Bytes &b, std::size_t o, bool le) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(get(b, o, 4, le)));
}
void put(Bytes &b, std::size_t o, std::uint64_t value, int count) {
    for (int i = count - 1; i >= 0; --i) {
        b[o + i] = static_cast<unsigned char>(value);
        value >>= 8;
    }
}
double doubleAt(const Bytes &b, std::size_t o, bool le) {
    return std::bit_cast<double>(get(b, o, 8, le));
}
void read(std::ifstream &in, Bytes &bytes, std::size_t n) {
    bytes.resize(n);
    require(static_cast<bool>(in.read(reinterpret_cast<char *>(bytes.data()), n)),
            "Truncated SEG-Y header or sample data.");
}
void write(std::ofstream &out, const Bytes &b) {
    out.write(reinterpret_cast<const char *>(b.data()), b.size());
    require(static_cast<bool>(out), "SEG-Y write failed.");
}
int sampleBytes(int format) {
    switch (format) {
    case 1:
    case 2:
    case 5:
        return 4;
    case 3:
        return 2;
    case 6:
        return 8;
    case 8:
        return 1;
    default:
        return 0;
    }
}
double scale(std::int16_t value) {
    return value > 0 ? value : value < 0 ? -1.0 / value : 1;
}
std::int32_t millimetres(double v) {
    require(std::isfinite(v) && std::abs(v * 1000) < std::numeric_limits<std::int32_t>::max(),
            "SEG-Y coordinates exceed the supported millimetre range.");
    return static_cast<std::int32_t>(std::llround(v * 1000));
}
} // namespace
bool writeSegy(const RecordDataView &records, int component, const std::filesystem::path &path,
               std::string &error, bool ieee64) {
    std::filesystem::path temporary;
    error.clear();
    try {
        require(!records.traces.empty() && component >= 0 &&
                    component < static_cast<int>(records.components.size()),
                "Select an existing raw component before SEG-Y export.");
        require(std::isfinite(records.dt) && records.dt > 0 && records.dt * 1e6 <= 65535,
                "SEG-Y sample interval must be positive and at most 65535 microseconds.");
        const auto count = records.sampleCount(0);
        require(count > 0 && count <= 65535, "SEG-Y export requires 1..65535 samples per trace.");
        require(records.traces.size() <= 100000, "Too many SEG-Y traces.");
        for (const auto &trace : records.traces) {
            require(trace.channels[component].size() == count, "SEG-Y export requires equal trace lengths.");
            require(std::isfinite(trace.startTime) && std::abs(trace.startTime * 1000) <= 32767 &&
                        std::abs(trace.startTime * 1000 - std::round(trace.startTime * 1000)) < 1e-7,
                    "SEG-Y export delay must be representable as whole milliseconds.");
            for (double v : trace.position)
                millimetres(v);
            for (double v : trace.source)
                millimetres(v);
            for (double v : trace.channels[component])
                require(std::isfinite(v) && (ieee64 || std::abs(v) <= std::numeric_limits<float>::max()),
                        "SEG-Y samples must be finite and representable in the selected IEEE precision.");
        }
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());
        temporary = path;
        temporary += ".partial";
        for (int suffix = 1; std::filesystem::exists(temporary); ++suffix) {
            temporary = path;
            temporary += ".partial" + std::to_string(suffix);
        }
        std::ofstream out(temporary, std::ios::binary);
        require(static_cast<bool>(out), "Cannot create SEG-Y output.");
        Bytes text(3200, ' ');
        const auto line = [&](int row, const std::string &content) {
            const auto prefix =
                "C" + (row < 10 ? std::string(" ") : std::string()) + std::to_string(row) + " ";
            const auto s = prefix + content;
            std::copy_n(s.begin(), std::min<std::size_t>(80, s.size()), text.begin() + (row - 1) * 80);
        };
        line(1, "SEISWAVE LAB - RAW ELASTIC WAVE RECEIVER RECORDS");
        line(2, "COMPONENT=" + records.components[component] + " UNITS=" + records.units);
        std::ostringstream dt;
        dt << std::setprecision(17) << records.dt;
        line(3, "EXACT DT (SECONDS)=" + dt.str());
        line(4, "SEG-Y REV 2.0, BIG ENDIAN, IEEE " + std::string(ieee64 ? "64" : "32") + " BIT SAMPLES");
        line(5, "EXACT DT: BINARY HEADER BYTES 3273-3280 (IEEE64 MICROSECONDS)");
        line(6, "COORDINATES/ELEVATIONS IN METRES; SCALARS=-1000; Z POSITIVE DOWN");
        line(7, "NO DISPLAY GAIN, AGC OR NORMALIZATION APPLIED; TIME ZERO INCLUDED");
        line(8, "X IS INLINE, Y CROSSLINE; ONE VELOCITY COMPONENT PER FILE");
        line(40, "END TEXTUAL HEADER");
        write(out, text);
        Bytes binary(400);
        const auto interval =
            static_cast<std::uint16_t>(std::clamp(std::llround(records.dt * 1e6), 1LL, 65535LL));
        put(binary, 12, std::min<std::size_t>(65535, records.traces.size()), 2);
        put(binary, 16, interval, 2);
        put(binary, 18, interval, 2);
        put(binary, 20, count, 2);
        put(binary, 22, count, 2);
        put(binary, 24, ieee64 ? 6 : 5, 2);
        put(binary, 54, 1, 2);
        put(binary, 60, records.traces.size(), 4);
        put(binary, 68, count, 4);
        put(binary, 72, std::bit_cast<std::uint64_t>(records.dt * 1e6), 8);
        put(binary, 80, std::bit_cast<std::uint64_t>(records.dt * 1e6), 8);
        put(binary, 88, count, 4);
        put(binary, 96, 0x01020304, 4);
        binary[300] = 2;
        put(binary, 302, 1, 2);
        put(binary, 312, records.traces.size(), 8);
        put(binary, 320, 3600, 8);
        write(out, binary);
        for (std::size_t r = 0; r < records.traces.size(); ++r) {
            const auto &trace = records.traces[r];
            Bytes header(240);
            put(header, 0, r + 1, 4);
            put(header, 4, r + 1, 4);
            put(header, 8, 1, 4);
            put(header, 12, r + 1, 4);
            const auto name = records.components[component];
            put(header, 28, name == "Vz" ? 12 : name == "Vx" ? 14 : 1, 2);
            put(header, 40, static_cast<std::uint32_t>(millimetres(-trace.position[2])), 4);
            put(header, 48, static_cast<std::uint32_t>(millimetres(trace.source[2])), 4);
            put(header, 68, static_cast<std::uint16_t>(-1000), 2);
            put(header, 70, static_cast<std::uint16_t>(-1000), 2);
            put(header, 72, static_cast<std::uint32_t>(millimetres(trace.source[0])), 4);
            put(header, 76, static_cast<std::uint32_t>(millimetres(trace.source[1])), 4);
            put(header, 80, static_cast<std::uint32_t>(millimetres(trace.position[0])), 4);
            put(header, 84, static_cast<std::uint32_t>(millimetres(trace.position[1])), 4);
            put(header, 88, 1, 2);
            put(header, 108, static_cast<std::uint16_t>(std::llround(trace.startTime * 1000)), 2);
            put(header, 114, count, 2);
            put(header, 116, interval, 2);
            put(header, 202, records.units == "m/s" ? 6 : records.units == "Pa" ? 1 : 0, 2);
            write(out, header);
            Bytes samples(count * (ieee64 ? 8 : 4));
            for (std::size_t t = 0; t < count; ++t) {
                const auto v = trace.channels[component][t];
                if (ieee64)
                    put(samples, t * 8, std::bit_cast<std::uint64_t>(v), 8);
                else
                    put(samples, t * 4, std::bit_cast<std::uint32_t>(static_cast<float>(v)), 4);
            }
            write(out, samples);
        }
        out.close();
        require(static_cast<bool>(out), "SEG-Y final flush failed.");
#ifdef _WIN32
        require(MoveFileExW(temporary.c_str(), path.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
                "Cannot replace the SEG-Y destination file.");
#else
        std::filesystem::rename(temporary, path);
#endif
        return true;
    } catch (const std::exception &ex) {
        if (!temporary.empty()) {
            std::error_code ec;
            std::filesystem::remove(temporary, ec);
        }
        error = ex.what();
        return false;
    }
}
bool readSegy(const std::filesystem::path &path, ImportedRecords &destination, std::string &error) {
    error.clear();
    try {
        std::ifstream in(path, std::ios::binary);
        require(static_cast<bool>(in), "Cannot open SEG-Y input.");
        const auto size = std::filesystem::file_size(path);
        require(size >= 3600, "SEG-Y input is shorter than its file headers.");
        Bytes text, binary;
        read(in, text, 3200);
        read(in, binary, 400);
        const auto marker = get(binary, 96, 4);
        require(marker == 0 || marker == 0x01020304 || marker == 0x04030201,
                "Unsupported SEG-Y byte ordering (pair-swapped files are not supported).");
        bool little = marker == 0x04030201;
        if (!marker && !sampleBytes(static_cast<int>(get(binary, 24, 2))) &&
            sampleBytes(static_cast<int>(get(binary, 24, 2, true))))
            little = true;
        const int format = static_cast<int>(get(binary, 24, 2, little)), width = sampleBytes(format),
                  revision = binary[300];
        require(width > 0, "Unsupported SEG-Y sample format. Supported codes: 1,2,3,5,6,8.");
        require(revision <= 2, "Unsupported SEG-Y revision.");
        require(
            revision < 2 || get(binary, 306, 4, little) == 0,
            "SEG-Y additional trace headers are not supported; convert to standard 240-byte trace headers.");
        std::uint64_t offset = 3600;
        const int ext = revision ? i16(binary, 304, little) : 0;
        if (revision >= 2 && get(binary, 320, 8, little))
            offset = get(binary, 320, 8, little);
        else {
            require(ext >= 0 && ext <= 1024,
                    "SEG-Y needs an explicit, bounded extended textual header count.");
            offset += static_cast<std::uint64_t>(ext) * 3200;
        }
        require(offset >= 3600 && offset <= size, "SEG-Y first trace offset is outside the file.");
        in.seekg(offset);
        const auto trailer = revision >= 2 ? i32(binary, 328, little) : 0;
        require(trailer >= 0, "SEG-Y variable trailer counts are not supported.");
        require(static_cast<std::uint64_t>(trailer) * 3200 <= size - offset,
                "Invalid SEG-Y data trailer size.");
        const auto end = size - static_cast<std::uint64_t>(trailer) * 3200;
        std::uint64_t ns = get(binary, 20, 2, little);
        if (revision >= 2 && get(binary, 68, 4, little))
            ns = get(binary, 68, 4, little);
        double globalInterval = static_cast<double>(get(binary, 16, 2, little));
        if (revision >= 2 && doubleAt(binary, 72, little) != 0)
            globalInterval = doubleAt(binary, 72, little);
        const bool fixed = revision && get(binary, 302, 2, little) == 1;
        const double lengthUnit = get(binary, 54, 2, little) == 2 ? .3048 : 1;
        ImportedRecords result;
        std::size_t decoded = 0;
        int commonId = -1, commonUnit = -1;
        const std::string textual(text.begin(), text.end());
        if (textual.find("UNITS=m/s") != std::string::npos)
            result.units = "m/s";
        while (offset < end) {
            require(result.traces.size() < 4096, "SEG-Y import exceeds 4096 trace positions.");
            require(end - offset >= 240, "Incomplete SEG-Y trace header.");
            Bytes header, samples;
            read(in, header, 240);
            offset += 240;
            const auto count = fixed ? ns : (get(header, 114, 2, little) ? get(header, 114, 2, little) : ns);
            require(count > 0 && count <= 1000000 && count <= ((256ULL << 20) - decoded) / sizeof(double),
                    "SEG-Y sample count exceeds the import memory budget (256 MiB).");
            double interval = fixed && globalInterval > 0 ? globalInterval
                                                          : static_cast<double>(get(header, 116, 2, little));
            if (interval == 0)
                interval = globalInterval;
            require(std::isfinite(interval) && interval >= 1e-6 && interval <= 1e9,
                    "SEG-Y sample interval is missing, invalid or outside 1 ps..1000 s.");
            if (result.traces.empty())
                result.dt = interval * 1e-6;
            else
                require(std::abs(result.dt - interval * 1e-6) <= std::max(1e-12, result.dt * 1e-9),
                        "SEG-Y traces have different sample intervals; resample before import.");
            require(count * width <= end - offset, "Truncated SEG-Y trace samples.");
            read(in, samples, static_cast<std::size_t>(count) * width);
            offset += count * width;
            decoded += count * sizeof(double);
            ImportedTrace trace;
            const double coord = scale(i16(header, 70, little)), elev = scale(i16(header, 68, little));
            require(get(header, 88, 2, little) <= 1,
                    "SEG-Y angular coordinates are not supported in this Cartesian record viewer.");
            trace.position = {i32(header, 80, little) * coord * lengthUnit,
                              i32(header, 84, little) * coord * lengthUnit,
                              -static_cast<double>(i32(header, 40, little)) * elev * lengthUnit};
            trace.source = {
                i32(header, 72, little) * coord * lengthUnit, i32(header, 76, little) * coord * lengthUnit,
                (i32(header, 48, little) - static_cast<double>(i32(header, 44, little))) * elev * lengthUnit};
            trace.startTime =
                i16(header, 108, little) * .001 * (revision ? scale(i16(header, 214, little)) : 1);
            trace.samples.resize(count);
            const int weighting = i16(header, 168, little);
            require(weighting >= -1022 && weighting <= 1022,
                    "SEG-Y trace weighting is outside the supported finite range.");
            for (std::size_t t = 0; t < count; ++t) {
                const auto o = t * width;
                double v = 0;
                if (format == 5)
                    v = std::bit_cast<float>(static_cast<std::uint32_t>(get(samples, o, 4, little)));
                else if (format == 6)
                    v = doubleAt(samples, o, little);
                else if (format == 2)
                    v = i32(samples, o, little);
                else if (format == 3)
                    v = i16(samples, o, little);
                else if (format == 8)
                    v = std::bit_cast<std::int8_t>(samples[o]);
                else {
                    const auto bits = static_cast<std::uint32_t>(get(samples, o, 4, little));
                    v = (bits & 0xffffff) ? std::ldexp(static_cast<double>(bits & 0xffffff),
                                                       4 * (static_cast<int>((bits >> 24) & 127) - 64) - 24)
                                          : 0;
                    if (bits & 0x80000000)
                        v = -v;
                }
                v = std::ldexp(v, -weighting);
                require(std::isfinite(v), "SEG-Y contains nonfinite samples.");
                trace.samples[t] = v;
            }
            const int id = static_cast<int>(get(header, 28, 2, little));
            if (commonId < 0)
                commonId = id;
            else if (commonId != id)
                commonId = 0;
            const int unit = revision ? i16(header, 202, little) : 0;
            if (commonUnit < 0)
                commonUnit = unit;
            else if (commonUnit != unit)
                commonUnit = 1000;
            result.traces.push_back(std::move(trace));
        }
        require(!result.traces.empty(), "SEG-Y contains no data traces.");
        result.component = commonId == 12 ? "Vz" : commonId == 14 ? "Vx" : "Amplitude";
        const char *units[]{"raw", "Pa", "V", "mV", "A", "m", "m/s", "m/s2", "N", "W"};
        if (commonUnit > 0 && commonUnit < 10)
            result.units = units[commonUnit];
        if (commonUnit == 1000)
            result.units = "mixed";
        result.description = "SEG-Y rev " + std::to_string(revision) + " | format " + std::to_string(format) +
                             (little ? " | little endian" : " | big endian");
        destination = std::move(result);
        return true;
    } catch (const std::exception &ex) {
        error = ex.what();
        return false;
    }
}
} // namespace SeisWave
