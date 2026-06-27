#pragma once
// =============================================================================
// Pages.h  —  Base class shared by all screen panels
//
// Each page:
//   • Receives DB results routed from MainFrame::OnDbResult via TryHandleDbResult.
//   • Tracks its own request IDs so only its own results are consumed.
//   • Can post tasks via the shared worker pointer.
// =============================================================================

#include <wx/wx.h>
#include <set>
#include <string>

class PharmacyWorkerThread;

// Shared request-id counter — GUI-thread only, no locking needed.
inline long long next_req_id() {
    static long long s_counter = 0;
    return ++s_counter;
}

class BasePage : public wxPanel {
public:
    BasePage(wxWindow* parent, PharmacyWorkerThread* worker)
        : wxPanel(parent, wxID_ANY)
        , m_worker(worker)
    {
        SetBackgroundColour(wxColour(248, 250, 252)); // clr::Bg()
    }

    // Called by MainFrame for every DB result.
    // Returns true if this page consumed the result.
    virtual bool TryHandleDbResult(long long req_id, const std::string& json) {
        if (m_my_reqs.count(req_id) == 0) return false;
        m_my_reqs.erase(req_id);
        HandleDbResult(req_id, json);
        return true;
    }

    virtual bool TryHandleDbError(long long req_id, const std::string& msg) {
        if (m_my_reqs.count(req_id) == 0) return false;
        m_my_reqs.erase(req_id);
        HandleDbError(req_id, msg);
        return true;
    }

    // Called when the page becomes visible.
    virtual void OnPageShown() {}

protected:
    PharmacyWorkerThread* m_worker;

    long long PostReq(long long id) {
        m_my_reqs.insert(id);
        return id;
    }

    virtual void HandleDbResult(long long /*req_id*/, const std::string& /*json*/) {}
    virtual void HandleDbError (long long /*req_id*/, const std::string& /*msg*/)  {}

private:
    std::set<long long> m_my_reqs;
};
