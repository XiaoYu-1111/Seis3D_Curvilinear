#include "SeisCommon/SegyIo.hpp"
#include "SeisCommon/AtomicFile.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
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
// ASCII <-> IBM CP037 for the traditional 3200-byte EBCDIC textual header.
constexpr unsigned char ebcdic[]{
    0x00,0x01,0x02,0x03,0x37,0x2d,0x2e,0x2f,0x16,0x05,0x25,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x3c,0x3d,0x32,0x26,0x18,0x19,0x3f,0x27,0x1c,0x1d,0x1e,0x1f,
    0x40,0x5a,0x7f,0x7b,0x5b,0x6c,0x50,0x7d,0x4d,0x5d,0x5c,0x4e,0x6b,0x60,0x4b,0x61,
    0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0x7a,0x5e,0x4c,0x7e,0x6e,0x6f,
    0x7c,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,
    0xd7,0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xba,0xe0,0xbb,0xb0,0x6d,
    0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,
    0x97,0x98,0x99,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xc0,0x4f,0xd0,0xa1,0x07};
std::uint32_t ibmFloat(double v) {
    if (v == 0) return 0;
    const auto sign = std::signbit(v) ? 0x80000000U : 0;
    int power{};
    std::frexp(std::abs(v), &power);
    int exponent = static_cast<int>(std::ceil(power / 4.0));
    if (exponent < -64) return 0; // Below the smallest normalized IBM number.
    require(exponent <= 63, "SEG-Y IBM32 sample overflow; reduce the export multiplier.");
    auto fraction = static_cast<std::uint32_t>(std::llround(std::ldexp(std::abs(v), 24 - 4 * exponent)));
    if (fraction == 0x1000000U) { fraction = 0x100000U; ++exponent; }
    require(exponent <= 63, "SEG-Y IBM32 rounding overflow.");
    return sign | (static_cast<std::uint32_t>(exponent + 64) << 24) | fraction;
}
} // namespace
bool writeSegy(const RecordDataView &records, int component, const std::filesystem::path &path,
               std::string &error, bool ieee64) {
    SegyExportOptions options;
    options.profile = ieee64 ? SegyExportProfile::ExactIeee64 : SegyExportProfile::ExactIeee32;
    return writeSegy(records, component, path, error, options);
}
bool writeSegy(const RecordDataView &records, int component, const std::filesystem::path &path,
               std::string &error, const SegyExportOptions &options) {
    std::filesystem::path temporary;
    error.clear();
    try {
        const bool compatible = options.profile == SegyExportProfile::LegacyIbm32 ||
                                options.profile == SegyExportProfile::CompatibleIeee32;
        const bool ieee64 = options.profile == SegyExportProfile::ExactIeee64;
        const bool ibm = options.profile == SegyExportProfile::LegacyIbm32;
        require(options.profile == SegyExportProfile::ExactIeee32 || ieee64 || compatible, "Unknown SEG-Y export profile.");
        require(std::isfinite(options.sampleGain) && options.sampleGain > 0, "SEG-Y export multiplier must be finite and positive.");
        require(std::isfinite(options.sampleIntervalSeconds) && options.sampleIntervalSeconds >= 0,
                "Invalid SEG-Y output sampling interval.");
        require(!records.traces.empty() && component >= 0 &&
                    component < static_cast<int>(records.components.size()),
                "Select an existing raw component before SEG-Y export.");
        require(std::isfinite(records.dt) && records.dt > 0, "Invalid source sampling interval.");
        const double outputDt = options.sampleIntervalSeconds > 0 ? options.sampleIntervalSeconds : records.dt;
        require(outputDt > 0 && outputDt * 1e6 <= 65535 && (!compatible || outputDt * 1e6 >= 1),
                "SEG-Y sample interval must be positive and at most 65535 microseconds.");
        require(!compatible || std::abs(outputDt * 1e6 - std::round(outputDt * 1e6)) < 1e-7,
                "Legacy SEG-Y requires whole-microsecond dt; select 0.5 or 1 ms output sampling.");
        const auto sourceCount = records.sampleCount(0);
        require(sourceCount > 0, "SEG-Y export requires nonempty traces.");
        const double outputCount = outputDt == records.dt ? static_cast<double>(sourceCount)
            : std::floor((sourceCount - 1) * records.dt / outputDt + 1e-9) + 1;
        require(std::isfinite(outputCount) && outputCount >= 1 && outputCount <= 65535,
                "SEG-Y export requires 1..65535 output samples per trace.");
        const auto count = static_cast<std::size_t>(outputCount);
        require(records.traces.size() <= 100000, "Too many SEG-Y traces.");
        for (const auto &trace : records.traces) {
            require(trace.channels[component].size() == sourceCount, "SEG-Y export requires equal trace lengths.");
            require(std::isfinite(trace.startTime) && std::abs(trace.startTime * 1000) <= 32767 &&
                        std::abs(trace.startTime * 1000 - std::round(trace.startTime * 1000)) < 1e-7,
                    "SEG-Y export delay must be representable as whole milliseconds.");
            for (double v : trace.position)
                millimetres(v);
            for (double v : trace.source)
                millimetres(v);
            for (double v : trace.channels[component])
                require(std::isfinite(v) && std::isfinite(v * options.sampleGain) &&
                            (ieee64 || ibm || std::abs(v * options.sampleGain) <= std::numeric_limits<float>::max()),
                        "SEG-Y samples/multiplier overflow the selected precision.");
        }
        const auto sample = [&](const TraceView &trace, std::size_t t) {
            if (outputDt == records.dt) return trace.channels[component][t] * options.sampleGain;
            const double u = std::clamp(t * outputDt / records.dt, 0.0, static_cast<double>(sourceCount - 1));
            const auto i = static_cast<std::size_t>(u);
            return std::lerp(trace.channels[component][i], trace.channels[component][std::min(i + 1, sourceCount - 1)], u - i) * options.sampleGain;
        };
        std::size_t underflow = 0;
        double outputPeak = 0;
        for (const auto &trace : records.traces)
            for (std::size_t t = 0; t < count; ++t) {
                const double value = sample(trace, t);
                outputPeak = std::max(outputPeak, std::abs(value));
                if (ibm) underflow += value != 0 && ibmFloat(value) == 0;
                else if (!ieee64) underflow += value != 0 && static_cast<float>(value) == 0;
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
        line(1, "SEISWAVE LAB - ELASTIC WAVE RECEIVER RECORDS");
        const auto units = options.sampleGain == 1 ? records.units : "scaled(" + records.units + ")";
        line(2, "COMPONENT=" + records.components[component] + " UNITS=" + units);
        std::ostringstream dt;
        dt << std::setprecision(17) << outputDt;
        line(3, "EXACT DT (SECONDS)=" + dt.str());
        line(4, "SEG-Y REV " + std::string(compatible ? "1.0" : "2.0") + ", BIG ENDIAN, " +
                    (ibm ? "IBM32" : ieee64 ? "IEEE64" : "IEEE32"));
        line(5, compatible ? "DT: BINARY BYTES 3217-3218; TRACE BYTES 117-118 (MICROSECONDS)"
                           : "EXACT DT: BINARY HEADER BYTES 3273-3280 (IEEE64 MICROSECONDS)");
        line(6, "COORDINATES/ELEVATIONS IN METRES; SCALARS=-1000; Z POSITIVE DOWN");
        line(7, "NO DISPLAY AGC OR TRACE NORMALIZATION; FIRST INPUT SAMPLE INCLUDED");
        line(8, "X IS INLINE, Y CROSSLINE; ONE VELOCITY COMPONENT PER FILE");
        std::ostringstream gain; gain << std::setprecision(17) << options.sampleGain;
        line(9, "EXPORT_MULTIPLIER=" + gain.str() + "; ORIGINAL_VALUE=STORED_VALUE/MULTIPLIER");
        line(10, "FFID=1; CHANNEL=1..N; CDP=1..N (ORDINAL, NOT A BINNED CMP GRID)");
        std::ostringstream sourceDt; sourceDt << std::setprecision(17) << records.dt;
        line(11, "SOURCE_DT_SECONDS=" + sourceDt.str() + "; RESAMPLING=" + (outputDt == records.dt ? "NONE" : "LINEAR"));
        std::ostringstream peak; peak << std::setprecision(17) << outputPeak;
        line(12, "OUTPUT_PEAK=" + peak.str() + "; UNDERFLOWED_SAMPLES=" + std::to_string(underflow));
        line(13, compatible ? "TRACE_ID=1 LIVE SEISMIC; COMPONENT IDENTIFIED ON C 2 / FILENAME"
                            : "TRACE_ID: 12=VERTICAL, 14=INLINE; COMPONENT IDENTIFIED ON C 2");
        line(40, "END TEXTUAL HEADER");
        if (ibm)
            for (auto &c : text) c = c < 128 ? ebcdic[c] : ebcdic['?'];
        write(out, text);
        Bytes binary(400);
        const auto interval =
            static_cast<std::uint16_t>(std::clamp(std::llround(outputDt * 1e6), 1LL, 65535LL));
        put(binary, 12, std::min<std::size_t>(65535, records.traces.size()), 2);
        put(binary, 16, interval, 2);
        put(binary, 18, interval, 2);
        put(binary, 20, count, 2);
        put(binary, 22, count, 2);
        put(binary, 24, ibm ? 1 : ieee64 ? 6 : 5, 2);
        put(binary, 26, 1, 2);
        put(binary, 28, 1, 2); // As recorded; do not claim CMP binning.
        put(binary, 54, 1, 2);
        if (!compatible) {
            put(binary, 60, records.traces.size(), 4);
            put(binary, 68, count, 4);
            put(binary, 72, std::bit_cast<std::uint64_t>(outputDt * 1e6), 8);
            put(binary, 80, std::bit_cast<std::uint64_t>(outputDt * 1e6), 8);
            put(binary, 88, count, 4);
            put(binary, 96, 0x01020304, 4);
        }
        binary[300] = compatible ? 1 : 2;
        put(binary, 302, 1, 2);
        if (!compatible) {
            put(binary, 312, records.traces.size(), 8);
            put(binary, 320, 3600, 8);
        }
        write(out, binary);
        for (std::size_t r = 0; r < records.traces.size(); ++r) {
            const auto &trace = records.traces[r];
            Bytes header(240);
            put(header, 0, r + 1, 4);
            put(header, 4, r + 1, 4);
            put(header, 8, 1, 4);
            put(header, 12, r + 1, 4);
            put(header, 16, 1, 4);
            put(header, 20, r + 1, 4);
            put(header, 24, 1, 4);
            const auto name = records.components[component];
            put(header, 28, compatible ? 1 : name == "Vz" ? 12 : name == "Vx" ? 14 : 1, 2);
            put(header, 30, 1, 2); put(header, 32, 1, 2); put(header, 34, 1, 2);
            const double dx = trace.position[0] - trace.source[0], dy = trace.position[1] - trace.source[1];
            const double offset = std::copysign(std::hypot(dx, dy), dx != 0 ? dx : dy);
            require(std::abs(offset) <= std::numeric_limits<std::int32_t>::max(), "SEG-Y offset exceeds Int32 metres.");
            put(header, 36, static_cast<std::uint32_t>(static_cast<std::int32_t>(std::llround(offset))), 4);
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
            put(header, 180, static_cast<std::uint32_t>(millimetres((trace.source[0] + trace.position[0]) * .5)), 4);
            put(header, 184, static_cast<std::uint32_t>(millimetres((trace.source[1] + trace.position[1]) * .5)), 4);
            put(header, 202, options.sampleGain != 1 ? 0 : records.units == "m/s" ? 6 : records.units == "Pa" ? 1 : 0, 2);
            write(out, header);
            Bytes samples(count * (ieee64 ? 8 : 4));
            for (std::size_t t = 0; t < count; ++t) {
                const auto v = sample(trace, t);
                if (ibm)
                    put(samples, t * 4, ibmFloat(v), 4);
                else if (ieee64)
                    put(samples, t * 8, std::bit_cast<std::uint64_t>(v), 8);
                else
                    put(samples, t * 4, std::bit_cast<std::uint32_t>(static_cast<float>(v)), 4);
            }
            write(out, samples);
        }
        out.close();
        require(static_cast<bool>(out), "SEG-Y final flush failed.");
        SeisWave::replaceCompletedFile(temporary,path);
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
        std::string textual(text.begin(), text.end());
        if (text.front() == ebcdic['C']) {
            for (std::size_t n = 0; n < text.size(); ++n) {
                const auto found = std::find(std::begin(ebcdic), std::end(ebcdic), text[n]);
                textual[n] = found == std::end(ebcdic) ? '?' : static_cast<char>(found - std::begin(ebcdic));
            }
        }
        const bool ownText = textual.find("SEISWAVE LAB") != std::string::npos;
        const auto token = [&](const char *key) {
            const auto row = textual.substr(80, 80);
            const auto start = row.find(key);
            if (start == std::string::npos) return std::string();
            const auto begin = start + std::char_traits<char>::length(key);
            return row.substr(begin, row.find(' ', begin) - begin);
        };
        if (ownText && !token("UNITS=").empty())
            result.units = token("UNITS=");
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
        if (ownText && !token("COMPONENT=").empty())
            result.component = token("COMPONENT=");
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
