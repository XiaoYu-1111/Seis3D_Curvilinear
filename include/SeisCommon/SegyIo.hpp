#pragma once
#include "SeismicRecords.hpp"
#include <filesystem>
namespace SeisWave {
enum class SegyExportProfile { ExactIeee32, ExactIeee64, LegacyIbm32, CompatibleIeee32 };
struct SegyExportOptions {
    SegyExportProfile profile{SegyExportProfile::ExactIeee32};
    double sampleGain{1}; // One constant for all traces. Never a display gain or per-trace normalization.
    double sampleIntervalSeconds{}; // 0 preserves dt; positive values explicitly resample linearly.
};
bool writeSegy(const RecordDataView &, int component, const std::filesystem::path &, std::string &error,
               const SegyExportOptions &);
// SEG-Y rev 2, big endian, one selected component per file. Extended binary
// interval preserves the solver's non-integer-microsecond dt without resampling.
bool writeSegy(const RecordDataView &, int component, const std::filesystem::path &, std::string &error,
               bool ieee64 = false);
// Rev 0/1 and the supported fixed-header subset of rev 2, IBM/IEEE/integer data.
// Failure leaves the destination unchanged. Limits bound untrusted allocations.
bool readSegy(const std::filesystem::path &, ImportedRecords &, std::string &error);
} // namespace SeisWave
