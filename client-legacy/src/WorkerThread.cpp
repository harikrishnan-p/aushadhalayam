// =============================================================================
// WorkerThread.cpp
// =============================================================================

#include "WorkerThread.h"
#include "GstEngine.h"
#include "sqlite3.h"

#include <wx/log.h>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Event type definitions
// ─────────────────────────────────────────────────────────────────────────────
wxDEFINE_EVENT(wxEVT_DB_RESULT, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_DB_ERROR,  wxCommandEvent);

// ─────────────────────────────────────────────────────────────────────────────
// jsonutil — minimal JSON builder / extractor for self-generated payloads
// ─────────────────────────────────────────────────────────────────────────────
namespace jsonutil {

std::string escape(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 4);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", unsigned(c));
                    r += buf;
                } else {
                    r += char(c);
                }
        }
    }
    return r;
}

std::string make_str(const std::string& s) {
    return "\"" + escape(s) + "\"";
}

std::string make_obj(const std::vector<std::pair<std::string,std::string>>& fields) {
    std::string r = "{";
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i) r += ",";
        r += "\"" + fields[i].first + "\":" + fields[i].second;
    }
    r += "}";
    return r;
}

std::string make_arr(const std::vector<std::string>& items) {
    std::string r = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) r += ",";
        r += items[i];
    }
    r += "]";
    return r;
}

// Extract a named value from a FLAT JSON object (no nesting).
// Handles string and number values.  Not a general-purpose parser.
std::string extract_str(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    pos += pattern.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) ++pos;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        ++pos;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    case 'r':  result += '\r'; break;
                    default:   result += json[pos];
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        return result;
    }
    // Numeric / boolean
    size_t end = json.find_first_of(",}", pos);
    if (end == std::string::npos) end = json.size();
    std::string val = json.substr(pos, end - pos);
    while (!val.empty() && val.back() == ' ') val.pop_back();
    return val;
}

long long extract_int(const std::string& json, const std::string& key) {
    std::string v = extract_str(json, key);
    return v.empty() ? 0LL : std::stoll(v);
}

double extract_dbl(const std::string& json, const std::string& key) {
    std::string v = extract_str(json, key);
    return v.empty() ? 0.0 : std::stod(v);
}

} // namespace jsonutil

