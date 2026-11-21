#include <cstdio>
#include <cmath>
#include <chrono>
#include "../../build_normal/source/program/hutao_mesh.hpp"
#include <vector>
int main(int argc, char** argv) {
    float yaw = atof(argv[1]) * 3.14159265f / 180, pit = atof(argv[2]) * 3.14159265f / 180; const char* fn = argv[3];
    const float H = 1080, up = 2.6f / 19.5f * 1037.0f / 3.5f; const int q = argc > 4 ? atoi(argv[4]) : 1;
    float f[3] = { cosf(pit) * cosf(yaw), sinf(pit), cosf(pit) * sinf(yaw) };
    float ch = sqrtf(f[0] * f[0] + f[2] * f[2]); float r[3] = { -f[2] / ch, 0, f[0] / ch };
    float u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    { static std::vector<unsigned char> t0(696320), t1(172032), t2(696320); FILE* f; f=fopen("/home/claude/tmt/tools/hutao_sd/tex0.bin","rb"); fread(t0.data(),1,t0.size(),f); fclose(f); f=fopen("/home/claude/tmt/tools/hutao_sd/tex1.bin","rb"); fread(t1.data(),1,t1.size(),f); fclose(f); f=fopen("/home/claude/tmt/tools/hutao_sd/tex2.bin","rb"); fread(t2.data(),1,t2.size(),f); fclose(f); if (getenv("HI")) { hutao::g_Hi[0]=t0.data(); hutao::g_Hi[1]=t1.data(); hutao::g_Hi[2]=t2.data(); } }
    static std::vector<uint16_t> zb(hutao::kBudget); static std::vector<float> av(hutao::kNV * 3); static std::vector<int8_t> an(hutao::kNV * 3);
    hutao::g_Zp = zb.data(); hutao::g_AV = av.data(); hutao::g_AN = an.data();
    if (argc > 5) { const float ph = atof(argv[5]), run = argc > 6 ? atof(argv[6]) : 0.0f; /* 歩き/走り (run 0..1) */
        float rn = run, mv = 1.0f; if (getenv("IDLE")) { hutao::Pose(0, 0, 0, 0, atof(getenv("EL")), 0, 0, atof(getenv("LOW")), atof(getenv("AB")), 0); } else hutao::Pose(ph, mv * (0.40f + 0.20f * rn), mv * (0.7f + 1.0f * rn), mv * (0.28f - 0.14f * rn), 0.10f + mv * (0.08f + 1.30f * rn), mv * 0.04f + rn * 0.40f, mv * (0.10f + 0.20f * rn) * cosf(2 * ph), atof(getenv("LOW")), rn * atof(getenv("AB")), rn * 0.55f); }
    int mx, my, w, h; hutao::Project(up, up / q, r, u, f, mx, my, w, h); fprintf(stderr, "%dx%d q=%d px=%d (budget %d)\n", w, h, q, w * h, hutao::kBudget);
    std::vector<uint32_t> buf(w * h);
    auto t0 = std::chrono::steady_clock::now();
    hutao::Draw(buf.data(), w, h, mx, my, r, u, f);
    auto t1 = std::chrono::steady_clock::now();
    fprintf(stderr, "%.1f ms\n", std::chrono::duration<double, std::milli>(t1 - t0).count());
    const int S = w, SH = h * q; (void)H;
    FILE* o = fopen(fn, "wb"); fprintf(o, "P6\n%d %d\n255\n", S, SH);
    for (int i = 0; i < S * SH; i++) { uint32_t c = buf[(i / S / q) * S + i % S]; unsigned char p[3]; if (c >> 24) { p[0] = c & 255; p[1] = (c >> 8) & 255; p[2] = (c >> 16) & 255; } else { p[0] = 70; p[1] = 90; p[2] = 110; } fwrite(p, 1, 3, o); }
    fclose(o);
}
