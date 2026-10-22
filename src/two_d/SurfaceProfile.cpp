#include "Seis2D/SurfaceProfile.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace Seis2D::Curvilinear {
SurfaceProfile::SurfaceProfile(const SurfaceSamples &points) : points_(points) {
    if (points.size() < 2 || points.size() > 1000000)
        throw std::runtime_error("Surface profile requires 2..1000000 samples.");
    std::vector<double> h(points.size() - 1), d(h.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (!std::isfinite(points[i][0]) || !std::isfinite(points[i][1]) ||
            (i && points[i][0] <= points[i - 1][0]))
            throw std::runtime_error("Surface samples must be finite with strictly increasing x.");
        if (i) {
            h[i - 1] = points[i][0] - points[i - 1][0];
            d[i - 1] = (points[i][1] - points[i - 1][1]) / h[i - 1];
            if (!std::isfinite(h[i - 1]) || !std::isfinite(d[i - 1]))
                throw std::runtime_error("Surface sample differences overflow.");
        }
    }
    slopes_.resize(points.size());
    slopes_.front() = d.front();
    slopes_.back() = d.back();
    if (points.size() == 2)
        return;
    const auto endpoint = [](double h0, double h1, double d0, double d1) {
        double m = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
        if (m * d0 <= 0)
            return 0.0;
        if (d0 * d1 < 0 && std::abs(m) > 3 * std::abs(d0))
            return 3 * d0;
        return m;
    };
    slopes_.front() = endpoint(h[0], h[1], d[0], d[1]);
    const auto n = h.size();
    slopes_.back() = endpoint(h[n - 1], h[n - 2], d[n - 1], d[n - 2]);
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        if (d[i - 1] * d[i] <= 0)
            slopes_[i] = 0;
        else {
            const double a = 2 * h[i] + h[i - 1], b = h[i] + 2 * h[i - 1];
            slopes_[i] = (a + b) / (a / d[i - 1] + b / d[i]);
        }
    }
    for (double slope : slopes_)
        if (!std::isfinite(slope))
            throw std::runtime_error("Surface interpolation coefficients overflow.");
}
std::array<double, 2> SurfaceProfile::sample(double x) const {
    // Linear continuation is used only for the solver's three ghost columns.
    if (x <= points_.front()[0])
        return {points_.front()[1] + (x - points_.front()[0]) * slopes_.front(), slopes_.front()};
    if (x >= points_.back()[0])
        return {points_.back()[1] + (x - points_.back()[0]) * slopes_.back(), slopes_.back()};
    const auto upper = std::upper_bound(points_.begin(), points_.end(), x,
                                        [](double value, const auto &p) { return value < p[0]; });
    const auto i = static_cast<std::size_t>(upper - points_.begin() - 1);
    const double h = points_[i + 1][0] - points_[i][0], t = (x - points_[i][0]) / h;
    const double a = points_[i][1], b = points_[i + 1][1], m = slopes_[i], n = slopes_[i + 1];
    return {(2 * t * t * t - 3 * t * t + 1) * a + (t * t * t - 2 * t * t + t) * h * m +
                (-2 * t * t * t + 3 * t * t) * b + (t * t * t - t * t) * h * n,
            (6 * t * t - 6 * t) * (a - b) / h + (3 * t * t - 4 * t + 1) * m + (3 * t * t - 2 * t) * n};
}
SurfaceSamples resampleSurfaceProfile(const SurfaceSamples &source, double targetSpan, std::size_t count) {
    if (!std::isfinite(targetSpan) || targetSpan <= 0 || count < 2 || count > 1000000)
        throw std::runtime_error("Surface resampling requires positive finite span and 2..1000000 nodes.");
    const SurfaceProfile curve(source);
    const double origin = source.front()[0], span = source.back()[0] - origin;
    if (!std::isfinite(span) || span <= 0)
        throw std::runtime_error("Surface profile span is invalid.");
    SurfaceSamples result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(count - 1);
        const double x = i + 1 == count ? targetSpan : t * targetSpan;
        const double elevation = i == 0 ? source.front()[1]
                                 : i + 1 == count ? source.back()[1]
                                                  : curve.sample(origin + t * span)[0];
        if (!std::isfinite(elevation) || (i && x <= result.back()[0]))
            throw std::runtime_error("Resampled surface is not finite or strictly increasing.");
        result.push_back({x, elevation});
    }
    return result;
}
bool readSurfaceProfile(const std::filesystem::path &path, SurfaceSamples &destination, std::string &error) {
    try {
        if (std::filesystem::file_size(path) > 64 * 1024 * 1024)
            throw std::runtime_error("Surface profile file exceeds 64 MiB.");
        std::ifstream in(path);
        if (!in)
            throw std::runtime_error("Cannot open surface profile.");
        SurfaceSamples next;
        std::string line;
        std::size_t number = 0;
        bool headerAllowed = true;
        while (std::getline(in, line)) {
            ++number;
            if (number == 1 && line.starts_with("\xef\xbb\xbf"))
                line.erase(0, 3);
            if (const auto comment = line.find('#'); comment != std::string::npos)
                line.resize(comment);
            for (char &c : line)
                if (c == ',' || c == ';' || c == '\t')
                    c = ' ';
            if (line.find_first_not_of(" \r\n") == std::string::npos)
                continue;
            std::istringstream row(line);
            double x{}, z{};
            std::string extra;
            if (!(row >> x >> z) || (row >> extra)) {
                const auto first = line.find_first_not_of(" ");
                if (headerAllowed && first != std::string::npos &&
                    (line[first] == 'x' || line[first] == 'X')) {
                    headerAllowed = false;
                    continue;
                }
                throw std::runtime_error("Surface profile: expected two numeric columns at line " +
                                         std::to_string(number));
            }
            headerAllowed = false;
            next.push_back({x, z});
            if (next.size() > 1000000)
                throw std::runtime_error("Too many surface samples.");
        }
        if (!in.eof())
            throw std::runtime_error("Surface profile read failed.");
        SurfaceProfile checked(next);
        destination = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
} // namespace Seis2D::Curvilinear
