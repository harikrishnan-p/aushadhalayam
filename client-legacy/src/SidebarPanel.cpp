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
    SetBackgroundStyle(wxBG_STYLE_PAINT);   // suppress default erase
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

// ── Paint ─────────────────────────────────────────────────────────────────────

void SidebarPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    wxSize sz = GetClientSize();

    // Background
    dc.SetBackground(wxBrush(clr::SidebarBg()));
    dc.Clear();

    // ── Logo area ─────────────────────────────────────────────────────────────
    // "A" badge
    dc.SetBrush(wxBrush(clr::Brand()));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRoundedRectangle(12, 14, 32, 32, 6);
    dc.SetTextForeground(*wxWHITE);
    wxFont logoF = dc.GetFont(); logoF.SetPointSize(14); logoF.MakeBold();
    dc.SetFont(logoF);
    dc.DrawText("A", 22, 20);

    // App name
    wxFont nameF = dc.GetFont(); nameF.SetPointSize(sz::FontPt); nameF.MakeBold();
    dc.SetFont(nameF);
    dc.SetTextForeground(*wxWHITE);
    dc.DrawText("Aushadhalayam", 52, 16);
    wxFont subF = dc.GetFont(); subF.SetPointSize(8); subF.SetStyle(wxFONTSTYLE_NORMAL);
    dc.SetFont(subF);
    dc.SetTextForeground(clr::SidebarText());
    dc.DrawText("Pharmacy POS", 52, 30);

    // Divider
    dc.SetPen(wxPen(wxColour(255,255,255,30)));
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
            dc.DrawText(sec, kPadX + 4, iy - kSecH + 8);
        }

        bool active = (e.page_id == m_active);
        bool hovered = (i == m_hover && !active);

        // Item background
        dc.SetPen(*wxTRANSPARENT_PEN);
        if (active) {
            dc.SetBrush(wxBrush(clr::SidebarActBg()));
            dc.DrawRoundedRectangle(8, iy, sz::SidebarW - 16, kItemH, 5);
        } else if (hovered) {
            dc.SetBrush(wxBrush(wxColour(255,255,255,20)));
            dc.DrawRoundedRectangle(8, iy, sz::SidebarW - 16, kItemH, 5);
        }

        // Item label
        dc.SetFont(itemF);
        dc.SetTextForeground(active ? clr::SidebarActTxt() : clr::SidebarText());
        wxString lbl = wxString::FromUTF8(e.label.c_str());
        int tw, th;
        dc.GetTextExtent(lbl, &tw, &th);
        dc.DrawText(lbl, kPadX + 8, iy + (kItemH - th) / 2);
    }

    // ── Version at bottom ─────────────────────────────────────────────────────
    dc.SetFont(secF);
    dc.SetTextForeground(clr::SidebarText());
    dc.DrawText("v0.1.0", kPadX + 8, sz.y - 20);
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void SidebarPanel::OnMouseMove(wxMouseEvent& e) {
    int idx = HitTest(e.GetY());
    if (idx != m_hover) {
        m_hover = idx;
        Refresh(false);
    }
    e.Skip();
}

void SidebarPanel::OnMouseLeave(wxMouseEvent& e) {
    m_hover = -1;
    Refresh(false);
    e.Skip();
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
