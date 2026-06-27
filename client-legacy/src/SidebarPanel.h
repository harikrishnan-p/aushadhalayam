#pragma once
// =============================================================================
// SidebarPanel.h  —  Custom-painted sidebar navigation
//
// Matches the React sidebar: 220px wide, dark background (#1e293b), logo area,
// section labels, and nav buttons with hover/active highlight.
// =============================================================================

#include <wx/wx.h>
#include <vector>
#include <string>

wxDECLARE_EVENT(wxEVT_SIDEBAR_NAV, wxCommandEvent);

struct NavEntry {
    std::string label;
    std::string section;   // if non-empty, draw a section heading before this item
    int         page_id;
};

class SidebarPanel : public wxPanel {
public:
    SidebarPanel(wxWindow* parent,
                 const std::vector<NavEntry>& entries,
                 int initial_page);

    void SetActivePage(int page_id);
    int  GetActivePage() const { return m_active; }

private:
    std::vector<NavEntry> m_entries;
    int  m_active;
    int  m_hover;   // index of hovered item (-1 = none)

    // Geometry helpers
    static const int kLogoH   = 64;   // logo area height
    static const int kItemH   = 32;   // nav item height
    static const int kPadX    = 10;   // horizontal padding inside item
    static const int kPadY    = 6;    // top padding for first item after section label
    static const int kSecH    = 28;   // section label row height

    int ItemY(int idx) const;         // y-top of nav item[idx]
    int HitTest(int y)  const;        // returns item index or -1

    void OnPaint     (wxPaintEvent&);
    void OnMouseMove (wxMouseEvent&);
    void OnMouseLeave(wxMouseEvent&);
    void OnLeftDown  (wxMouseEvent&);

    wxDECLARE_EVENT_TABLE();
};