// ─────────────────────────────────────────────────────────────────────────────
// RAII statement wrapper (local to this TU for brevity)
// ─────────────────────────────────────────────────────────────────────────────
struct Stmt {
    sqlite3_stmt* s = nullptr;
    explicit Stmt(sqlite3* db, const char* sql) {
        sqlite3_prepare_v2(db, sql, -1, &s, nullptr);
    }
    ~Stmt() { if (s) sqlite3_finalize(s); }
    int step() { return sqlite3_step(s); }
    void bind_text  (int i, const std::string& v) { sqlite3_bind_text  (s, i, v.c_str(), -1, SQLITE_TRANSIENT); }
    void bind_int   (int i, int v)                { sqlite3_bind_int   (s, i, v); }
    void bind_int64 (int i, sqlite3_int64 v)      { sqlite3_bind_int64 (s, i, v); }
    void bind_double(int i, double v)             { sqlite3_bind_double(s, i, v); }
    void bind_null  (int i)                       { sqlite3_bind_null  (s, i); }
    std::string col_text  (int i) { const unsigned char* t = sqlite3_column_text  (s,i); return t ? (const char*)t : ""; }
    int         col_int   (int i) { return sqlite3_column_int   (s,i); }
    sqlite3_int64 col_int64(int i){ return sqlite3_column_int64 (s,i); }
    double      col_double(int i) { return sqlite3_column_double(s,i); }
    bool ok() const { return s != nullptr; }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper: current microseconds since Unix epoch (no C++11 <chrono> needed)
// ─────────────────────────────────────────────────────────────────────────────
static sqlite3_int64 now_us() {
    // GetSystemTimeAsFileTime is available on XP SP3+
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    sqlite3_int64 t = (sqlite3_int64(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    // Convert Windows epoch (100ns intervals since 1601-01-01) to Unix epoch microseconds
    // Offset: 116444736000000000 × 100ns intervals from 1601 to 1970
    return (t - 116444736000000000LL) / 10;
}

// ─────────────────────────────────────────────────────────────────────────────
// PharmacyWorkerThread
// ─────────────────────────────────────────────────────────────────────────────

PharmacyWorkerThread::PharmacyWorkerThread(wxEvtHandler* sink,
                                           const wxString& db_path,
                                           const std::string& device_id)
    : wxThread(wxTHREAD_JOINABLE)
    , m_sink(sink)
    , m_db_path(db_path)
    , m_device_id(device_id)
    , m_queue_cond(m_queue_mutex)
    , m_quit(false)
    , m_db(nullptr)
{}

PharmacyWorkerThread::~PharmacyWorkerThread() {}

bool PharmacyWorkerThread::PostTask(const DbTask& task) {
    wxMutexLocker lock(m_queue_mutex);
    if (!lock.IsOk()) return false;
    m_task_queue.push_back(task);
    m_queue_cond.Signal();
    return true;
}

// ── Thread Entry Point ────────────────────────────────────────────────────────

wxThread::ExitCode PharmacyWorkerThread::Entry() {
    if (!InitDatabase()) {
        PostError(-1, "Failed to open database: " + std::string(m_db_path.mb_str()));
        return reinterpret_cast<wxThread::ExitCode>(1);
    }

    while (!TestDestroy()) {
        DbTask task;
        bool has_task = false;

        {
            wxMutexLocker lock(m_queue_mutex);
            if (m_task_queue.empty()) {
                // Wait up to 200ms then re-evaluate TestDestroy()
                m_queue_cond.WaitTimeout(200);
            }
            if (!m_task_queue.empty()) {
                task = m_task_queue.front();
                m_task_queue.pop_front();
                has_task = true;
            }
        }

        if (!has_task) continue;
        if (task.type == DbCommandType::CMD_QUIT) break;

        std::string result;
        bool ok = true;

        try {
            switch (task.type) {
                case DbCommandType::CMD_SEARCH_PRODUCT:
                    result = HandleSearchProduct(task.payload);
                    break;
                case DbCommandType::CMD_GET_STOCK_BATCH:
                    result = HandleGetStockBatch(task.payload);
                    break;
                case DbCommandType::CMD_CHECKOUT:
                    result = HandleCheckout(task.payload);
                    break;
                case DbCommandType::CMD_CANCEL_BILL:
                    result = HandleCancelBill(task.payload);
                    break;
                case DbCommandType::CMD_FLUSH_SYNC:
                    result = HandleFlushSync(task.payload);
                    break;
                case DbCommandType::CMD_GET_LOW_STOCK:
                    result = HandleGetLowStock(task.payload);
                    break;
                default:
                    ok = false;
                    result = "Unknown command";
            }
        } catch (const std::exception& e) {
            ok = false;
            result = e.what();
        }

        if (ok) PostResult(task.request_id, result);
        else    PostError (task.request_id, result);
    }

    CloseDatabase();
    return reinterpret_cast<wxThread::ExitCode>(0);
}

// ── DB Lifecycle ──────────────────────────────────────────────────────────────

bool PharmacyWorkerThread::InitDatabase() {
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
    if (sqlite3_open_v2(m_db_path.mb_str(), &m_db, flags, nullptr) != SQLITE_OK)
        return false;

    const char* pragmas =
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=NORMAL;"
        "PRAGMA foreign_keys=ON;"
        "PRAGMA temp_store=MEMORY;"
        "PRAGMA cache_size=-8000;"
        "PRAGMA busy_timeout=5000;";
    char* errmsg = nullptr;
    sqlite3_exec(m_db, pragmas, nullptr, nullptr, &errmsg);
    if (errmsg) { sqlite3_free(errmsg); }

    // Register this device
    Stmt ins(m_db,
        "INSERT OR IGNORE INTO devices(device_id,device_name) VALUES(?,?);");
    if (ins.ok()) {
        ins.bind_text(1, m_device_id);
        ins.bind_text(2, "POS-" + m_device_id.substr(0, 8));
        ins.step();
    }

    return true;
}

void PharmacyWorkerThread::CloseDatabase() {
    if (m_db) {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
}

// ── Event Posting ─────────────────────────────────────────────────────────────

void PharmacyWorkerThread::PostResult(long long request_id, const std::string& json) {
    wxCommandEvent* ev = new wxCommandEvent(wxEVT_DB_RESULT);
    ev->SetExtraLong(static_cast<long>(request_id));
    ev->SetString(wxString::FromUTF8(json.c_str()));
    wxQueueEvent(m_sink, ev);  // thread-safe: ownership transferred
}

void PharmacyWorkerThread::PostError(long long request_id, const std::string& error) {
    wxCommandEvent* ev = new wxCommandEvent(wxEVT_DB_ERROR);
    ev->SetExtraLong(static_cast<long>(request_id));
    ev->SetString(wxString::FromUTF8(error.c_str()));
    wxQueueEvent(m_sink, ev);
}

// ── Command Handlers ──────────────────────────────────────────────────────────

std::string PharmacyWorkerThread::HandleSearchProduct(const std::string& json) {
    std::string query = jsonutil::extract_str(json, "query");

    const char* sql =
        "SELECT p.id, p.sku, p.name, p.generic_name, p.hsn_code, p.gst_rate,"
        "       p.unit, p.mrp, p.is_scheduled,"
        "       COALESCE(SUM(sb.quantity),0) AS total_qty"
        " FROM products p"
        " LEFT JOIN stock_batches sb ON sb.product_id = p.id"
        "    AND sb.is_deleted=0 AND sb.expiry_date > strftime('%Y-%m','now')"
        " WHERE p.is_deleted=0"
        "   AND (p.name LIKE ?1 OR p.sku LIKE ?1 OR p.generic_name LIKE ?1)"
        " GROUP BY p.id"
        " ORDER BY total_qty DESC"
        " LIMIT 20;";

    std::string pattern = "%" + query + "%";
    std::vector<std::string> rows;

    Stmt st(m_db, sql);
    if (!st.ok()) return "[]";
    st.bind_text(1, pattern);

    while (st.step() == SQLITE_ROW) {
        using namespace jsonutil;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", (long long)st.col_int64(0));
        rows.push_back(make_obj({
            {"id",           buf},
            {"sku",          make_str(st.col_text(1))},
            {"name",         make_str(st.col_text(2))},
            {"generic_name", make_str(st.col_text(3))},
            {"hsn_code",     make_str(st.col_text(4))},
            {"gst_rate",     std::to_string(st.col_double(5))},
            {"unit",         make_str(st.col_text(6))},
            {"mrp",          std::to_string(st.col_double(7))},
            {"is_scheduled", std::to_string(st.col_int(8))},
            {"total_qty",    std::to_string((int)st.col_int64(9))},
        }));
    }
    return jsonutil::make_arr(rows);
}

std::string PharmacyWorkerThread::HandleGetStockBatch(const std::string& json) {
    long long product_id = jsonutil::extract_int(json, "product_id");

    const char* sql =
        "SELECT id, batch_number, expiry_date, quantity, mrp"
        " FROM stock_batches"
        " WHERE product_id=? AND is_deleted=0 AND quantity>0"
        "   AND expiry_date > strftime('%Y-%m','now')"
        " ORDER BY expiry_date ASC;";  // FEFO dispatch

    std::vector<std::string> rows;
    Stmt st(m_db, sql);
    if (!st.ok()) return "[]";
    st.bind_int64(1, product_id);

    while (st.step() == SQLITE_ROW) {
        using namespace jsonutil;
        rows.push_back(make_obj({
            {"id",           std::to_string((int)st.col_int64(0))},
            {"batch_number", make_str(st.col_text(1))},
            {"expiry_date",  make_str(st.col_text(2))},
            {"quantity",     std::to_string(st.col_int(3))},
            {"mrp",          std::to_string(st.col_double(4))},
        }));
    }
    return jsonutil::make_arr(rows);
}

// Checkout request JSON shape:
// {
//   "customer_name": "Walk-in",
//   "payment_mode": "CASH",
//   "discount_pct": 0.0,
//   "is_interstate": 0,
//   "items": [
//     {"product_id":1,"batch_id":3,"product_name":"...","hsn_code":"3004",
//      "batch_number":"B001","expiry_date":"2026-12","quantity":2,
//      "mrp":12.0,"discount_pct":0.0,"gst_rate":12.0}
//   ]
// }
std::string PharmacyWorkerThread::HandleCheckout(const std::string& json) {
    // ── Parse header ─────────────────────────────────────────────────────────
    std::string customer_name = jsonutil::extract_str(json, "customer_name");
    std::string payment_mode  = jsonutil::extract_str(json, "payment_mode");
    double bill_disc_pct      = jsonutil::extract_dbl(json, "discount_pct");
    bool is_interstate        = jsonutil::extract_int(json, "is_interstate") != 0;

    if (payment_mode.empty()) payment_mode = "CASH";

    // ── Generate bill number ─────────────────────────────────────────────────
    char bill_number[32];
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        // Count today's bills for serial
        Stmt cnt(m_db,
            "SELECT COUNT(*)+1 FROM bills WHERE bill_date LIKE ?||'%';");
        char today[16];
        std::snprintf(today, sizeof(today), "%04d-%02d-%02d",
                      st.wYear, st.wMonth, st.wDay);
        cnt.bind_text(1, today);
        int serial = 1;
        if (cnt.step() == SQLITE_ROW) serial = cnt.col_int(0);
        std::snprintf(bill_number, sizeof(bill_number),
                      "BILL-%04d%02d%02d-%04d",
                      st.wYear, st.wMonth, st.wDay, serial);
    }

    // ── Parse items array  (manual extract: find "items":[...]) ──────────────
    // Locate array bounds
    size_t items_pos = json.find("\"items\"");
    if (items_pos == std::string::npos)
        throw std::runtime_error("Checkout payload missing 'items' array");
    size_t arr_start = json.find('[', items_pos);
    size_t arr_end   = json.find(']', arr_start);
    if (arr_start == std::string::npos || arr_end == std::string::npos)
        throw std::runtime_error("Malformed items array");

    // Split items by '},{' — works because we control the JSON format
    std::string arr = json.substr(arr_start + 1, arr_end - arr_start - 1);
    std::vector<std::string> item_jsons;
    size_t depth = 0, seg_start = 0;
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i] == '{') { if (depth++ == 0) seg_start = i; }
        else if (arr[i] == '}') {
            if (--depth == 0) item_jsons.push_back(arr.substr(seg_start, i - seg_start + 1));
        }
    }
    if (item_jsons.empty()) throw std::runtime_error("Bill has no items");

