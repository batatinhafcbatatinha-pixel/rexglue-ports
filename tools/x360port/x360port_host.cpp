// Generic port host additions (tools/x360port), compiled into every generic Xbox 360 port.
//
// Low guest memory: on the console the 4 KB-page heap starts at 0x00010000, and titles read there through null
// pointers plus an offset before the pointer is set (Skate 3 reads 0x00012160 from a global its own init fills in a
// few instructions later, and overwrites what it read). There the read returns whatever is mapped; under the runtime
// the page is not committed and the read is a fatal access violation. A read of an uncommitted page in the low 16 MB
// of guest memory gets a zero page, read-only, and the instruction runs again. Writes are left alone: a write there is
// a real bug the runtime should report.
#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

LONG CALLBACK low_guest_read(PEXCEPTION_POINTERS ep) {
  if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
      ep->ExceptionRecord->ExceptionInformation[0] != 0)
    return EXCEPTION_CONTINUE_SEARCH;
  const uintptr_t addr = (uintptr_t)ep->ExceptionRecord->ExceptionInformation[1];
  const uintptr_t guest = addr & 0xFFFFFFFFull;   // the guest arena is 4 GB aligned
  if ((addr >> 32) == 0 || guest < 0x10000 || guest >= 0x01000000)
    return EXCEPTION_CONTINUE_SEARCH;
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery((void *)addr, &mbi, sizeof mbi) || mbi.State == MEM_COMMIT)
    return EXCEPTION_CONTINUE_SEARCH;               // committed: a protection fault, the runtime's to handle
  if (!VirtualAlloc((void *)(addr & ~(uintptr_t)0xFFF), 0x1000, MEM_COMMIT, PAGE_READONLY))
    return EXCEPTION_CONTINUE_SEARCH;
  static volatile LONG shown;
  if (InterlockedIncrement(&shown) <= 16)
    fprintf(stderr, "[x360port] read of unmapped low guest memory 0x%08llX: zero page mapped\n",
            (unsigned long long)guest);
  return EXCEPTION_CONTINUE_EXECUTION;
}

// Fatal host exceptions other than guest memory faults (an illegal or privileged instruction, a division by zero):
// the faulting code and the call stack as offsets in the executable, for llvm-symbolizer --obj=game.exe (the
// translated functions are named after their guest address). Reports and lets the exception go on.
LONG CALLBACK report_fatal(PEXCEPTION_POINTERS ep) {
  const DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_PRIV_INSTRUCTION &&
      code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_STACK_OVERFLOW)
    return EXCEPTION_CONTINUE_SEARCH;
  static volatile LONG shown;
  if (InterlockedIncrement(&shown) > 2) return EXCEPTION_CONTINUE_SEARCH;
  const uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
  CONTEXT ctx = *ep->ContextRecord;
  fprintf(stderr, "[x360port] exception 0x%08lX at game.exe+0x%llX; stack:", code,
          (unsigned long long)(ctx.Rip - base));
  for (int i = 0; i < 16 && ctx.Rip; ++i) {
    DWORD64 image = 0;
    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &image, nullptr);
    fprintf(stderr, " %s+0x%llX", image == base ? "game.exe" : "dll", (unsigned long long)(ctx.Rip - image));
    if (!fn) { ctx.Rip = *(DWORD64 *)ctx.Rsp; ctx.Rsp += 8; continue; }
    void *data = nullptr;
    DWORD64 frame = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, ctx.Rip, fn, &ctx, &data, &frame, nullptr);
  }
  fprintf(stderr, "\n");
  fflush(stderr);
  return EXCEPTION_CONTINUE_SEARCH;
}

const struct Install {
  Install() {
    AddVectoredExceptionHandler(1, low_guest_read);
    AddVectoredExceptionHandler(1, report_fatal);
  }
} s_install;

}  // namespace
#endif

