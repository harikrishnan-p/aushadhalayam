// =============================================================================
// DatabaseManager.cpp
// =============================================================================

#include "DatabaseManager.h"
#include "sqlite3.h"
#include <stdexcept>
#include <cstdio>

DatabaseManager::DatabaseManager() : m_db(nullptr) {}

DatabaseManager::~DatabaseManager() { close(); }

bool DatabaseManager::open(const std::string& path) {
    close();
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
    if (sqlite3_open_v2(path.c_str(), &m_db, flags, nullptr) != SQLITE_OK) {
        m_db = nullptr;
        return false;
    }
    return configure_wal();
}

void DatabaseManager::close() {
    if (m_db) {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
}

bool DatabaseManager::configure_wal() {
    // Enable WAL mode for concurrent read/write access
    const char* pragmas =
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=NORMAL;"
        "PRAGMA foreign_keys=ON;"
        "PRAGMA temp_store=MEMORY;"
        "PRAGMA cache_size=-8000;"
        "PRAGMA busy_timeout=5000;";    // wait up to 5s on a locked DB
    return exec(pragmas);
}

bool DatabaseManager::exec(const char* sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    return rc == SQLITE_OK;
}

bool DatabaseManager::exec_stmt(const char* sql,
                                std::function<void(SqliteStmt&)> bind_cb) {
    SqliteStmt stmt;
    if (!stmt.prepare(m_db, sql)) return false;
    bind_cb(stmt);
    int rc = stmt.step();
    return (rc == SQLITE_DONE || rc == SQLITE_ROW);
}

bool DatabaseManager::query(const char* sql,
                            std::function<void(SqliteStmt&)> bind_cb,
                            std::function<void(const DbRow&)> row_cb) {
    SqliteStmt stmt;
    if (!stmt.prepare(m_db, sql)) return false;
    bind_cb(stmt);

    const int ncols = stmt.column_count();
    while (stmt.step() == SQLITE_ROW) {
        DbRow row;
        for (int i = 0; i < ncols; ++i) {
            const char* name = sqlite3_column_name(m_db ? nullptr : nullptr, i);
            // column_name is on the raw stmt, not via our wrapper
            const char* cname = sqlite3_column_name(
                reinterpret_cast<sqlite3_stmt*>(stmt.is_valid() ? const_cast<SqliteStmt*>(&stmt) : nullptr), i);
            (void)cname; // workaround: access raw handle via member
            row.values.push_back(stmt.column_text(i));
        }
        row_cb(row);
    }
    return true;
}

bool DatabaseManager::begin_immediate() {
    return exec("BEGIN IMMEDIATE;");
}

bool DatabaseManager::commit() {
    return exec("COMMIT;");
}

bool DatabaseManager::rollback() {
    return exec("ROLLBACK;");
}

bool DatabaseManager::apply_schema(const std::string& schema_sql) {
    return exec(schema_sql.c_str());
}

bool DatabaseManager::register_device(const std::string& device_id,
                                      const std::string& device_name) {
    const char* sql =
        "INSERT OR IGNORE INTO devices(device_id, device_name) VALUES(?,?);";
    return exec_stmt(sql, [&](SqliteStmt& s) {
        s.bind_text(1, device_id);
        s.bind_text(2, device_name);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Query helper overload that passes column names in the row
// (Replace the query() above with this corrected version)
// ─────────────────────────────────────────────────────────────────────────────
// We need to expose the raw stmt pointer for column_name().
// Restructure: the lambda receives a SqliteStmt* directly so callers can use
// the sqlite3 C API for column names when needed.
