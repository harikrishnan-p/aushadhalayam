#pragma once
// =============================================================================
// DatabaseManager.h  —  RAII wrappers for raw SQLite3 C API (C++11, no STL map)
//
// Designed for use inside a single thread only.  The worker thread owns one
// DatabaseManager instance; the GUI thread has none.
// =============================================================================

#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include "sqlite3.h"

// ─────────────────────────────────────────────────────────────────────────────
// SqliteStmt — RAII sqlite3_stmt handle
// ─────────────────────────────────────────────────────────────────────────────
class SqliteStmt {
public:
    SqliteStmt() : m_stmt(nullptr) {}
    ~SqliteStmt() { finalize(); }

    bool prepare(sqlite3* db, const char* sql) {
        finalize();
        return sqlite3_prepare_v2(db, sql, -1, &m_stmt, nullptr) == SQLITE_OK;
    }

    void finalize() {
        if (m_stmt) { sqlite3_finalize(m_stmt); m_stmt = nullptr; }
    }

    // Returns SQLITE_ROW, SQLITE_DONE, or an error code
    int step() { return sqlite3_step(m_stmt); }

    bool reset() { return m_stmt && (sqlite3_reset(m_stmt) == SQLITE_OK); }

    // Parameter binding (1-based index)
    bool bind_text  (int i, const std::string& v) { return sqlite3_bind_text  (m_stmt, i, v.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK; }
    bool bind_int   (int i, int v)                { return sqlite3_bind_int   (m_stmt, i, v)                                == SQLITE_OK; }
    bool bind_int64 (int i, sqlite3_int64 v)      { return sqlite3_bind_int64 (m_stmt, i, v)                                == SQLITE_OK; }
    bool bind_double(int i, double v)              { return sqlite3_bind_double(m_stmt, i, v)                                == SQLITE_OK; }
    bool bind_null  (int i)                        { return sqlite3_bind_null  (m_stmt, i)                                   == SQLITE_OK; }

    // Column accessors (0-based index)
    std::string    column_text  (int i) const {
        const unsigned char* t = sqlite3_column_text(m_stmt, i);
        return t ? reinterpret_cast<const char*>(t) : "";
    }
    int            column_int   (int i) const { return sqlite3_column_int   (m_stmt, i); }
    sqlite3_int64  column_int64 (int i) const { return sqlite3_column_int64 (m_stmt, i); }
    double         column_double(int i) const { return sqlite3_column_double(m_stmt, i); }
    int            column_count ()      const { return sqlite3_column_count (m_stmt);    }

    bool is_valid() const { return m_stmt != nullptr; }

private:
    sqlite3_stmt* m_stmt;
    SqliteStmt(const SqliteStmt&) = delete;
    SqliteStmt& operator=(const SqliteStmt&) = delete;
};

// ─────────────────────────────────────────────────────────────────────────────
// Row  —  lightweight key-value bag for query results
// ─────────────────────────────────────────────────────────────────────────────
struct DbRow {
    std::vector<std::string> columns;
    std::vector<std::string> values;

    const std::string& get(const std::string& col) const {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i] == col) return values[i];
        }
        static const std::string empty;
        return empty;
    }

    int    get_int   (const std::string& c) const { return std::stoi(get(c).empty() ? "0" : get(c)); }
    double get_double(const std::string& c) const { return std::stod(get(c).empty() ? "0" : get(c)); }
    long long get_int64(const std::string& c) const { return std::stoll(get(c).empty() ? "0" : get(c)); }
};

// ─────────────────────────────────────────────────────────────────────────────
// DatabaseManager  —  one per thread; opens its own SQLite connection
// ─────────────────────────────────────────────────────────────────────────────
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    bool open(const std::string& path);
    void close();
    bool is_open() const { return m_db != nullptr; }

    // Execute DDL/DML with no result rows; returns true on success
    bool exec(const char* sql);

    // Parameterized single-step execution (INSERT/UPDATE/DELETE)
    // cb receives a prepared statement with ? placeholders already set
    bool exec_stmt(const char* sql, std::function<void(SqliteStmt&)> bind_cb);

    // SELECT helper: calls row_cb once per result row
    bool query(const char* sql,
               std::function<void(SqliteStmt&)> bind_cb,
               std::function<void(const DbRow&)> row_cb);

    bool begin_immediate();
    bool commit();
    bool rollback();

    sqlite3_int64 last_insert_rowid() const {
        return m_db ? sqlite3_last_insert_rowid(m_db) : 0;
    }

    const char* errmsg() const {
        return m_db ? sqlite3_errmsg(m_db) : "db not open";
    }

    // Apply the schema.sql DDL (called once on first launch)
    bool apply_schema(const std::string& schema_sql);

    // Register this device in the devices table
    bool register_device(const std::string& device_id, const std::string& device_name);

    sqlite3* raw() { return m_db; }

private:
    sqlite3* m_db;

    bool configure_wal();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};