// Untranslated code runs emulated instead of stopping the game.
//
// An indirect call to an address the translation holds no function for went to the runtime's
// rex::runtime::ResolveIndirectFunction, whose answer reports "Call to invalid or unregistered function" and stops.
// The game's import of that function is redirected here: a target in the title's code gets a trampoline that runs
// the PowerPC code on Unicorn (unicorn.dll next to the game) over the guest memory, in the 32-bit address mode the
// title's MSR has. Whenever the emulated code reaches an address with a translated function, emulation pauses and the
// native function runs with the registers handed over; emulation ends when it returns to the link register it was
// called with. The address is logged with the same words the runtime uses, in build/logs/x360port-fallback.log, so
// x360port learn declares it and the next build translates it natively.
//
// Unicorn does not give access to the vector registers: code that depends on vector state carried in from the
// translated caller is not right under emulation, and VMX128 instructions are not emulated at all (the emulation then
// stops and the runtime reports the address as before).
#if defined(_WIN32) && defined(X360PORT_FALLBACK)
#include <unicorn/unicorn.h>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>
#include <filesystem>
#include <mutex>
#include <string>

namespace {

struct {
  decltype(&uc_open) open;
  decltype(&uc_hook_add) hook_add;
  decltype(&uc_mem_map_ptr) mem_map_ptr;
  decltype(&uc_mem_unmap) mem_unmap;
  decltype(&uc_emu_start) emu_start;
  decltype(&uc_emu_stop) emu_stop;
  decltype(&uc_reg_write) reg_write;
  decltype(&uc_reg_read) reg_read;
  decltype(&uc_strerror) strerror_;
  decltype(&uc_ctl) ctl;
} uc;

uc_engine *g_uc;
std::mutex g_uc_lock;
uint32_t g_code_base, g_code_size, g_table;   // the function table: g_membase + g_table + (address - code base) * 2
uint8_t *g_membase;
using ResolveFn = PPCFunc *(*)(uint32_t);
ResolveFn g_resolve;

thread_local uint32_t t_native, t_entry;
thread_local uint32_t t_mapped_base[1024], t_mapped_size[1024];
thread_local unsigned t_mapped;

PPCFunc *translated(uint32_t address) {
  if (address - g_code_base >= g_code_size) return nullptr;
  return *reinterpret_cast<PPCFunc **>(g_membase + g_table + uint64_t(address - g_code_base) * 2);
}

void unmap_all() {
  while (t_mapped) {
    --t_mapped;
    uc.mem_unmap(g_uc, t_mapped_base[t_mapped], t_mapped_size[t_mapped]);
  }
}

bool on_unmapped(uc_engine *e, uc_mem_type, uint64_t address, int, int64_t, void *) {
  for (uint64_t span = 0x10000; span >= 0x1000; span >>= 4) {
    uint64_t base = address & ~(span - 1);
    if (base + span > 0x100000000ull) continue;
    uint8_t *host = g_membase + base;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(host, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) continue;
    if ((uint8_t *)mbi.BaseAddress > host || (uint8_t *)mbi.BaseAddress + mbi.RegionSize < host + span) continue;
    DWORD protect = mbi.Protect & 0xFFu;
    if ((mbi.Protect & PAGE_GUARD) || protect == PAGE_NOACCESS) continue;
    uint32_t perms = UC_PROT_READ | UC_PROT_EXEC;
    if (protect == PAGE_READWRITE || protect == PAGE_EXECUTE_READWRITE) perms |= UC_PROT_WRITE;
    if (t_mapped >= 1024) unmap_all();
    if (uc.mem_map_ptr(e, base, size_t(span), perms, host) == UC_ERR_OK) {
      t_mapped_base[t_mapped] = uint32_t(base);
      t_mapped_size[t_mapped++] = uint32_t(span);
      return true;
    }
  }
  return false;
}

void on_block(uc_engine *e, uint64_t address, uint32_t, void *) {
  if (uint32_t(address) != t_entry && translated(uint32_t(address))) {
    t_native = uint32_t(address);
    uc.emu_stop(e);
  }
}

PPCRegister PPCContext::*const kGpr[32] = {
    &PPCContext::r0,  &PPCContext::r1,  &PPCContext::r2,  &PPCContext::r3,  &PPCContext::r4,  &PPCContext::r5,
    &PPCContext::r6,  &PPCContext::r7,  &PPCContext::r8,  &PPCContext::r9,  &PPCContext::r10, &PPCContext::r11,
    &PPCContext::r12, &PPCContext::r13, &PPCContext::r14, &PPCContext::r15, &PPCContext::r16, &PPCContext::r17,
    &PPCContext::r18, &PPCContext::r19, &PPCContext::r20, &PPCContext::r21, &PPCContext::r22, &PPCContext::r23,
    &PPCContext::r24, &PPCContext::r25, &PPCContext::r26, &PPCContext::r27, &PPCContext::r28, &PPCContext::r29,
    &PPCContext::r30, &PPCContext::r31};
PPCRegister PPCContext::*const kFpr[32] = {
    &PPCContext::f0,  &PPCContext::f1,  &PPCContext::f2,  &PPCContext::f3,  &PPCContext::f4,  &PPCContext::f5,
    &PPCContext::f6,  &PPCContext::f7,  &PPCContext::f8,  &PPCContext::f9,  &PPCContext::f10, &PPCContext::f11,
    &PPCContext::f12, &PPCContext::f13, &PPCContext::f14, &PPCContext::f15, &PPCContext::f16, &PPCContext::f17,
    &PPCContext::f18, &PPCContext::f19, &PPCContext::f20, &PPCContext::f21, &PPCContext::f22, &PPCContext::f23,
    &PPCContext::f24, &PPCContext::f25, &PPCContext::f26, &PPCContext::f27, &PPCContext::f28, &PPCContext::f29,
    &PPCContext::f30, &PPCContext::f31};
PPCCRRegister PPCContext::*const kCr[8] = {&PPCContext::cr0, &PPCContext::cr1, &PPCContext::cr2, &PPCContext::cr3,
                                           &PPCContext::cr4, &PPCContext::cr5, &PPCContext::cr6, &PPCContext::cr7};

void to_uc(PPCContext &ctx, uint64_t pc) {
  for (int i = 0; i < 32; ++i) uc.reg_write(g_uc, UC_PPC_REG_0 + i, &(ctx.*kGpr[i]).u64);
  for (int i = 0; i < 32; ++i) uc.reg_write(g_uc, UC_PPC_REG_FPR0 + i, &(ctx.*kFpr[i]).u64);
  for (int i = 0; i < 8; ++i) {
    const PPCCRRegister &c = ctx.*kCr[i];
    uint32_t v = (c.lt << 3) | (c.gt << 2) | (c.eq << 1) | c.so;
    uc.reg_write(g_uc, UC_PPC_REG_CR0 + i, &v);
  }
  uint32_t xer = (uint32_t(ctx.xer.so) << 31) | (uint32_t(ctx.xer.ov) << 30) | (uint32_t(ctx.xer.ca) << 29);
  uint64_t lr = ctx.lr, ctr = ctx.ctr.u64, msr = ctx.msr;
  uc.reg_write(g_uc, UC_PPC_REG_XER, &xer);
  uc.reg_write(g_uc, UC_PPC_REG_LR, &lr);
  uc.reg_write(g_uc, UC_PPC_REG_CTR, &ctr);
  msr = (msr & ~uint64_t(0x30)) | (uint64_t(1) << 60);   // IR/DR off (real mode), HV on; SF stays off (32-bit)
  uc.reg_write(g_uc, UC_PPC_REG_MSR, &msr);
  uc.reg_write(g_uc, UC_PPC_REG_PC, &pc);
}

uint64_t from_uc(PPCContext &ctx) {
  for (int i = 0; i < 32; ++i) uc.reg_read(g_uc, UC_PPC_REG_0 + i, &(ctx.*kGpr[i]).u64);
  for (int i = 0; i < 32; ++i) uc.reg_read(g_uc, UC_PPC_REG_FPR0 + i, &(ctx.*kFpr[i]).u64);
  for (int i = 0; i < 8; ++i) {
    uint32_t v = 0;
    uc.reg_read(g_uc, UC_PPC_REG_CR0 + i, &v);
    PPCCRRegister &c = ctx.*kCr[i];
    c.lt = (v >> 3) & 1; c.gt = (v >> 2) & 1; c.eq = (v >> 1) & 1; c.so = v & 1;
  }
  uint32_t xer = 0;
  uint64_t lr = 0, ctr = 0, pc = 0;
  uc.reg_read(g_uc, UC_PPC_REG_XER, &xer);
  uc.reg_read(g_uc, UC_PPC_REG_LR, &lr);
  uc.reg_read(g_uc, UC_PPC_REG_CTR, &ctr);
  uc.reg_read(g_uc, UC_PPC_REG_PC, &pc);
  ctx.xer.so = (xer >> 31) & 1; ctx.xer.ov = (xer >> 30) & 1; ctx.xer.ca = (xer >> 29) & 1;
  ctx.lr = lr;
  ctx.ctr.u64 = ctr;
  return pc;
}

void record(uint32_t target) {
  static std::mutex m;
  static uint32_t seen[256];
  static unsigned n;
  std::lock_guard<std::mutex> guard(m);
  for (unsigned i = 0; i < n; ++i)
    if (seen[i] == target) return;
  if (n < 256) seen[n++] = target;
  std::error_code ec;
  std::filesystem::create_directories("logs", ec);
  if (FILE *f = std::fopen("logs/x360port-fallback.log", "a")) {
    std::fprintf(f, "Call to invalid or unregistered function at guest address 0x%08X (run emulated)\n", target);
    std::fclose(f);
  }
  if (n <= 32)
    std::fprintf(stderr, "[x360port] untranslated code at 0x%08X: running it emulated (recorded for the next build)\n",
                 target);
}

// 1 when the code at target ran and returned to the caller's link register, 0 when it could not be run.
bool run(PPCContext &ctx, uint8_t *, uint32_t target) {
  record(target);
  const uint64_t until = ctx.lr;
  uint64_t pc = target;
  std::unique_lock<std::mutex> lock(g_uc_lock);
  for (unsigned rounds = 0; rounds < 1000000u; ++rounds) {
    t_native = 0;
    t_entry = uint32_t(pc);
    to_uc(ctx, pc);
    if (std::getenv("X360PORT_FALLBACK_TRACE"))
      std::fprintf(stderr, "[x360port] run 0x%08llX until 0x%08llX r1=%llX r3=%llX r4=%llX r5=%llX\n",
                   (unsigned long long)pc, (unsigned long long)until, (unsigned long long)ctx.r1.u64,
                   (unsigned long long)ctx.r3.u64, (unsigned long long)ctx.r4.u64, (unsigned long long)ctx.r5.u64);
    uc_err err = uc.emu_start(g_uc, pc, until, 0, 0);
    unmap_all();
    pc = from_uc(ctx);
    if (err != UC_ERR_OK) {
      std::fprintf(stderr, "[x360port] emulation of 0x%08X stopped at 0x%08llX: %s\n", target,
                   (unsigned long long)pc, uc.strerror_(err));
      return false;
    }
    if (!t_native) return pc == until;
    // A translated function: native. It returns to the link register the emulated bl set (a tail branch leaves
    // the caller's, which ends the emulation).
    PPCFunc *native = translated(t_native);
    lock.unlock();
    native(ctx, g_membase);
    lock.lock();
    pc = ctx.lr;
    if (pc == until) return true;
  }
  return false;
}

void trampoline(PPCContext &ctx, uint8_t *base) {
  const uint32_t target = ctx.last_indirect_target;
  if (!run(ctx, base, target)) g_resolve(target)(ctx, base);   // the runtime's own report and stop
}

PPCFunc *resolve_hook(uint32_t target) {
  if (g_uc && target - g_code_base < g_code_size && (target & 3u) == 0) return trampoline;
  return g_resolve(target);
}

// Redirects game.exe's import of rex::runtime::ResolveIndirectFunction to resolve_hook.
bool hook_import() {
  auto *image = reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr));
  auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(image);
  auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(image + dos->e_lfanew);
  const auto &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  for (auto *imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(image + dir.VirtualAddress); imp->Name; ++imp) {
    auto *names = reinterpret_cast<IMAGE_THUNK_DATA *>(image + imp->OriginalFirstThunk);
    auto *slots = reinterpret_cast<IMAGE_THUNK_DATA *>(image + imp->FirstThunk);
    for (; names->u1.AddressOfData; ++names, ++slots) {
      if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
      auto *by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(image + names->u1.AddressOfData);
      if (std::strcmp(reinterpret_cast<const char *>(by_name->Name),
                      "?ResolveIndirectFunction@runtime@rex@@YAP6AXAEAUPPCContext@@PEAE@ZI@Z") != 0)
        continue;
      DWORD old;
      if (!VirtualProtect(&slots->u1.Function, sizeof(void *), PAGE_READWRITE, &old)) return false;
      g_resolve = reinterpret_cast<ResolveFn>(slots->u1.Function);
      slots->u1.Function = reinterpret_cast<ULONG_PTR>(&resolve_hook);
      VirtualProtect(&slots->u1.Function, sizeof(void *), old, &old);
      return true;
    }
  }
  return false;
}

}  // namespace

