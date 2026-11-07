// Stand-in Xbox Avatar system. The SDK (like Xenia) fails XamAvatarInitialize, so the game never builds
// the player's model. Here XAM "has" one avatar for every request: the game's own Cal3D dummy model,
// converted by scripts/make_avatar_blob.py into the render-ready layout XamAvatarGetAssets returns
// (avatar_dummy.bin, relocatable images of the main and physical buffers).
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <mutex>
#include <vector>

#include <windows.h>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "generated/default/doritoscrashcourse_init.h"

namespace {

struct Blob {
  bool ok = false;
  uint32_t main_size = 0, gpu_size = 0;
  std::vector<uint8_t> main, gpu;
  std::vector<uint32_t> relocs;
};

std::filesystem::path ExeDir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return std::filesystem::path(buf).parent_path();
}

const Blob& GetBlob() {
  static Blob blob;
  static std::once_flag once;
  std::call_once(once, [] {
    std::vector<std::filesystem::path> candidates;
    if (const char* p = std::getenv("DORITOS_AVATAR_BLOB"); p && *p) candidates.emplace_back(p);
    candidates.push_back(ExeDir() / "avatar_dummy.bin");
    candidates.emplace_back(R"(D:\CodexBuilds\doritos\game_files\avatar_dummy.bin)");
    for (const auto& path : candidates) {
      std::ifstream f(path, std::ios::binary);
      if (!f) continue;
      std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
      if (d.size() < 20 || std::memcmp(d.data(), "DAVA", 4)) continue;
      auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, d.data() + o, 4); return v; };
      blob.main_size = u32(8);
      blob.gpu_size = u32(12);
      uint32_t n = u32(16);
      size_t o = 20;
      blob.main.assign(d.begin() + o, d.begin() + o + blob.main_size);
      o += blob.main_size;
      blob.gpu.assign(d.begin() + o, d.begin() + o + blob.gpu_size);
      o += blob.gpu_size;
      for (uint32_t i = 0; i < n; i++) blob.relocs.push_back(u32(o + 4 * i));
      blob.ok = true;
      REXLOG_INFO("avatar: stand-in assets {} (main {} bytes, gpu {} bytes, {} relocs)",
                  path.string(), blob.main_size, blob.gpu_size, n);
      return;
    }
    REXLOG_WARN("avatar: avatar_dummy.bin not found; players will have no model");
  });
  return blob;
}

void Complete(uint32_t overlapped) {
  if (overlapped) rex::system::kernel_state()->CompleteOverlappedImmediate(overlapped, 0);
}

// Guest address -> host pointer through the SDK (0xE0000000+ physical views are not base + address).
inline uint8_t* Host(uint32_t a) {
  return rex::system::kernel_state()->memory()->TranslateVirtual<uint8_t*>(a);
}
inline void Store32(uint8_t*, uint32_t a, uint32_t v) {
  v = _byteswap_ulong(v);
  std::memcpy(Host(a), &v, 4);
}
inline uint32_t Load32(uint8_t*, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, Host(a), 4);
  return _byteswap_ulong(v);
}

}  // namespace

// XamAvatarInitialize(...): succeed (the SDK returns failure, which disables the game's avatar manager).
bool AvatarEnabled() {
  const char* e = std::getenv("DORITOS_AVATAR");
  return !(e && e[0] == '0');
}

