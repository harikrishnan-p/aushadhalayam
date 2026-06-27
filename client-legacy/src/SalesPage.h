#pragma once
#include "Pages.h"
#include "ChartPanel.h"
#include <wx/listctrl.h>
#include <wx/datectrl.h>
class SalesPage : public BasePage {
public:
    SalesPage(wxWindow* parent, PharmacyWorkerThread* worker);
    void OnPageShown() override;
private:
    wxStaticText* m_lbl_total;
    wxStaticText* m_lbl_bills;
    ChartPanel*   m_chart;
    wxListCtrl*   m_list;
    long long     m_req;
    void HandleDbResult(long long, const std::string&) override;
};
