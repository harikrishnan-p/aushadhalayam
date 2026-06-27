// =============================================================================
// ChartPanel.cpp
// =============================================================================

#include "ChartPanel.h"
#include <wx/dcbuffer.h>
#include "AppColors.h"

wxBEGIN_EVENT_TABLE(ChartPanel, wxPanel)
    EVT_PAINT(ChartPanel::OnPaint)
wxEND_EVENT_TABLE()

ChartPanel::ChartPanel(wxWindow* parent, int height)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, height))
    , m_height(height)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(-1, height));
    SetBackgroundColour(clr::Surface());
}

void ChartPanel::SetBars(const std::vector<ChartBar>& bars) {
    m_bars = bars;
    Refresh();
}

void ChartPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    wxSize sz = GetClientSize();

    dc.SetBackground(wxBrush(clr::Surface()));
    dc.Clear();

    if (m_bars.empty()) return;

    // Find max
    double maxV = 1.0;
    for (const auto& b : m_bars) if (b.value > maxV) maxV = b.value;

    const int labelH  = 16;  // height reserved for labels below bars
    const int padLeft = 4;
    const int padRight= 4;
    const int barAreaH= sz.y - labelH - 4;
    int n = (int)m_bars.size();
    int barW = (sz.x - padLeft - padRight) / n;
    int gap  = 3;

    wxFont lf; lf.SetPointSize(8);
    dc.SetFont(lf);

    for (int i = 0; i < n; ++i) {
        int x = padLeft + i * barW;
        int barPixH = (int)(m_bars[i].value / maxV * (barAreaH - 4));
        if (barPixH < 2) barPixH = 2;

        // Bar
        bool isLast = (i == n - 1);
        wxColour barClr = isLast ? clr::Brand() : wxColour(clr::Brand().Red(),
            clr::Brand().Green(), clr::Brand().Blue(), 180);
        dc.SetBrush(wxBrush(barClr));
        dc.SetPen(*wxTRANSPARENT_PEN);
        int bx = x + gap / 2;
        int bw = barW - gap;
        int by = barAreaH - barPixH;
        dc.DrawRoundedRectangle(bx, by, bw, barPixH, 3);

        // Label
        dc.SetTextForeground(clr::Text3());
        wxString lbl = wxString::FromUTF8(m_bars[i].label.c_str());
        int tw, th;
        dc.GetTextExtent(lbl, &tw, &th);
        int lx = bx + (bw - tw) / 2;
        dc.DrawText(lbl, lx, sz.y - labelH);
    }
}
