#pragma once
// =============================================================================
// ChartPanel.h  —  Simple bar chart drawn with wxDC
// =============================================================================

#include <wx/wx.h>
#include <vector>
#include <string>

struct ChartBar {
    std::string label;   // shown below bar (e.g. "20 Jun")
    double      value;   // bar height is proportional to max value
};

class ChartPanel : public wxPanel {
public:
    ChartPanel(wxWindow* parent, int height = 110);
    void SetBars(const std::vector<ChartBar>& bars);

private:
    std::vector<ChartBar> m_bars;
    int m_height;

    void OnPaint(wxPaintEvent&);
    wxDECLARE_EVENT_TABLE();
};
