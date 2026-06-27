#pragma once
#include "Pages.h"
#include <wx/listctrl.h>

class MedicinesPage : public BasePage {
public:
    MedicinesPage(wxWindow* parent, PharmacyWorkerThread* worker);
    void OnPageShown() override;
private:
    wxTextCtrl* m_txt_search;
    wxListCtrl* m_list;
    wxTimer     m_timer;
    long long   m_req;
    void OnSearchText(wxCommandEvent&);
    void OnTimer(wxTimerEvent&);
    void HandleDbResult(long long, const std::string&) override;
    wxDECLARE_EVENT_TABLE();
};
