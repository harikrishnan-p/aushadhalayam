#pragma once
// =============================================================================
// SidebarPanel.h  —  Custom-painted sidebar navigation
// =============================================================================

#include <wx/wx.h>
#include <vector>
#include <string>

wxDECLARE_EVENT(wxEVT_SIDEBAR_NAV, wxCommandEvent);

enum NavIcon {
    ICON_DASHBOARD = 0,
    ICON_BILLING,
    ICON_INVENTORY,
    ICON_MEDICINES,
    ICON_PURCHASE,
    ICON_SALES,
    ICON_CUSTOMERS,
    ICON_SETTINGS,
};

struct NavEntry {
    std::string label;
    std::string section;   // if non-empty, draw a section heading before this item
    int         page_id;
    NavIcon     icon;
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
    int  m_hover;

    static const int kLogoH  = 64;
    static const int kItemH  = 34;
    static const int kPadX   = 10;
    static const int kSecH   = 26;
    static const int kIconSz = 16;   // icon bounding box

    int ItemY(int idx) const;
    int HitTest(int y)  const;

    void DrawNavIcon(wxDC& dc, NavIcon icon, int cx, int cy, const wxColour& col);

    void OnPaint     (wxPaintEvent&);
    void OnMouseMove (wxMouseEvent&);
    void OnMouseLeave(wxMouseEvent&);
    void OnLeftDown  (wxMouseEvent&);

    wxDECLARE_EVENT_TABLE();
};