    // ── Compute totals ────────────────────────────────────────────────────────
    struct ParsedItem {
        int         product_id, batch_id, quantity;
        std::string product_name, hsn_code, batch_number, expiry_date;
        double      mrp, discount_pct, gst_rate;
        gst::LineResult gst;
    };

    std::vector<ParsedItem> items;
    double sum_taxable = 0, sum_cgst = 0, sum_sgst = 0, sum_igst = 0, sum_total = 0;

    for (const auto& ij : item_jsons) {
        ParsedItem p;
        p.product_id   = static_cast<int>(jsonutil::extract_int(ij, "product_id"));
        p.batch_id     = static_cast<int>(jsonutil::extract_int(ij, "batch_id"));
        p.product_name = jsonutil::extract_str(ij, "product_name");
        p.hsn_code     = jsonutil::extract_str(ij, "hsn_code");
        p.batch_number = jsonutil::extract_str(ij, "batch_number");
        p.expiry_date  = jsonutil::extract_str(ij, "expiry_date");
        p.quantity     = static_cast<int>(jsonutil::extract_int(ij, "quantity"));
        p.mrp          = jsonutil::extract_dbl(ij, "mrp");
        p.discount_pct = jsonutil::extract_dbl(ij, "discount_pct");
        p.gst_rate     = jsonutil::extract_dbl(ij, "gst_rate");
        p.gst          = gst::compute_line(p.mrp, p.quantity, p.discount_pct,
                                           p.gst_rate, is_interstate);
        sum_taxable += p.gst.taxable_value;
        sum_cgst    += p.gst.cgst_amount;
        sum_sgst    += p.gst.sgst_amount;
        sum_igst    += p.gst.igst_amount;
        sum_total   += p.gst.line_total;
        items.push_back(p);
    }

