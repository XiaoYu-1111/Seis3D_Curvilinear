#pragma once
// Shared DRP coefficients and direction schedule; no dimension-specific storage.
#ifdef __CUDACC__
#define SEIS_SHARED_HD __host__ __device__ inline
#else
#define SEIS_SHARED_HD inline
#endif
namespace SeisWave::Difference {
SEIS_SHARED_HD int direction(int step, int stage, int axis) {
    const int flags[8][3]{{0, 0, 0}, {1, 1, 0}, {1, 1, 1}, {0, 0, 1},
                          {0, 1, 0}, {1, 0, 0}, {1, 0, 1}, {0, 1, 1}};
    return (flags[step % 8][axis] + stage) % 2;
}
SEIS_SHARED_HD int stencil(int dir, int depth, int *offsets, double *coefficients) {
    const int n = depth == 1 ? 2 : depth == 2 ? 3 : 5;
    const double forward[5]{-.30874, -.6326, 1.233, -.3334, .04168};
    const double lower[3]{-7.0 / 6, 8.0 / 6, -1.0 / 6};
    for (int l = 0; l < n; ++l) {
        const int pos = n == 5 ? l - 1 : l;
        const double c = n == 5 ? forward[l] : n == 3 ? lower[l] : l == 0 ? -1.0 : 1.0;
        offsets[l] = dir ? -pos : pos;
        coefficients[l] = dir ? -c : c;
    }
    return n;
}
} // namespace SeisWave::Difference
#undef SEIS_SHARED_HD
