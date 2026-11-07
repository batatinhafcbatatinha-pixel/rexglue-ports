// Madagascar 3 look for the PC settings, after the game's own Options menu: a small blue title
// plate with a gold rim and beads, a blue wood panel framed in gold with ornaments at the
// corners, white PT Sans Bold with a dark drop shadow, the selection as a gold plate behind the
// line, and the coloured Xbox button discs at the bottom left. PT Sans (ParaType, free licence,
// the game's own face) ships with the port in fonts\ next to the executable.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <cwctype>

#include "pc_font.h"
#include "pc_settings.h"

#include <rex/logging.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <vector>

namespace {

constexpr int kEm = 48;
constexpr uint32_t kWhite = 0xFFFFFFFFu, kShadow = 0xC0101030u, kGold = 0xFFE0B656u;
constexpr uint32_t kGoldDark = 0xFF8A6420u, kMuted = 0xFFB4BBDCu;

float Scale(int em) { return float(em) / float(kEm); }

void LoadTheme(pcs::Theme& t) {
  try {
    auto dir = pcfont::ExeDir() / "fonts";
    pcfont::LoadTtfFile(dir / "PT_Sans-Web-Bold.ttf", kEm, t.font);
    pcfont::LoadTtfFile(dir / "PT_Sans-Web-Regular.ttf", kEm, t.extra_font);
  } catch (const std::exception& e) {
    t.font = {};
    REXLOG_WARN("[pc] Madagascar menu font: {} (the menu keeps its own look)", e.what());
  }
}

void Disc(pcs::Canvas& c, int cx, int cy, int r, uint32_t argb) {
  int xy[32];
  for (int k = 0; k < 16; ++k) {
    xy[2 * k] = cx + int(std::lround(r * std::cos(k * 3.14159265 / 8)));
    xy[2 * k + 1] = cy + int(std::lround(r * std::sin(k * 3.14159265 / 8)));
  }
  c.Polygon(xy, 16, argb);
}

// White text with the game's soft dark drop shadow.
int Shadowed(pcs::Canvas& c, const pcs::ThemeFont& f, const std::wstring& s, int x, int y, int w, int em,
             uint32_t argb, int align) {
  c.Text(f, s, x + 2, y + 2, w, Scale(em), kShadow, align, false);
  return c.Text(f, s, x, y, w, Scale(em), argb, align, false);
}

// The blue wood of the game's panels: a base blue with darker and lighter grain streaks.
void Wood(pcs::Canvas& c, int x, int y, int w, int h) {
  c.Fill(x, y, w, h, 0xFF3446A4u);
  static const int grain[] = {3, 11, 17, 26, 31, 40, 47, 55, 62, 69, 77, 86, 91};
  for (int band = 0; band < h; band += 96)
    for (int g : grain) {
      if (band + g >= h) break;
      c.Fill(x, y + band + g, w, 1, (g % 2) ? 0x30101A60u : 0x205A6CD0u);
    }
}

// The gold frame: a bright rim, a dark inner line and beads with small scrolls at the corners.
void Frame(pcs::Canvas& c, int x, int y, int w, int h, bool ornate) {
  c.Fill(x - 5, y - 5, w + 10, 5, kGold);
  c.Fill(x - 5, y + h, w + 10, 5, kGold);
  c.Fill(x - 5, y, 5, h, kGold);
  c.Fill(x + w, y, 5, h, kGold);
  c.Fill(x, y, w, 1, kGoldDark);
  c.Fill(x, y + h - 1, w, 1, kGoldDark);
  c.Fill(x, y, 1, h, kGoldDark);
  c.Fill(x + w - 1, y, 1, h, kGoldDark);
  const int cx[4] = {x - 2, x + w + 2, x - 2, x + w + 2}, cy[4] = {y - 2, y - 2, y + h + 2, y + h + 2};
  for (int k = 0; k < 4; ++k) {
    if (ornate) {
      // A small scroll: three beads curling out of the corner.
      int dx = k % 2 ? 1 : -1, dy = k < 2 ? -1 : 1;
      Disc(c, cx[k], cy[k], 9, kGold);
      Disc(c, cx[k] + dx * 12, cy[k] - dy * 4, 5, kGold);
      Disc(c, cx[k] - dx * 4, cy[k] + dy * 12, 5, kGold);
      Disc(c, cx[k], cy[k], 4, kGoldDark);
    } else {
      Disc(c, cx[k], cy[k], 5, kGold);
    }
  }
}

struct Line {
  const pcs::MenuRow* row;
  const std::wstring* heading;
};

void Draw(const pcs::Theme& t, pcs::Canvas& c, pcs::MenuView& v) {
  const pcs::ThemeFont& bold = t.font;
  const pcs::ThemeFont& regular = t.extra_font.glyphs.empty() ? bold : t.extra_font;
  auto text_y = [&](int em, int top, int height) { return top + (height - int(bold.line_height * Scale(em))) / 2; };

  // The game's menus float over the scene; a light dim keeps the text readable.
  c.Fill(0, 0, 1280, 720, 0x50000000u);

  // Title plate, as "Options".
  int tw = c.Measure(bold, v.title, Scale(32), false);
  int px = 640 - tw / 2 - 18;
  c.Fill(px, 50, tw + 36, 58, 0xFF3446A4u);
  Frame(c, px, 50, tw + 36, 58, false);
  Shadowed(c, bold, v.title, px, text_y(32, 50, 58), tw + 36, 32, kWhite, 2);

  std::vector<Line> lines;
  int selected_line = 0;
  for (const pcs::MenuRow& r : v.rows) {
    if (!r.section.empty()) lines.push_back({nullptr, &r.section});
    if (r.selected) selected_line = int(lines.size());
    lines.push_back({&r, nullptr});
  }
  constexpr int kTop = 150, kRow = 40, kVisible = 10;
  static int first = 0;
  if (selected_line < first + 1) first = std::max(0, selected_line - 1);
  if (selected_line > first + kVisible - 2) first = selected_line - kVisible + 2;
  first = std::clamp(first, 0, std::max(0, int(lines.size()) - kVisible));
  const int shown = std::min(kVisible, int(lines.size()) - first);
  const int x0 = 250, x1 = 1030;
  Wood(c, x0, kTop - 14, x1 - x0, shown * kRow + 28);
  Frame(c, x0, kTop - 14, x1 - x0, shown * kRow + 28, true);
  for (int i = 0; i < shown; ++i) {
    const Line& line = lines[first + i];
    const int y = kTop + i * kRow;
    if (line.heading) {
      Shadowed(c, bold, *line.heading, x0 + 30, text_y(22, y, kRow), 400, 22, kGold, 0);
      continue;
    }
    const pcs::MenuRow& r = *line.row;
    std::wstring value = r.value + (r.restart ? L" *" : L"");
    uint32_t ink = r.greyed ? kMuted : kWhite;
    if (r.selected) {
      // The game's gold plate behind the line: a lighter top half over a deeper gold.
      c.Fill(x0 + 20, y + 3, x1 - x0 - 40, kRow - 6, 0xFFC88A26u);
      c.Fill(x0 + 20, y + 3, x1 - x0 - 40, (kRow - 6) / 2, 0xFFE5AE3Eu);
      c.Fill(x0 + 20, y + kRow - 4, x1 - x0 - 40, 1, kGoldDark);
    }
    Shadowed(c, bold, r.label, x0 + 30, text_y(28, y, kRow), 430, 28, ink, 0);
    if (!value.empty()) {
      int right = x1 - 34;
      if (r.selected) {
        int vw = c.Measure(bold, value, Scale(26), false);
        const int ar[] = {right - 10, y + 12, right - 10, y + kRow - 12, right, y + kRow / 2};
        c.Polygon(ar, 3, kWhite);
        int lx = right - 30 - vw - 18;
        const int al[] = {lx + 10, y + 12, lx + 10, y + kRow - 12, lx, y + kRow / 2};
        c.Polygon(al, 3, kWhite);
        v.hits.push_back({r.id, lx - 6, y, lx + 20, y + kRow, true});
        right -= 22;
      }
      Shadowed(c, bold, value, x0 + 470, text_y(26, y, kRow), right - x0 - 470, 26, ink, 1);
    }
    v.hits.push_back({r.id, x0, y, x1, y + kRow, false});
  }
  if (first > 0) {
    const int up[] = {x1 - 40, kTop - 2, x1 - 22, kTop - 2, x1 - 31, kTop - 12};
    c.Polygon(up, 3, kGold);
  }
  if (first + shown < int(lines.size())) {
    const int b = kTop + shown * kRow;
    const int down[] = {x1 - 40, b + 2, x1 - 22, b + 2, x1 - 31, b + 12};
    c.Polygon(down, 3, kGold);
  }

  // The selected line's explanation under the panel, then the restart note.
  const int under = kTop + shown * kRow + 30;
  if (!v.help.empty()) Shadowed(c, regular, v.help, x0, under, x1 - x0, 22, kWhite, 2);
  if (!v.restart_note.empty()) Shadowed(c, bold, v.restart_note, x0, under + 30, x1 - x0, 20, kGold, 2);

  // Bottom left, as the game: "(B) Back (A) Select" with coloured discs.
  int hx = 140;
  auto hint = [&](wchar_t letter, uint32_t colour, const std::wstring& caption, int id) {
    Disc(c, hx + 16, 650, 17, 0xFFF0F0F0u);
    Disc(c, hx + 16, 650, 15, colour);
    Shadowed(c, bold, std::wstring(1, letter), hx, text_y(24, 634, 32), 32, 24, kWhite, 2);
    int w = Shadowed(c, bold, caption, hx + 40, text_y(30, 632, 36), 300, 30, kWhite, 0);
    v.hits.push_back({id, hx, 630, hx + 40 + w, 670, false});
    hx += 40 + w + 24;
  };
  // The game writes its prompts in sentence case ("Back", "Select").
  auto sentence = [](std::wstring s) {
    for (size_t i = 1; i < s.size(); ++i) s[i] = std::towlower(s[i]);
    return s;
  };
  hint(L'B', 0xFFD83A3Au, sentence(v.back), -2);
  hint(L'A', 0xFF3FAE3Fu, sentence(v.select), -1);
  Shadowed(c, regular, v.keys, hx + 10, text_y(19, 634, 32), 1220 - hx, 19, 0xFFE4E8F4u, 0);
}

}  // namespace

const pcs::Theme* PcMadagascarTheme() {
  static pcs::Theme theme = [] {
    pcs::Theme t;
    t.load = LoadTheme;
    t.draw = Draw;
    return t;
  }();
  return &theme;
}
