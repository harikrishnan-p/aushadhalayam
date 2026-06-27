// =============================================================================
// SidebarPanel.cpp
// =============================================================================

#include "SidebarPanel.h"
#include "AppColors.h"
#include <wx/dcbuffer.h>

wxDEFINE_EVENT(wxEVT_SIDEBAR_NAV, wxCommandEvent);

wxBEGIN_EVENT_TABLE(SidebarPanel, wxPanel)
    EVT_PAINT       (SidebarPanel::OnPaint)
    EVT_MOTION      (SidebarPanel::OnMouseMove)
    EVT_LEAVE_WINDOW(SidebarPanel::OnMouseLeave)
    EVT_LEFT_DOWN   (SidebarPanel::OnLeftDown)
wxEND_EVENT_TABLE()

SidebarPanel::SidebarPanel(wxWindow* parent,
                           const std::vector<NavEntry>& entries,
                           int initial_page)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(sz::SidebarW, -1))
    , m_entries(entries)
    , m_active(initial_page)
    , m_hover(-1)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(sz::SidebarW, -1));
    SetMaxSize(wxSize(sz::SidebarW, -1));
}

void SidebarPanel::SetActivePage(int page_id) {
    m_active = page_id;
    Refresh();
}

// ── Geometry ──────────────────────────────────────────────────────────────────

int SidebarPanel::ItemY(int idx) const {
    int y = kLogoH + 8;
    for (int i = 0; i < idx; ++i) {
        if (!m_entries[i].section.empty()) y += kSecH;
        y += kItemH + 2;
    }
    if (!m_entries[idx].section.empty()) y += kSecH;
    return y;
}

int SidebarPanel::HitTest(int y) const {
    for (int i = 0; i < (int)m_entries.size(); ++i) {
        int top = ItemY(i);
        if (y >= top && y < top + kItemH) return i;
    }
    return -1;
}

// ── Icon drawing ──────────────────────────────────────────────────────────────
// Each icon is drawn centred on (cx, cy) within a kIconSz×kIconSz box.
// All shapes use only lines and rectangles for XP compatibility.

