#include <cstdio>
#include <cmath>
#include <chrono>
#include "../../build_normal/source/program/hutao_mesh.hpp"
#include <vector>
int main(int argc, char** argv) {
    float yaw = atof(argv[1]) * 3.14159265f / 180, pit = atof(argv[2]) * 3.14159265f / 180; const char* fn = argv[3];
    const int S = 360; const float upx = S / (2 * hutao::kRadius) * 0.9f;
    float f[3] = { cosf(pit) * cosf(yaw), sinf(pit), cosf(pit) * sinf(yaw) };
    float ch = sqrtf(f[0] * f[0] + f[2] * f[2]); float r[3] = { -f[2] / ch, 0, f[0] / ch };
    float u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    std::vector<uint32_t> buf(S * S);
    auto t0 = std::chrono::steady_clock::now();
    hutao::Render(buf.data(), S, upx, r, u, f);
    auto t1 = std::chrono::steady_clock::now();
    fprintf(stderr, "%.1f ms\n", std::chrono::duration<double, std::milli>(t1 - t0).count());
    FILE* o = fopen(fn, "wb"); fprintf(o, "P6\n%d %d\n255\n", S, S);
    for (int i = 0; i < S * S; i++) { uint32_t c = buf[i]; unsigned char p[3]; if (c >> 24) { p[0] = c & 255; p[1] = (c >> 8) & 255; p[2] = (c >> 16) & 255; } else { p[0] = 70; p[1] = 90; p[2] = 110; } fwrite(p, 1, 3, o); }
    fclose(o);
}
