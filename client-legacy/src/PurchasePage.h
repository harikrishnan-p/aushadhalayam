#pragma once
#include "Pages.h"
#include <wx/listctrl.h>
class PurchasePage : public BasePage {
public:
    PurchasePage(wxWindow* parent, PharmacyWorkerThread* worker);
    void OnPageShown() override;
private:
    wxListCtrl* m_list;
    long long   m_req;
    void HandleDbResult(long long, const std::string&) override;
};
