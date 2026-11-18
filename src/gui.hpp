// Do A Barrel Roll: the flight instruments, kept apart from the game so tests/flight_test.cpp can check them. They
// are drawn by the FH2 Mod Platform's HUD canvas (fh2mod_hud.h, platform 2.9.1): the mod gives the platform this
// XAML once, and then only numbers and texts.
//
// The picture is for the HUD page's 1920 x 1080. In the middle is an artificial horizon: a ladder of pitch lines
// that turns against the car's bank and slides with its pitch, behind a fixed car symbol and a bank scale, in a
// window that clips it. Left of it are the throttle and a speed tape, right of it a height tape with the climb
// under it, above it a compass tape. Each tape is one long strip with every tick and label on it, slid behind its
// window by one number, so nothing but six numbers changes from frame to frame.
//
// Only what the platform's docs list as seen working in the game's HUD page is used: Grids placed by margins, a
// Canvas for what overflows, the game's text blocks and materials, stroked Paths of M and L, and bound transforms.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "flight.hpp"

namespace gui {

// The canvas's numbers (N0 on) and texts (T0 on).
enum { kRoll, kPitch, kSpeed, kHeight, kHeading, kThrottle, kNumbers };
enum { kSpeedText, kHeightText, kClimbText, kHeadingText, kTexts };

constexpr float kPitchPx   = 8;      // the ladder: px a degree of pitch
constexpr float kSpeedPx   = 2;      // the speed tape: px a km/h
constexpr float kSpeedMax  = 700;    // km/h at its top
constexpr float kHeightPx  = 0.5f;   // the height tape: px a metre
constexpr float kHeightMin = -200, kHeightMax = 3000;   // metres at its ends
constexpr float kHeadingPx = 4;      // the compass tape: px a degree

// What the instruments read: bank (right wing down) and pitch (nose up) in degrees, km/h, metres, the compass
// heading (0 to 360) and the throttle (0 to 1).
struct Reading {
    float bank, pitch, kmh, height, heading, throttle;
};

inline Reading reading(const flight::Body& b, float height, float throttle) {
    const float deg = 57.29578f;
    float h         = std::atan2(b.forward.x, b.forward.z) * deg;   // as flight::heading: +z is taken as north
    if (h < 0) h += 360;
    const float fy = b.forward.y < -1 ? -1 : b.forward.y > 1 ? 1 : b.forward.y;
    return {std::atan2(-b.right.y, b.up.y) * deg, std::asin(fy) * deg, flight::len(b.vel) * 3.6f, height, h, throttle};
}

// The six numbers for a reading, each rounded to what can be seen, so one that hasn't visibly changed isn't sent
// to the page again.
inline void numbers(const Reading& r, float out[kNumbers]) {
    const auto to = [](float v, float step) { return std::round(v / step) * step + 0.0f; };   // + 0: never -0, which is another number to the page
    out[kRoll]     = to(-r.bank, 0.1f);            // the horizon turns against the bank
    out[kPitch]    = to(r.pitch * kPitchPx, 0.5f); // and slides down as the nose comes up
    out[kSpeed]    = to(std::clamp(r.kmh, 0.0f, kSpeedMax) * kSpeedPx, 0.5f);
    out[kHeight]   = to((std::clamp(r.height, kHeightMin, kHeightMax) - kHeightMin) * kHeightPx, 0.5f);
    out[kHeading]  = to(-r.heading * kHeadingPx, 0.5f);
    out[kThrottle] = to(flight::clamp01(r.throttle), 0.02f);
}

namespace detail {

inline std::string num(float v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.6g", static_cast<double>(v));
    return b;
}

constexpr const char* kLine   = "#FFF5F5F5";   // the HUD's white
constexpr const char* kOrange = "#FFFE5723";   // the game's orange: what's fixed to the car
constexpr const char* kGlass  = "#66101418";
constexpr const char* kSmall  = "A_Style_FZTextBlock_HudSmall";
constexpr const char* kMedium = "A_Style_FZTextBlock_HudMedium";

// A box at (x, y) of its parent Grid.
inline std::string at(float x, float y, float w, float h) {
    return " HorizontalAlignment=\"Left\" VerticalAlignment=\"Top\" Margin=\"" + num(x) + "," + num(y) + ",0,0\" Width=\"" + num(w) +
           "\" Height=\"" + num(h) + "\"";
}

// A filled box.
inline std::string box(float x, float y, float w, float h, const char* colour, const char* preset = "A_FZMaterialPresetRoundtangle_HudItem") {
    return std::string("<local:FZMaterial Preset=\"{DynamicResource ") + preset + "}\" Color=\"" + colour + "\"" + at(x, y, w, h) + " />\n";
}

// A text in a box of its own, so the game only has to centre it there. `text` is the Text attribute's value.
inline std::string label(float x, float y, float w, float h, const std::string& text, const char* align, const char* style = kSmall) {
    return "<Grid" + at(x, y, w, h) + "><local:FZTextBlock Style=\"{StaticResource " + style + "}\" Text=\"" + text +
           "\" Color=\"" + kLine + "\" HorizontalAlignment=\"" + align + "\" VerticalAlignment=\"Center\" /></Grid>\n";
}

// Straight lines as one stroked Path, placed so its box is exactly the lines' bounds: the picture is then the same
// whether the game stretches a Path to its box or draws it as given.
struct Lines {
    struct Seg {
        float x1, y1, x2, y2;
    };
    std::vector<Seg> segs;
    void add(float x1, float y1, float x2, float y2) { segs.push_back({x1, y1, x2, y2}); }
    std::string path(const char* colour, float thickness) const {
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (const Seg& s : segs) {
            x0 = std::min({x0, s.x1, s.x2}), y0 = std::min({y0, s.y1, s.y2});
            x1 = std::max({x1, s.x1, s.x2}), y1 = std::max({y1, s.y1, s.y2});
        }
        std::string d;
        for (const Seg& s : segs)
            d += (d.empty() ? "M" : " M") + num(s.x1 - x0) + "," + num(s.y1 - y0) + " L" + num(s.x2 - x0) + "," + num(s.y2 - y0);
        return "<Path Data=\"" + d + "\" Stroke=\"" + colour + "\" StrokeThickness=\"" + num(thickness) + "\"" + at(x0, y0, x1 - x0, y1 - y0) + " />\n";
    }
};

// A window at (x, y), with dark glass behind it if `glass`, and in it a strip of `sw` x `sh` placed at (left, top)
// of the window, moved by `transform`. Whatever of the strip is outside the window is clipped.
inline std::string window_open(float x, float y, float w, float h, float left, float top, float sw, float sh, const std::string& transform,
                               bool glass = true, const char* origin = nullptr) {
    return "<Grid" + at(x, y, w, h) + " ClipToBounds=\"True\">\n" +
           (glass ? box(0, 0, w, h, kGlass, "A_FZMaterialPresetGlassRoundtangle_HudItem") : std::string()) + "<Canvas>\n" +
           "<Grid Canvas.Left=\"" + num(left) + "\" Canvas.Top=\"" + num(top) + "\" Width=\"" + num(sw) + "\" Height=\"" + num(sh) + "\"" +
           (origin ? std::string(" RenderTransformOrigin=\"") + origin + "\"" : std::string()) + ">\n<Grid.RenderTransform>\n" + transform +
           "</Grid.RenderTransform>\n";
}
constexpr const char* kStripClose = "</Grid>\n</Canvas>\n";   // then what's fixed in the window, then </Grid>

// The box a tape's or the compass's number sits in: darker glass with an orange edge.
inline std::string readout(float x, float y, float w, float h, int text) {
    return "<Grid" + at(x, y, w, h) + ">\n<local:FZMaterial Preset=\"{DynamicResource A_FZMaterialPresetGlassRoundtangle_HudItem}\" Color=\"#F0101418\" />\n" +
           "<Border BorderThickness=\"2\" BorderBrush=\"" + kOrange + "\" />\n<local:FZTextBlock Style=\"{StaticResource " + kMedium +
           "}\" Text=\"{Binding T" + std::to_string(text) + "}\" Color=\"" + kLine + "\" HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\" />\n</Grid>\n";
}

} // namespace detail

// The canvas's XAML: kNumbers numbers, kTexts texts, no switches.
inline std::string xaml() {
    using namespace detail;
    const auto bind = [](int n) { return "\"{Binding N" + std::to_string(n) + "}\""; };
    std::string x = "<Grid>\n";

    // ---- the horizon: a 520 x 420 window round the middle of the screen, (960, 500) -------------------------------
    {
        // the ladder: 1100 x 1440, its middle (the horizon, at no pitch) on the window's; 90 degrees up is its top
        const float W = 1100, H = 180 * kPitchPx, cx = W / 2, cy = H / 2;
        x += window_open(700, 290, 520, 420, (520 - W) / 2, (420 - H) / 2, W, H,
                         "<TransformGroup>\n<TranslateTransform Y=" + bind(kPitch) + " />\n<RotateTransform Angle=" + bind(kRoll) +
                             " />\n</TransformGroup>\n",
                         false, "0.5,0.5");   // no glass: the car is behind this one
        x += box(0, cy - 1.5f, W, 3, kLine);   // the horizon
        Lines rungs;
        for (int d = -90; d <= 90; d += 5) {
            if (d == 0) continue;
            const float y = cy - static_cast<float>(d) * kPitchPx;
            const bool ten = d % 10 == 0;
            rungs.add(cx - (ten ? 130.0f : 60.0f), y, cx - (ten ? 40.0f : 30.0f), y);
            rungs.add(cx + (ten ? 40.0f : 30.0f), y, cx + (ten ? 130.0f : 60.0f), y);
            if (ten) x += label(cx + 138, y - 14, 44, 28, std::to_string(d < 0 ? -d : d), "Left");
        }
        x += rungs.path(kLine, 2);
        x += kStripClose;
        // fixed to the car: its symbol on the window's middle, and the bank scale over it (0, 10, 20, 30, 45 and 60
        // degrees each way)
        x += box(150, 208, 70, 4, kOrange) + box(300, 208, 70, 4, kOrange) + box(256, 206, 8, 8, kOrange);
        Lines scale;
        for (const int a : {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60}) {
            const float s = std::sin(static_cast<float>(a) / 57.29578f), c = std::cos(static_cast<float>(a) / 57.29578f);
            const float outer = a % 30 == 0 ? 206.0f : 198.0f;   // (not "far": windows.h takes that word)
            scale.add(260 + 188 * s, 210 - 188 * c, 260 + outer * s, 210 - outer * c);
        }
        x += scale.path(kLine, 2);
        // the bank pointer, which turns with the horizon under the scale
        x += "<Grid" + at(0, 0, 520, 420) + " RenderTransformOrigin=\"0.5,0.5\">\n<Grid.RenderTransform>\n<RotateTransform Angle=" + bind(kRoll) +
             " />\n</Grid.RenderTransform>\n";
        Lines pointer;
        pointer.add(251, 40, 260, 24);
        pointer.add(260, 24, 269, 40);
        x += pointer.path(kOrange, 3) + "</Grid>\n</Grid>\n";
    }

    // ---- the speed tape, left of the horizon: 0 km/h at the strip's foot, its top kSpeedMax ------------------------
    {
        const float H = kSpeedMax * kSpeedPx;
        x += label(560, 268, 110, 28, "KM/H", "Center");
        x += window_open(560, 300, 110, 400, 0, 200 - H, 110, H, "<TranslateTransform Y=" + bind(kSpeed) + " />\n");
        Lines ticks;
        for (int s = 0; s <= static_cast<int>(kSpeedMax); s += 10) {
            const float y = H - static_cast<float>(s) * kSpeedPx;
            ticks.add(s % 50 == 0 ? 78.0f : 94.0f, y, 110, y);
            if (s % 50 == 0) x += label(4, y - 14, 68, 28, std::to_string(s), "Right");
        }
        x += ticks.path(kLine, 2) + kStripClose + readout(0, 180, 110, 40, kSpeedText) + "</Grid>\n";
    }

    // ---- the throttle, left of the speed tape: a bar that fills from its foot --------------------------------------
    x += box(536, 300, 12, 400, kGlass, "A_FZMaterialPresetGlassRoundtangle_HudItem");
    x += std::string("<local:FZMaterial Preset=\"{DynamicResource A_FZMaterialPresetRoundtangle_HudItem}\" Color=\"") + kLine + "\"" +
         at(536, 300, 12, 400) + " RenderTransformOrigin=\"0.5,1\">\n<local:FZMaterial.RenderTransform>\n<ScaleTransform ScaleX=\"1\" ScaleY=" +
         bind(kThrottle) + " />\n</local:FZMaterial.RenderTransform>\n</local:FZMaterial>\n";
    x += label(506, 704, 72, 28, "THR", "Center");

    // ---- the height tape, right of the horizon: kHeightMin at the strip's foot ------------------------------------
    {
        const float H = (kHeightMax - kHeightMin) * kHeightPx;
        x += label(1250, 268, 120, 28, "ALT M", "Center");
        x += window_open(1250, 300, 120, 400, 0, 200 - H, 120, H, "<TranslateTransform Y=" + bind(kHeight) + " />\n");
        Lines ticks;
        for (int a = static_cast<int>(kHeightMin); a <= static_cast<int>(kHeightMax); a += 20) {
            const float y = H - (static_cast<float>(a) - kHeightMin) * kHeightPx;
            ticks.add(0, y, a % 100 == 0 ? 32.0f : 16.0f, y);
            if (a % 100 == 0) x += label(40, y - 14, 76, 28, std::to_string(a), "Left");
        }
        x += ticks.path(kLine, 2) + kStripClose + readout(0, 180, 120, 40, kHeightText) + "</Grid>\n";
        x += label(1250, 704, 120, 28, "{Binding T" + std::to_string(kClimbText) + "}", "Center");
    }

    // ---- the compass tape, above the horizon: 90 degrees more at each end, so it never runs out --------------------
    {
        const float W = 540 * kHeadingPx;
        x += window_open(660, 214, 600, 48, 300 - 90 * kHeadingPx, 0, W, 48, "<TranslateTransform X=" + bind(kHeading) + " />\n");
        Lines ticks;
        for (int d = -90; d <= 450; d += 5) {
            const float px = static_cast<float>(d + 90) * kHeadingPx;
            ticks.add(px, 0, px, d % 10 == 0 ? 14.0f : 8.0f);
            if (d % 30 != 0) continue;
            const int h = (d + 360) % 360;
            char two[8];
            std::snprintf(two, sizeof two, "%02d", h / 10);
            x += label(px - 24, 18, 48, 28, h == 0 ? "N" : h == 90 ? "E" : h == 180 ? "S" : h == 270 ? "W" : two, "Center");
        }
        x += ticks.path(kLine, 2) + kStripClose + box(298, 0, 4, 18, kOrange) + "</Grid>\n";
        x += readout(915, 172, 90, 38, kHeadingText);
    }

    return x + "</Grid>\n";
}

} // namespace gui