    double grand_total     = gst::round2(sum_total);
    double discount_amount = gst::round2(grand_total * bill_disc_pct / 100.0);
    grand_total           -= discount_amount;

    // ── Atomic transaction ────────────────────────────────────────────────────
    // BEGIN IMMEDIATE prevents another terminal from selling the same stock
    {
        char* errmsg = nullptr;
        sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
        if (errmsg) { sqlite3_free(errmsg); throw std::runtime_error("Could not begin transaction"); }
    }

    // Verify and deduct stock (FEFO: handled by caller selecting correct batch)
    for (const auto& p : items) {
        Stmt chk(m_db,
            "SELECT quantity FROM stock_batches WHERE id=? AND is_deleted=0;");
        if (!chk.ok()) { sqlite3_exec(m_db,"ROLLBACK;",nullptr,nullptr,nullptr); throw std::runtime_error("DB error"); }
        chk.bind_int64(1, p.batch_id);
        if (chk.step() != SQLITE_ROW) {
            sqlite3_exec(m_db,"ROLLBACK;",nullptr,nullptr,nullptr);
            throw std::runtime_error("Batch not found: " + p.batch_number);
        }
        int avail = chk.col_int(0);
        if (avail < p.quantity) {
            sqlite3_exec(m_db,"ROLLBACK;",nullptr,nullptr,nullptr);
            throw std::runtime_error("Insufficient stock for: " + p.product_name +
                                     " (have " + std::to_string(avail) + ")");
        }
    }

