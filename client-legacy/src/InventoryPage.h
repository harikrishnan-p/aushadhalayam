#pragma once
#include "Pages.h"
#include <wx/listctrl.h>

class InventoryPage : public BasePage {
public:
    InventoryPage(wxWindow* parent, PharmacyWorkerThread* worker);
    void OnPageShown() override;
private:
    wxListCtrl* m_list;
    wxListCtrl* m_alert_list;
    long long   m_req_inv, m_req_alert;
    void HandleDbResult(long long, const std::string&) override;
};