REX_HOOK_RAW(__imp__XamAvatarInitialize) {
  (void)base;
  if (!AvatarEnabled()) {
    ctx.r3.u64 = 0xFFFFFFFF;  // like the SDK/Xenia: avatar system unavailable
    return;
  }
  REXLOG_INFO("avatar: guest memory base {}", (void*)Host(0x40000000));
  REXLOG_INFO("avatar: XamAvatarInitialize({:#x}, {:#x}, {:#x}) -> S_OK", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
  GetBlob();
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XamAvatarShutdown) {
  (void)base;
  ctx.r3.u64 = 0;
}

// XamAvatarGetManifestLocalUser(user_index, manifest*, overlapped*)
REX_HOOK_RAW(__imp__XamAvatarGetManifestLocalUser) {
  (void)base;
  REXLOG_INFO("avatar: GetManifestLocalUser(user {}, buf {:#x}, ovl {:#x})", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
  Complete(ctx.r5.u32);
  ctx.r3.u64 = 0;
}

// XamAvatarGetMetadataRandom(body_type, count, metadata*, overlapped*)
REX_HOOK_RAW(__imp__XamAvatarGetMetadataRandom) {
  (void)base;
  REXLOG_INFO("avatar: GetMetadataRandom({}, {}, {:#x}, ovl {:#x})", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
  Complete(ctx.r6.u32);
  ctx.r3.u64 = 0;
}

// XamAvatarGetMetadataSignedOutProfileCount(u32* count, ...)
REX_HOOK_RAW(__imp__XamAvatarGetMetadataSignedOutProfileCount) {
  if (ctx.r3.u32) Store32(base, ctx.r3.u32, 0);
  ctx.r3.u64 = 0;
}

// XamAvatarGetMetadataSignedOutProfile(index, metadata*, overlapped*)
REX_HOOK_RAW(__imp__XamAvatarGetMetadataSignedOutProfile) {
  (void)base;
  Complete(ctx.r5.u32);
  ctx.r3.u64 = 0;
}

// XamAvatarGetAssetsResultSize(component_mask, u32* main_size, u32* gpu_size)
REX_HOOK_RAW(__imp__XamAvatarGetAssetsResultSize) {
  const Blob& b = GetBlob();
  REXLOG_INFO("avatar: GetAssetsResultSize(mask {:#x}) -> {} / {}", ctx.r3.u32, b.main_size, b.gpu_size);
  if (!b.ok) {
    ctx.r3.u64 = 0x80004005;  // E_FAIL
    return;
  }
  Store32(base, ctx.r4.u32, b.main_size);
  Store32(base, ctx.r5.u32, b.gpu_size);
  ctx.r3.u64 = 0;
}

// XamAvatarGetAssets(manifest*, component_mask, flags, main_buffer, gpu_buffer, overlapped*)
REX_HOOK_RAW(__imp__XamAvatarGetAssets) {
  const Blob& b = GetBlob();
  uint32_t main = ctx.r6.u32, gpu = ctx.r7.u32, ovl = ctx.r8.u32;
  REXLOG_INFO("avatar: GetAssets(manifest {:#x}, mask {:#x}, flags {:#x}, main {:#x}, gpu {:#x}, ovl {:#x})",
              ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, main, gpu, ovl);
  if (!b.ok || !main || !gpu) {
    ctx.r3.u64 = 0x80004005;
    return;
  }
  std::memcpy(Host(main), b.main.data(), b.main.size());
  std::memcpy(Host(gpu), b.gpu.data(), b.gpu.size());
  for (uint32_t r : b.relocs) {
    uint32_t at = r & 0x7FFFFFFF;
    uint32_t target = (r & 0x80000000) ? gpu : main;
    Store32(base, main + at, Load32(base, main + at) + target);
  }
  Complete(ovl);
  ctx.r3.u64 = 0;
}

// ---------------------------------------------------------------------------------------------
// DORITOS_AVATAR_TRACE=1: log the avatar draw path (counts once per ~2 s).
// ---------------------------------------------------------------------------------------------
namespace {
bool Trace() {
  static const bool on = [] { const char* e = std::getenv("DORITOS_AVATAR_TRACE"); return e && *e == '1'; }();
  return on;
}
std::atomic<uint32_t> g_parts_calls{0}, g_pass_calls{0};
uint32_t g_last_log = 0;
}  // namespace

REX_HOOK_RAW(sub_823919D0) {
  if (Trace()) {
    uint32_t res = ctx.r3.u32;
    uint32_t n = ++g_parts_calls;
    uint32_t now = GetTickCount();
    if (now - g_last_log > 2000) {
      g_last_log = now;
      uint32_t master = Load32(base, res + 0x40), mgr = Load32(base, res + 0x3C);
      uint32_t idx = Load32(base, res + 0x148);
      uint32_t arr = master ? Load32(base, master + 0x214) : 0;
      uint32_t mb = arr ? Load32(base, arr + 4 * idx) : 0;
      REXLOG_INFO("avatar trace: parts-draw #{} res {:#x} mgr {:#x} master {:#x} idx {} main {:#x} parts {} "
                  "pass-calls {} palette[0] {:08X} {:08X} {:08X} {:08X}",
                  n, res, mgr, master, idx, mb, mb ? Load32(base, mb + 8) : 0, g_pass_calls.load(),
                  Load32(base, 0x831E7E00), Load32(base, 0x831E7E04), Load32(base, 0x831E7E08),
                  Load32(base, 0x831E7E0C));
    }
  }
  __imp__sub_823919D0(ctx, base);
}

REX_HOOK_RAW(sub_82392028) {
  if (Trace()) ++g_pass_calls;
  __imp__sub_82392028(ctx, base);
}

// Trace: after the palette update, report the skeleton object (key at res+0x140) and how many palette
// bones are NaN.
std::atomic<uint64_t> g_skel_key{0};
// Trace-only guest read that tolerates bad pointers (returns 0).
uint32_t SafeLoad32(uint32_t a) {
  if (a < 0x10000 || a >= 0xF0000000u) return 0;
  uint8_t* p = Host(a);
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(p, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
    return 0;
  uint32_t v;
  std::memcpy(&v, p, 4);
  return _byteswap_ulong(v);
}
std::atomic<uint32_t> g_skel_obj{0};

// Handle lookup (resource table): remember what the avatar's skeleton key resolves to.
REX_HOOK_RAW(sub_8223B8F8) {
  uint64_t key = ctx.r4.u64;
  __imp__sub_8223B8F8(ctx, base);
  if (Trace() && key == g_skel_key.load()) g_skel_obj = ctx.r3.u32;
}

REX_HOOK_RAW(sub_82391A68) {
  uint32_t res = ctx.r3.u32;
  if (Trace()) g_skel_key = (uint64_t(SafeLoad32(res + 0x140)) << 32) | SafeLoad32(res + 0x144);
  __imp__sub_82391A68(ctx, base);
  if (!Trace()) return;
  static uint32_t last = 0;
  uint32_t now = GetTickCount();
  if (now - last < 3000) return;
  last = now;
  uint32_t obj = g_skel_obj.load();
  uint32_t vt = obj ? SafeLoad32(obj) : 0;
  int nan = 0;
  for (int i = 0; i < 72 * 12; i++)
    if ((SafeLoad32(0x831E7E00 + 4 * i) & 0x7F800000) == 0x7F800000) nan++;
  REXLOG_INFO("avatar trace: res {:#x} key {:08X}{:08X} skel-obj {:#x} vt {:#x} count-fn {:#x} matrix-fn {:#x} "
              "palette NaN floats {}/864", res, SafeLoad32(res + 0x140), SafeLoad32(res + 0x144), obj, vt,
              vt ? SafeLoad32(vt + 440) : 0, vt ? SafeLoad32(vt + 488) : 0, nan);
  for (int b = 0; b < 3; b++) {
    uint32_t p = 0x831E7E00 + 48 * b;
    REXLOG_INFO("avatar trace: palette[{}] {:08X} {:08X} {:08X} {:08X} | {:08X} {:08X} {:08X} {:08X} | {:08X} {:08X} {:08X} {:08X}",
                b, SafeLoad32(p), SafeLoad32(p + 4), SafeLoad32(p + 8), SafeLoad32(p + 12),
                SafeLoad32(p + 16), SafeLoad32(p + 20), SafeLoad32(p + 24), SafeLoad32(p + 28),
                SafeLoad32(p + 32), SafeLoad32(p + 36), SafeLoad32(p + 40), SafeLoad32(p + 44));
  }
  if (obj && SafeLoad32(obj + 0x268)) {
    uint32_t x = SafeLoad32(SafeLoad32(obj + 0x268) + 4);
    uint32_t nodes = SafeLoad32(x + 4), n = (SafeLoad32(x + 8) - nodes) >> 2;
    auto f = [&](uint32_t a) { uint32_t v = SafeLoad32(a); float r; std::memcpy(&r, &v, 4); return r; };
    for (uint32_t i = 0; i < n && i < 80; i++) {
      uint32_t nd = SafeLoad32(nodes + 4 * i);
      REXLOG_INFO("avatar node {:2} {:#x} parent? {:#x} pos {:.3f} {:.3f} {:.3f} rot {:.3f} {:.3f} {:.3f} {:.3f} m {:.3f} {:.3f} {:.3f}",
                  i, nd, SafeLoad32(nd + 8), f(nd + 0x48), f(nd + 0x4C), f(nd + 0x50), f(nd + 0x54), f(nd + 0x58),
                  f(nd + 0x5C), f(nd + 0x60), f(nd + 0x64), f(nd + 0x68), f(nd + 0x6C));
    }
  }
  if (obj)
    for (int r = 0; r < 8; r++) {
      uint32_t p = obj + 16 * r;
      REXLOG_INFO("avatar trace: obj+{:#x} {:08X} {:08X} {:08X} {:08X}", 16 * r, SafeLoad32(p),
                  SafeLoad32(p + 4), SafeLoad32(p + 8), SafeLoad32(p + 12));
    }
}

// Trace: skeleton build (sub_82394968). Log each avatar resource's object/vtable before, and whether the
// node transforms are NaN right after the build (separates build-time NaN from animation-time NaN).
namespace {
uint32_t ResolveHandle(uint64_t key) {
  int16_t idx = int16_t(key >> 16);
  if (idx < 0 || idx > 0x3FFF) return 0;
  uint32_t mgr = SafeLoad32(0x82BB0000 - 0x678C);
  uint32_t obj = mgr ? SafeLoad32(mgr + 0x34 + 0x10 * uint32_t(idx)) : 0;
  if (!obj) return 0;
  uint64_t k = (uint64_t(SafeLoad32(obj + 0x10)) << 32) | SafeLoad32(obj + 0x14);
  return k == key ? obj : 0;
}
float F(uint32_t a) { uint32_t v = SafeLoad32(a); float r; std::memcpy(&r, &v, 4); return r; }
void DumpNodes(const char* tag, uint32_t obj) {
  uint32_t h = SafeLoad32(obj + 0x268);
  if (!h) { REXLOG_INFO("avatar build {}: obj {:#x} has no node set", tag, obj); return; }
  uint32_t x = SafeLoad32(h + 4);
  uint32_t nodes = SafeLoad32(x + 4), n = (SafeLoad32(x + 8) - nodes) >> 2;
  int bad = 0;
  for (uint32_t i = 0; i < n && i < 200; i++) {
    uint32_t nd = SafeLoad32(nodes + 4 * i);
    for (int k = 0; k < 7; k++) if ((SafeLoad32(nd + 0x48 + 4 * k) & 0x7F800000) == 0x7F800000) { bad++; break; }
  }
  REXLOG_INFO("avatar build {}: obj {:#x} nodes {} NaN-nodes {}", tag, obj, n, bad);
  for (uint32_t i = 0; i < n && i < 4; i++) {
    uint32_t nd = SafeLoad32(nodes + 4 * i);
    REXLOG_INFO("avatar build {}:  node {} pos {:.3f} {:.3f} {:.3f} rot {:.3f} {:.3f} {:.3f} {:.3f}", tag, i,
                F(nd + 0x48), F(nd + 0x4C), F(nd + 0x50), F(nd + 0x54), F(nd + 0x58), F(nd + 0x5C), F(nd + 0x60));
  }
}
}  // namespace

REX_HOOK_RAW(sub_82394968) {
  uint32_t self = ctx.r3.u32;
  std::vector<uint32_t> objs;
  if (Trace()) {
    int32_t n = int32_t(SafeLoad32(self + 0x200));
    uint32_t keys = SafeLoad32(self + 0x20C);
    REXLOG_INFO("avatar build: self {:#x} count {} scale(+0x170) {:.3f}", self, n, F(self + 0x170));
    for (int32_t i = 0; i < n && i < 8; i++) {
      uint64_t key = (uint64_t(SafeLoad32(keys + 8 * i)) << 32) | SafeLoad32(keys + 8 * i + 4);
      uint32_t obj = ResolveHandle(key), vt = obj ? SafeLoad32(obj) : 0;
      REXLOG_INFO("avatar build: [{}] key {:016X} obj {:#x} vt {:#x} +84 {:#x} +A0 {:#x} +C0 {:#x} +C4 {:#x}", i, key,
                  obj, vt, vt ? SafeLoad32(vt + 0x84) : 0, vt ? SafeLoad32(vt + 0xA0) : 0, vt ? SafeLoad32(vt + 0xC0) : 0,
                  vt ? SafeLoad32(vt + 0xC4) : 0);
      if (obj) { objs.push_back(obj); DumpNodes("before", obj); }
    }
  }
  __imp__sub_82394968(ctx, base);
  if (Trace()) {
    int32_t n = int32_t(SafeLoad32(self + 0x200));
    uint32_t keys = SafeLoad32(self + 0x20C);
    for (int32_t i = 0; i < n && i < 8; i++) {
      uint64_t key = (uint64_t(SafeLoad32(keys + 8 * i)) << 32) | SafeLoad32(keys + 8 * i + 4);
      if (uint32_t obj = ResolveHandle(key)) {
        uint32_t vt = SafeLoad32(obj);
        REXLOG_INFO("avatar build after: obj {:#x} vt {:#x} +84 {:#x} +A0 {:#x} +C0 {:#x} +C4 {:#x} +268 {:#x}", obj, vt,
                    SafeLoad32(vt + 0x84), SafeLoad32(vt + 0xA0), SafeLoad32(vt + 0xC0), SafeLoad32(vt + 0xC4),
                    SafeLoad32(obj + 0x268));
        if (vt == 0x82161C24) DumpNodes("after", obj);
      }
    }
  }
}

// Trace: pixel-shader constants seen by the avatar draw (avatar_ps: c4 g_ambient, c5 rimLightColour,
// c210-212 CustomColor1-3, c224 rimLightingPower, c225 g_ambientColor, c226 g_diffuseColor). The device
// constant shadow is *(0x82BAC23C); PS constant n lives at +0x1780 + 16 n (sub_823927D8).
REX_HOOK_RAW(sub_823927D8) {
  __imp__sub_823927D8(ctx, base);
  if (!Trace()) return;
  static uint32_t last = 0;
  uint32_t now = GetTickCount();
  if (now - last < 4000) return;
  last = now;
  uint32_t dev = SafeLoad32(0x82BAC23C);
  for (uint32_t reg : {4u, 5u, 210u, 211u, 212u, 224u, 225u, 226u}) {
    uint32_t a = dev + 0x1780 + 16 * reg;
    REXLOG_INFO("avatar ps c{}: {:.3f} {:.3f} {:.3f} {:.3f}", reg, F(a), F(a + 4), F(a + 8), F(a + 12));
  }
}
