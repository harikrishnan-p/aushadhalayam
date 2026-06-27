#pragma once
// =============================================================================
// AppColors.h  —  Fixed "Blue" colour palette + sizing constants
//
// Mirrors the React CSS custom properties so both builds share the same
// visual language.  Only one theme is compiled into the legacy binary.
// =============================================================================

#include <wx/colour.h>

namespace clr {

// ── Brand ────────────────────────────────────────────────────────────────────
inline wxColour Brand()      { return wxColour(37,  99, 235); }   // #2563eb
inline wxColour BrandDark()  { return wxColour(29,  78, 216); }   // #1d4ed8
inline wxColour BrandLight() { return wxColour(219, 234, 254); }  // #dbeafe
inline wxColour BrandMuted() { return wxColour(239, 246, 255); }  // #eff6ff

// ── Backgrounds ──────────────────────────────────────────────────────────────
inline wxColour Bg()         { return wxColour(248, 250, 252); }  // #f8fafc
inline wxColour Surface()    { return wxColour(255, 255, 255); }  // #ffffff
inline wxColour SurfaceAlt() { return wxColour(241, 245, 249); }  // #f1f5f9
inline wxColour Border()     { return wxColour(226, 232, 240); }  // #e2e8f0

// ── Text ─────────────────────────────────────────────────────────────────────
inline wxColour Text()       { return wxColour( 15,  23,  42); }  // #0f172a
inline wxColour Text2()      { return wxColour( 71,  85, 105); }  // #475569
inline wxColour Text3()      { return wxColour(148, 163, 184); }  // #94a3b8

// ── Semantic ─────────────────────────────────────────────────────────────────
inline wxColour Success()    { return wxColour( 22, 163,  74); }  // #16a34a
inline wxColour SuccessBg()  { return wxColour(220, 252, 231); }  // #dcfce7
inline wxColour Warning()    { return wxColour(217, 119,   6); }  // #d97706
inline wxColour WarningBg()  { return wxColour(254, 243, 199); }  // #fef3c7
inline wxColour Danger()     { return wxColour(220,  38,  38); }  // #dc2626
inline wxColour DangerBg()   { return wxColour(254, 226, 226); }  // #fee2e2

// ── Sidebar ───────────────────────────────────────────────────────────────────
inline wxColour SidebarBg()     { return wxColour(30,  41,  59); }  // #1e293b
inline wxColour SidebarText()   { return wxColour(148, 163, 184); } // #94a3b8
inline wxColour SidebarActBg()  { return wxColour(37,  99, 235); }  // #2563eb
inline wxColour SidebarActTxt() { return wxColour(255, 255, 255); } // #fff

// ── Schedule pills (background, foreground) ───────────────────────────────────
// OTC: green, H: amber, H1: orange, X: red
inline wxColour SchedOtcBg()  { return SuccessBg(); }
inline wxColour SchedOtcFg()  { return Success(); }
inline wxColour SchedHBg()    { return WarningBg(); }
inline wxColour SchedHFg()    { return Warning(); }
inline wxColour SchedH1Bg()   { return wxColour(253, 232, 216); }
inline wxColour SchedH1Fg()   { return wxColour(180,  83,   9); }
inline wxColour SchedXBg()    { return DangerBg(); }
inline wxColour SchedXFg()    { return Danger(); }

} // namespace clr

namespace sz {
// ── Sizing to match React ────────────────────────────────────────────────────
constexpr int SidebarW    = 220;  // sidebar width px
constexpr int TopbarH     = 52;   // topbar height px
constexpr int BtnH        = 28;   // standard button height (7px padding)
constexpr int BtnHLg      = 36;   // large button height (10px padding)
constexpr int InputH      = 28;   // text input height
constexpr int RowGap      = 8;    // gap between sibling controls
constexpr int CardPad     = 12;   // card inner padding
constexpr int SectionGap  = 14;   // gap between sections
constexpr int FontPt      = 10;   // 10pt ≈ 13px @ 96dpi
} // namespace sz
