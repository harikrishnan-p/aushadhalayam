#pragma once
// =============================================================================
// WorkerThread.h  —  Background SQLite worker thread for the legacy POS client
//
// Architecture
// ─────────────
// The GUI (main) thread and the database thread are completely separated:
//
//   GUI Thread                      Worker Thread
//   ──────────                      ─────────────
//   PostTask(DbTask)  ──queue──►   Entry() loop
//                                   │ process task
//                     ◄──event──   wxQueueEvent(sink, result)
//   OnDbResult / OnDbError
//
// Thread safety rules:
//   • m_queue_mutex guards m_task_queue and m_quit.
//   • m_queue_cond is signalled by the GUI thread after enqueuing.
//   • sqlite3* m_db is created and destroyed entirely within Entry().
//   • wxQueueEvent is the ONLY mechanism to return data to the GUI.
//     (Direct wxWindow method calls from a non-GUI thread are illegal.)
//
// Usage:
//   auto* t = new PharmacyWorkerThread(frame, db_path);
//   if (t->Create() == wxTHREAD_NO_ERROR) t->Run();
//   // Later, to stop:
//   DbTask q; q.type = DbCommandType::CMD_QUIT;
//   t->PostTask(q);
//   t->Wait();
// =============================================================================
#include <wx/wx.h>
#include <wx/thread.h>
#include <deque>
#include <string>
#include <utility>
#include <vector>

// Forward declaration only — keeps sqlite3.h out of this header
struct sqlite3;

// ─────────────────────────────────────────────────────────────────────────────
// Custom wxCommandEvents (declared here, defined in WorkerThread.cpp)
// ─────────────────────────────────────────────────────────────────────────────
wxDECLARE_EVENT(wxEVT_DB_RESULT, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_DB_ERROR,  wxCommandEvent);

// ─────────────────────────────────────────────────────────────────────────────
// Task command codes
// ─────────────────────────────────────────────────────────────────────────────
enum class DbCommandType : int {
    CMD_SEARCH_PRODUCT  = 1,   // payload: {"query":"..."} → product array JSON
    CMD_GET_STOCK_BATCH = 2,   // payload: {"product_id":N} → batch array JSON
    CMD_CHECKOUT        = 3,   // payload: CheckoutRequest JSON → BillResult JSON
    CMD_CANCEL_BILL     = 4,   // payload: {"bill_id":N,"reason":"..."}
    CMD_FLUSH_SYNC      = 5,   // payload: {} → pending sync count JSON
    CMD_GET_LOW_STOCK   = 6,   // payload: {} → low-stock array JSON
    CMD_SEARCH_CUSTOMER = 7,   // payload: {"query":"..."} → customer array JSON
    CMD_GET_INVENTORY   = 8,   // payload: {} → all non-expired batches JSON
    CMD_SEARCH_MEDICINE = 9,   // payload: {"query":"..."} → product+schedule JSON
    CMD_GET_PERIOD_SALES= 10,  // payload: {"month":"YYYY-MM"} → daily totals JSON
    CMD_QUIT            = 99,
};

// Serialisable task unit passed into the worker queue
struct DbTask {
    DbCommandType type;
    std::string   payload;      // JSON string — parameters for the command
    long long     request_id;   // echoed back in the result event for correlation
};

// ─────────────────────────────────────────────────────────────────────────────
// PharmacyWorkerThread
// ─────────────────────────────────────────────────────────────────────────────
class PharmacyWorkerThread : public wxThread {
public:
    // sink       — the wxEvtHandler (usually MainFrame) that receives events
    // db_path    — absolute path to the SQLite database file
    // device_id  — unique identifier for this POS terminal (written to LWW columns)
    PharmacyWorkerThread(wxEvtHandler* sink,
                         const wxString& db_path,
                         const std::string& device_id);

    ~PharmacyWorkerThread();

    // Thread-safe.  May be called from any thread.
    // Returns false only if the internal mutex fails (fatal).
    bool PostTask(const DbTask& task);

    // Called by the wxThread infrastructure — do not call directly
    wxThread::ExitCode Entry() override;

private:
    wxEvtHandler*      m_sink;
    wxString           m_db_path;
    std::string        m_device_id;

    // Task queue protected by mutex + condition variable
    mutable wxMutex    m_queue_mutex;
    wxCondition        m_queue_cond;
    std::deque<DbTask> m_task_queue;
    bool               m_quit;

    // ── DB helpers (worker thread only) ──────────────────────────────────────
    sqlite3*           m_db;

    bool InitDatabase();
    void CloseDatabase();

    std::string HandleSearchProduct (const std::string& json);
    std::string HandleGetStockBatch (const std::string& json);
    std::string HandleCheckout      (const std::string& json);
    std::string HandleCancelBill    (const std::string& json);
    std::string HandleFlushSync      (const std::string& json);
    std::string HandleGetLowStock    (const std::string& json);
    std::string HandleSearchCustomer (const std::string& json);
    std::string HandleGetInventory   (const std::string& json);
    std::string HandleSearchMedicine (const std::string& json);
    std::string HandleGetPeriodSales (const std::string& json);

    // Post events back to the GUI thread (wxQueueEvent is thread-safe)
    void PostResult(long long request_id, const std::string& json);
    void PostError (long long request_id, const std::string& error);

    wxDECLARE_NO_COPY_CLASS(PharmacyWorkerThread);
};

// ─────────────────────────────────────────────────────────────────────────────
// Minimal JSON helpers (no external library)
// ─────────────────────────────────────────────────────────────────────────────
namespace jsonutil {

std::string escape(const std::string& s);

// Extract a named value from a flat JSON object as a string.
// Not a full JSON parser — only handles our own well-formed payloads.
std::string extract_str(const std::string& json, const std::string& key);
long long   extract_int(const std::string& json, const std::string& key);
double      extract_dbl(const std::string& json, const std::string& key);

// Build simple JSON object from field pairs
// Usage: make_obj({"key","\"val\""}, {"n","42"}) → {"key":"val","n":42}
std::string make_obj(const std::vector<std::pair<std::string,std::string>>& fields);
std::string make_str(const std::string& s);  // wraps in "..."
std::string make_arr(const std::vector<std::string>& items);

} // namespace jsonutil
