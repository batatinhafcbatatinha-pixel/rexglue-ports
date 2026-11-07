// Debug-only guest memory reader. DORITOS_PEEK=<dir>: every 0.5 s, if <dir>/peek.txt exists, each line
//   LEN ADDR [+OFF ...]      (v = ADDR; for each +OFF: v = load32(v + OFF); dump LEN bytes at v)
// is answered in <dir>/peek_out.txt (appended) and peek.txt is deleted.
#include <windows.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

namespace doritos {

static bool Readable(const void* p) {
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(p, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) return false;
  DWORD prot = mbi.Protect & 0xFF;
  return prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_EXECUTE_READ ||
         prot == PAGE_EXECUTE_READWRITE || prot == PAGE_WRITECOPY;
}

static uint32_t Load32(uint8_t* mem, uint32_t a) {
  if (!Readable(mem + a)) return 0xDEADDEAD;
  uint32_t v;
  std::memcpy(&v, mem + a, 4);
  return _byteswap_ulong(v);
}

void StartPeekThread() {
  const char* dir = std::getenv("DORITOS_PEEK");
  if (!dir || !*dir) return;
  std::thread([d = std::filesystem::path(dir)] {
    for (;;) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      auto req = d / "peek.txt";
      if (!std::filesystem::exists(req)) continue;
      std::ifstream in(req);
      std::ofstream out(d / "peek_out.txt", std::ios::app);
      REXLOG_INFO("peek: request");
      auto* ks = rex::system::kernel_state();
      REXLOG_INFO("peek: kernel_state {}", (void*)ks);
      uint8_t* mem = ks ? ks->memory()->virtual_membase() : nullptr;
      std::string line;
      while (mem && std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        REXLOG_INFO("peek: {}", line);
        std::istringstream ss(line);
        std::string tok;
        if (line[0] == 'f') {  // f VALUE START END: aligned big-endian u32 occurrences (first 64)
          std::string a, b, c;
          ss >> tok >> a >> b >> c;
          uint32_t val = std::stoul(a, nullptr, 0), lo = std::stoul(b, nullptr, 0), hi = std::stoul(c, nullptr, 0);
          out << "== " << line << std::endl;
          int found = 0;
          for (uint32_t p = lo; p < hi && found < 64; p += 0x1000) {
            if (!Readable(mem + p)) continue;
            const uint32_t want = _byteswap_ulong(val);
            const uint32_t* page = reinterpret_cast<const uint32_t*>(mem + p);
            for (uint32_t k = 0; k < 0x400; k++)
              if (page[k] == want) {
                uint32_t q = p + 4 * k;
                char buf[16];
                std::snprintf(buf, sizeof buf, "%08X", q);
                out << "  " << buf << std::endl;
                if (++found >= 64) break;
              }
          }
          continue;
        }
        ss >> tok;
        uint32_t len = std::stoul(tok, nullptr, 0);
        ss >> tok;
        uint32_t v = std::stoul(tok, nullptr, 0);
        bool ok = true;
        while (ss >> tok) {
          uint32_t off = std::stoul(tok.substr(tok[0] == '+' ? 1 : 0), nullptr, 0);
          if (v + off < 0x10000 || v + off >= 0xF0000000u) { ok = false; break; }
          v = Load32(mem, v + off);
        }
        out << "== " << line << " -> " << std::hex << v << std::dec << "\n";
        if (!ok || v < 0x10000 || v >= 0xF0000000u) { out << "  (bad pointer)\n"; continue; }
        for (uint32_t i = 0; i < len; i += 16) {
          char buf[96];
          int n = std::snprintf(buf, sizeof buf, "%08X:", v + i);
          for (uint32_t j = 0; j < 16 && i + j < len; j += 4)
            n += std::snprintf(buf + n, sizeof buf - n, " %08X", Load32(mem, v + i + j));
          out << buf << "\n";
        }
      }
      in.close();
      out.close();
      std::error_code ec;
      std::filesystem::remove(req, ec);
      REXLOG_INFO("peek: done");
    }
  }).detach();
}

}  // namespace doritos
