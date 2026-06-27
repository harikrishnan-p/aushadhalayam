#pragma once
#include "Pages.h"
#include "ChartPanel.h"
#include <wx/listctrl.h>

class DashboardPage : public BasePage {
public:
    DashboardPage(wxWindow* parent, PharmacyWorkerThread* worker);
    void OnPageShown() override;

private:
    // KPI labels
    wxStaticText* m_lbl_sales;
    wxStaticText* m_lbl_bills;
    wxStaticText* m_lbl_alerts;
    wxStaticText* m_lbl_sync;

    ChartPanel*   m_chart;
    wxListCtrl*   m_alert_list;

    long long m_req_low_stock;
    long long m_req_sync;
    long long m_req_period;

    void BuildUI();
    void HandleDbResult(long long req_id, const std::string& json) override;
    void ParseLowStock(const std::string& json);
    void ParsePeriodSales(const std::string& json);
};
