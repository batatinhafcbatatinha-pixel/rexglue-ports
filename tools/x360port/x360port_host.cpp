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