void SidebarPanel::DrawNavIcon(wxDC& dc, NavIcon icon, int cx, int cy,
                               const wxColour& col)
{
    wxPen  pen(col, 1, wxPENSTYLE_SOLID);
    wxPen  pen2(col, 2, wxPENSTYLE_SOLID);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    int h = kIconSz / 2;   // half = 8

    switch (icon) {

    case ICON_DASHBOARD: {
        // 2×2 grid of rounded squares
        dc.SetPen(pen);
        dc.DrawRoundedRectangle(cx - h,     cy - h,     h - 1, h - 1, 1);
        dc.DrawRoundedRectangle(cx + 1,     cy - h,     h - 1, h - 1, 1);
        dc.DrawRoundedRectangle(cx - h,     cy + 1,     h - 1, h - 1, 1);
        dc.DrawRoundedRectangle(cx + 1,     cy + 1,     h - 1, h - 1, 1);
        break;
    }

    case ICON_BILLING: {
        // Receipt: rounded rect with 3 horizontal lines inside
        dc.SetPen(pen);
        dc.DrawRoundedRectangle(cx - h + 1, cy - h, kIconSz - 2, kIconSz, 2);
        dc.SetPen(pen);
        dc.DrawLine(cx - h + 4, cy - 3, cx + h - 4, cy - 3);
        dc.DrawLine(cx - h + 4, cy,     cx + h - 4, cy);
        dc.DrawLine(cx - h + 4, cy + 3, cx + h - 4, cy + 3);
        break;
    }

    case ICON_INVENTORY: {
        // Box / package: front face + top parallelogram
        dc.SetPen(pen);
        // front face
        dc.DrawRectangle(cx - h + 1, cy - 2, kIconSz - 2, h + 2);
        // top edge (trapezoid suggestion with two diagonal lines)
        dc.DrawLine(cx - h + 1, cy - 2, cx - 3,    cy - h + 1);
        dc.DrawLine(cx + h - 1, cy - 2, cx + 3,    cy - h + 1);
        dc.DrawLine(cx - 3,     cy - h + 1, cx + 3, cy - h + 1);
        // middle divider line on front face
        dc.DrawLine(cx, cy - 2, cx, cy + h);
        break;
    }

    case ICON_MEDICINES: {
        // Pill: left semicircle + right semicircle connected
        dc.SetPen(pen2);
        // Draw as a wide rounded rectangle rotated 45 deg — approximate with ellipse
        // Use a tilted capsule: draw as rotated rounded rect via lines
        // Simpler: horizontal pill shape
        dc.SetPen(pen);
        dc.DrawRoundedRectangle(cx - h, cy - 4, kIconSz, 8, 4);
        // Dividing line in the middle
        dc.DrawLine(cx, cy - 4, cx, cy + 4);
        // Fill left half suggestion: draw filled half
        dc.SetBrush(wxBrush(col));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRoundedRectangle(cx - h + 1, cy - 3, h - 1, 6, 3);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(pen);
        dc.DrawRoundedRectangle(cx - h, cy - 4, kIconSz, 8, 4);
        break;
    }

    case ICON_PURCHASE: {
        // Shopping cart: base tray + 2 wheels + handle
        dc.SetPen(pen);
        // handle (top-left hook)
        dc.DrawLine(cx - h,     cy - h + 2, cx - h + 3, cy - h + 2);
        dc.DrawLine(cx - h + 3, cy - h + 2, cx - h + 5, cy - 2);
        // tray body
        dc.DrawLine(cx - h + 5, cy - 2, cx + h - 1, cy - 2);
        dc.DrawLine(cx + h - 1, cy - 2, cx + h - 3, cy + 3);
        dc.DrawLine(cx - h + 4, cy + 3, cx + h - 3, cy + 3);
        // wheels
        dc.DrawCircle(cx - h + 5, cy + h - 1, 2);
        dc.DrawCircle(cx + h - 4, cy + h - 1, 2);
        break;
    }

    case ICON_SALES: {
        // Bar chart: 3 vertical bars of increasing height
        dc.SetBrush(wxBrush(col));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(cx - h + 1, cy + 1,      4, h - 1);   // short
        dc.DrawRectangle(cx - 2,     cy - 2,      4, h + 2);   // medium
        dc.DrawRectangle(cx + h - 5, cy - h + 1,  4, kIconSz - 1); // tall
        // baseline
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(pen);
        dc.DrawLine(cx - h, cy + h, cx + h, cy + h);
        break;
    }

    case ICON_CUSTOMERS: {
        // Two person silhouettes
        dc.SetPen(pen);
        // front person: head + shoulders arc
        dc.DrawCircle(cx - 1, cy - 4, 3);
        dc.DrawArc(cx - h + 3, cy + h, cx + 4, cy + h, cx - 1, cy + 2);
        // back person (offset right, lighter)
        dc.SetPen(wxPen(wxColour(col.Red(), col.Green(), col.Blue(), 160), 1));
        dc.DrawCircle(cx + 4, cy - 5, 3);
        dc.DrawArc(cx - 1, cy + h, cx + h - 1, cy + h, cx + 4, cy + 1);
        break;
    }

    case ICON_SETTINGS: {
        // Gear: circle with 4 rectangular teeth
        dc.SetPen(pen);
        dc.DrawCircle(cx, cy, 4);
        // 4 teeth (top/bottom/left/right)
        dc.SetBrush(wxBrush(col));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(cx - 2, cy - h,     4, 3);   // top
        dc.DrawRectangle(cx - 2, cy + h - 3, 4, 3);   // bottom
        dc.DrawRectangle(cx - h,     cy - 2, 3, 4);   // left
        dc.DrawRectangle(cx + h - 3, cy - 2, 3, 4);   // right
        // redraw circle over teeth so it looks right
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(pen);
        dc.DrawCircle(cx, cy, 4);
        break;
    }

    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void SidebarPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    wxSize sz = GetClientSize();

    dc.SetBackground(wxBrush(clr::SidebarBg()));
    dc.Clear();

    // ── Logo area ─────────────────────────────────────────────────────────────
    dc.SetBrush(wxBrush(clr::Brand()));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRoundedRectangle(12, 14, 32, 32, 6);
    dc.SetTextForeground(*wxWHITE);
    wxFont logoF = dc.GetFont(); logoF.SetPointSize(14); logoF.MakeBold();
    dc.SetFont(logoF);
    dc.DrawText("A", 22, 20);

    wxFont nameF = dc.GetFont(); nameF.SetPointSize(sz::FontPt); nameF.MakeBold();
    dc.SetFont(nameF);
    dc.SetTextForeground(*wxWHITE);
    dc.DrawText("Aushadhalayam", 52, 16);
    wxFont subF = dc.GetFont(); subF.SetPointSize(8);
    subF.SetStyle(wxFONTSTYLE_NORMAL); subF.SetWeight(wxFONTWEIGHT_NORMAL);
    dc.SetFont(subF);
    dc.SetTextForeground(clr::SidebarText());
    dc.DrawText("Pharmacy POS", 52, 30);

    dc.SetPen(wxPen(clr::SidebarHoverBg()));
    dc.DrawLine(12, kLogoH - 4, sz::SidebarW - 12, kLogoH - 4);

    // ── Nav items ─────────────────────────────────────────────────────────────
    wxFont itemF; itemF.SetPointSize(sz::FontPt);
    wxFont secF;  secF.SetPointSize(8); secF.MakeBold();

    for (int i = 0; i < (int)m_entries.size(); ++i) {
        const NavEntry& e = m_entries[i];
        int iy = ItemY(i);

        // Section label
        if (!e.section.empty()) {
            dc.SetFont(secF);
            dc.SetTextForeground(clr::SidebarText());
            wxString sec = wxString::FromUTF8(e.section.c_str()).Upper();
            dc.DrawText(sec, kPadX + 6, iy - kSecH + 7);
        }

        bool active  = (e.page_id == m_active);
        bool hovered = (i == m_hover && !active);

        // Item background
        dc.SetPen(*wxTRANSPARENT_PEN);
        if (active) {
            dc.SetBrush(wxBrush(clr::SidebarActBg()));
            dc.DrawRoundedRectangle(8, iy, sz::SidebarW - 16, kItemH, 5);
        } else if (hovered) {
            dc.SetBrush(wxBrush(clr::SidebarHoverBg()));
            dc.DrawRoundedRectangle(8, iy, sz::SidebarW - 16, kItemH, 5);
        }

        wxColour iconCol = (active || hovered) ? clr::SidebarActTxt() : clr::SidebarText();

        // Icon (centred vertically in item, left-padded)
        int iconCx = kPadX + 8 + kIconSz / 2;
        int iconCy = iy + kItemH / 2;
        DrawNavIcon(dc, e.icon, iconCx, iconCy, iconCol);

        // Label (offset right of icon)
        dc.SetFont(itemF);
        dc.SetTextForeground(iconCol);
        wxString lbl = wxString::FromUTF8(e.label.c_str());
        int tw, th;
        dc.GetTextExtent(lbl, &tw, &th);
        dc.DrawText(lbl, kPadX + 8 + kIconSz + 8, iy + (kItemH - th) / 2);
    }

    // ── Version ───────────────────────────────────────────────────────────────
    dc.SetFont(secF);
    dc.SetTextForeground(clr::SidebarText());
    dc.DrawText("v0.1.0", kPadX + 8, sz.y - 20);
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void SidebarPanel::OnMouseMove(wxMouseEvent& e) {
    int idx = HitTest(e.GetY());
    if (idx != m_hover) { m_hover = idx; Refresh(false); }
    e.Skip();
}

void SidebarPanel::OnMouseLeave(wxMouseEvent& e) {
    m_hover = -1; Refresh(false); e.Skip();
}

void SidebarPanel::OnLeftDown(wxMouseEvent& e) {
    int idx = HitTest(e.GetY());
    if (idx >= 0) {
        m_active = m_entries[idx].page_id;
        Refresh(false);
        wxCommandEvent nav(wxEVT_SIDEBAR_NAV, GetId());
        nav.SetInt(m_entries[idx].page_id);
        wxPostEvent(GetParent(), nav);
    }
    e.Skip();
}