    // Deduct stock
    for (const auto& p : items) {
        Stmt upd(m_db,
            "UPDATE stock_batches SET quantity=quantity-?,"
            " updated_at_us=?,"
            " row_version=row_version+1,"
            " device_id=?"
            " WHERE id=?;");
        if (!upd.ok()) { sqlite3_exec(m_db,"ROLLBACK;",nullptr,nullptr,nullptr); throw std::runtime_error("DB error"); }
        upd.bind_int   (1, p.quantity);
        upd.bind_int64 (2, now_us());
        upd.bind_text  (3, m_device_id);
        upd.bind_int64 (4, p.batch_id);
        upd.step();
    }

    // Insert bill header
    {
        Stmt ins(m_db,
            "INSERT INTO bills(bill_number,customer_name,"
            " total_amount,discount_amount,taxable_amount,"
            " cgst_amount,sgst_amount,igst_amount,total_gst_amount,grand_total,"
            " payment_mode,updated_at_us,device_id)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);");
        if (!ins.ok()) { sqlite3_exec(m_db,"ROLLBACK;",nullptr,nullptr,nullptr); throw std::runtime_error("DB error"); }
        double total_gst = gst::round2(sum_cgst + sum_sgst + sum_igst);
        ins.bind_text  (1,  bill_number);
        ins.bind_text  (2,  customer_name);
        ins.bind_double(3,  sum_total);
        ins.bind_double(4,  discount_amount);
        ins.bind_double(5,  gst::round2(sum_taxable));
        ins.bind_double(6,  gst::round2(sum_cgst));
        ins.bind_double(7,  gst::round2(sum_sgst));
        ins.bind_double(8,  gst::round2(sum_igst));
        ins.bind_double(9,  total_gst);
        ins.bind_double(10, grand_total);
        ins.bind_text  (11, payment_mode);
        ins.bind_int64 (12, now_us());
        ins.bind_text  (13, m_device_id);
        ins.step();
    }

    sqlite3_int64 bill_id = sqlite3_last_insert_rowid(m_db);
    if (bill_id == 0) {
        sqlite3_exec(m_db,"ROLLBACK;",nullptr,nullptr,nullptr);
        throw std::runtime_error("Failed to insert bill header");
    }

    // Insert bill items
    for (const auto& p : items) {
        Stmt ins(m_db,
            "INSERT INTO bill_items("
            " bill_id,product_id,batch_id,product_name,hsn_code,"
            " batch_number,expiry_date,quantity,mrp,discount_pct,"
            " unit_price,gst_rate,taxable_value,"
            " cgst_amount,sgst_amount,igst_amount,line_total,"
            " updated_at_us,device_id)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
        if (!ins.ok()) { sqlite3_exec(m_db,"ROLLBACK;",nullptr,nullptr,nullptr); throw std::runtime_error("DB error"); }
        ins.bind_int64 (1,  bill_id);
        ins.bind_int   (2,  p.product_id);
        ins.bind_int   (3,  p.batch_id);
        ins.bind_text  (4,  p.product_name);
        ins.bind_text  (5,  p.hsn_code);
        ins.bind_text  (6,  p.batch_number);
        ins.bind_text  (7,  p.expiry_date);
        ins.bind_int   (8,  p.quantity);
        ins.bind_double(9,  p.mrp);
        ins.bind_double(10, p.discount_pct);
        ins.bind_double(11, p.gst.unit_price);
        ins.bind_double(12, p.gst_rate);
        ins.bind_double(13, p.gst.taxable_value);
        ins.bind_double(14, p.gst.cgst_amount);
        ins.bind_double(15, p.gst.sgst_amount);
        ins.bind_double(16, p.gst.igst_amount);
        ins.bind_double(17, p.gst.line_total);
        ins.bind_int64 (18, now_us());
        ins.bind_text  (19, m_device_id);
        ins.step();
    }

    // Commit — triggers will atomically write sync_outbox entries
    {
        char* errmsg = nullptr;
        sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errmsg);
        if (errmsg) {
            sqlite3_free(errmsg);
            sqlite3_exec(m_db,"ROLLBACK;",nullptr,nullptr,nullptr);
            throw std::runtime_error("Commit failed");
        }
    }

