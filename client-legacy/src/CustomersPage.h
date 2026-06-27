#pragma once
#include "Pages.h"
#include <wx/listctrl.h>
class CustomersPage : public BasePage {
public:
    CustomersPage(wxWindow* parent, PharmacyWorkerThread* worker);
    void OnPageShown() override;
private:
    wxTextCtrl* m_txt;
    wxListCtrl* m_list;
    wxTimer     m_timer;
    long long   m_req;
    void OnText(wxCommandEvent&);
    void OnTimer(wxTimerEvent&);
    void HandleDbResult(long long, const std::string&) override;
    wxDECLARE_EVENT_TABLE();
};