// Called by the generated app (game_app.h, OnPostLoadXexImage) once the title is loaded.
void x360port_fallback_init(uint8_t *membase, uint32_t image_base, uint32_t image_size, uint32_t code_base,
                            uint32_t code_size) {
  const char *off = std::getenv("X360PORT_FALLBACK");
  if (off && std::strcmp(off, "0") == 0) return;
  HMODULE dll = LoadLibraryA("unicorn.dll");
  if (!dll) return;
#define UC_LOAD(field, name) \
  if (!(*reinterpret_cast<FARPROC *>(&uc.field) = GetProcAddress(dll, name))) return
  UC_LOAD(open, "uc_open"); UC_LOAD(hook_add, "uc_hook_add"); UC_LOAD(mem_map_ptr, "uc_mem_map_ptr");
  UC_LOAD(mem_unmap, "uc_mem_unmap"); UC_LOAD(emu_start, "uc_emu_start"); UC_LOAD(emu_stop, "uc_emu_stop");
  UC_LOAD(reg_write, "uc_reg_write"); UC_LOAD(reg_read, "uc_reg_read"); UC_LOAD(strerror_, "uc_strerror");
  UC_LOAD(ctl, "uc_ctl");
#undef UC_LOAD
  uc_engine *e;
  if (uc.open(UC_ARCH_PPC, uc_mode(UC_MODE_PPC64 | UC_MODE_BIG_ENDIAN), &e) != UC_ERR_OK) return;
  // A 970 (no TLB to fill, unlike the default e5500), run in real mode with hypervisor rights and 32-bit
  // addresses: effective addresses are guest addresses, as the runtime treats them.
  uc.ctl(e, UC_CTL_WRITE(UC_CTL_CPU_MODEL, 1), int(UC_CPU_PPC64_970FX_V3_1));
  uc_hook h1, h2;
  uc.hook_add(e, &h1, UC_HOOK_MEM_UNMAPPED, reinterpret_cast<void *>(&on_unmapped), nullptr, 1, 0);
  uc.hook_add(e, &h2, UC_HOOK_BLOCK, reinterpret_cast<void *>(&on_block), nullptr, 1, 0);
  g_membase = membase;
  g_code_base = code_base;
  g_code_size = code_size;
  g_table = image_base + image_size;
  g_uc = e;
  if (!hook_import()) g_uc = nullptr;
}

#else
#include <cstdint>
void x360port_fallback_init(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t) {}
#endif