    // Return summary JSON to GUI
    return jsonutil::make_obj({
        {"bill_id",        std::to_string(bill_id)},
        {"bill_number",    jsonutil::make_str(bill_number)},
        {"grand_total",    std::to_string(grand_total)},
        {"cgst_amount",    std::to_string(gst::round2(sum_cgst))},
        {"sgst_amount",    std::to_string(gst::round2(sum_sgst))},
        {"igst_amount",    std::to_string(gst::round2(sum_igst))},
        {"item_count",     std::to_string(items.size())},
    });
}

std::string PharmacyWorkerThread::HandleCancelBill(const std::string& json) {
    long long bill_id    = jsonutil::extract_int(json, "bill_id");
    std::string reason   = jsonutil::extract_str(json, "reason");

    char* errmsg = nullptr;
    sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errmsg);
    if (errmsg) { sqlite3_free(errmsg); throw std::runtime_error("Begin failed"); }

    // Restore stock for each item
    Stmt sel(m_db,
        "SELECT batch_id, quantity FROM bill_items WHERE bill_id=? AND is_deleted=0;");
    sel.bind_int64(1, bill_id);
    std::vector<std::pair<int,int>> restores;
    while (sel.step() == SQLITE_ROW)
        restores.push_back({sel.col_int(0), sel.col_int(1)});

    for (const auto& r : restores) {
        Stmt upd(m_db,
            "UPDATE stock_batches SET quantity=quantity+?,"
            " updated_at_us=?, row_version=row_version+1, device_id=?"
            " WHERE id=?;");
        upd.bind_int  (1, r.second);
        upd.bind_int64(2, now_us());
        upd.bind_text (3, m_device_id);
        upd.bind_int  (4, r.first);
        upd.step();
    }

    Stmt upd(m_db,
        "UPDATE bills SET is_cancelled=1, cancel_reason=?,"
        " updated_at_us=?, row_version=row_version+1, device_id=?"
        " WHERE id=?;");
    upd.bind_text  (1, reason);
    upd.bind_int64 (2, now_us());
    upd.bind_text  (3, m_device_id);
    upd.bind_int64 (4, bill_id);
    upd.step();

    sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
    return jsonutil::make_obj({{"cancelled_bill_id", std::to_string(bill_id)}});
}

std::string PharmacyWorkerThread::HandleFlushSync(const std::string& /*json*/) {
    Stmt cnt(m_db,
        "SELECT COUNT(*) FROM sync_outbox WHERE is_transmitted=0;");
    long long pending = 0;
    if (cnt.ok() && cnt.step() == SQLITE_ROW) pending = cnt.col_int64(0);
    return jsonutil::make_obj({{"pending_sync_count", std::to_string(pending)}});
}

std::string PharmacyWorkerThread::HandleGetLowStock(const std::string& /*json*/) {
    const char* sql =
        "SELECT id,sku,name,reorder_level,"
        " COALESCE(SUM(sb.quantity),0) AS total_stock"
        " FROM products p"
        " LEFT JOIN stock_batches sb ON sb.product_id=p.id"
        "   AND sb.is_deleted=0 AND sb.expiry_date>strftime('%Y-%m','now')"
        " WHERE p.is_deleted=0"
        " GROUP BY p.id HAVING total_stock <= p.reorder_level"
        " ORDER BY total_stock ASC LIMIT 50;";

    std::vector<std::string> rows;
    Stmt st(m_db, sql);
    if (!st.ok()) return "[]";
    while (st.step() == SQLITE_ROW) {
        rows.push_back(jsonutil::make_obj({
            {"id",            std::to_string((int)st.col_int64(0))},
            {"sku",           jsonutil::make_str(st.col_text(1))},
            {"name",          jsonutil::make_str(st.col_text(2))},
            {"reorder_level", std::to_string(st.col_int(3))},
            {"total_stock",   std::to_string((int)st.col_int64(4))},
        }));
    }
    return jsonutil::make_arr(rows);
}
