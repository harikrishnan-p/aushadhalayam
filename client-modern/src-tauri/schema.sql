-- =============================================================================
-- AUSHADHALAYAM PHARMACY POS  —  Ground-Truth SQLite Schema
-- Version    : 1.0.0
-- Engine     : SQLite 3.35+ (WAL mode, JSON1 extension required)
-- Shared by  : client-legacy (wxWidgets/C++) and client-modern (Tauri/Rust)
--
-- LWW (Last-Writer-Wins) Strategy
-- ─────────────────────────────────
-- Every business table carries three replication columns:
--   updated_at_us  INTEGER  microseconds since Unix epoch (tie-breaks writes)
--   device_id      TEXT     identifies the origin POS terminal
--   row_version    INTEGER  per-row monotonic counter (incremented on each UPDATE)
--
-- At the PostgreSQL merge layer, conflicts are resolved as:
--   1. Higher updated_at_us wins.
--   2. Equal timestamps (clock skew): higher row_version wins.
--   3. Equal row_version: device_id is used as a deterministic tiebreaker (lexicographic).
--
-- Sync Outbox
-- ──────────────
-- Mutations are captured atomically by AFTER triggers that INSERT a JSON
-- snapshot into sync_outbox in the same SQLite transaction.  The outbox is
-- append-only; the sync agent marks rows is_transmitted=1 after successful
-- delivery and never deletes them (audit trail).
-- =============================================================================

PRAGMA journal_mode  = WAL;          -- concurrent readers during writes
PRAGMA synchronous   = NORMAL;       -- crash-safe under WAL, faster than FULL
PRAGMA foreign_keys  = ON;
PRAGMA temp_store    = MEMORY;
PRAGMA cache_size    = -8000;        -- ~8 MB page cache

-- ═════════════════════════════════════════════════════════════════════════════
-- UTILITY EXPRESSION  (used in DEFAULT clauses throughout)
-- CAST((julianday('now') - 2440587.5) * 86400.0 * 1e6 AS INTEGER)
-- evaluates to INTEGER microseconds since 1970-01-01T00:00:00Z.
-- ═════════════════════════════════════════════════════════════════════════════

-- ─────────────────────────────────────────────────────────────────────────────
-- 1.  REFERENCE TABLES
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS categories (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    parent_id   INTEGER REFERENCES categories(id) ON DELETE SET NULL,
    -- LWW columns
    updated_at_us  INTEGER NOT NULL DEFAULT (CAST((julianday('now') - 2440587.5)*86400.0*1e6 AS INTEGER)),
    device_id      TEXT    NOT NULL DEFAULT '',
    row_version    INTEGER NOT NULL DEFAULT 1,
    is_deleted     INTEGER NOT NULL DEFAULT 0 CHECK (is_deleted IN (0,1))
);

CREATE TABLE IF NOT EXISTS suppliers (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    name                 TEXT    NOT NULL COLLATE NOCASE,
    gstin                TEXT,
    drug_license_number  TEXT,
    contact_person       TEXT,
    phone                TEXT,
    email                TEXT    COLLATE NOCASE,
    address              TEXT,
    -- LWW columns
    updated_at_us  INTEGER NOT NULL DEFAULT (CAST((julianday('now') - 2440587.5)*86400.0*1e6 AS INTEGER)),
    device_id      TEXT    NOT NULL DEFAULT '',
    row_version    INTEGER NOT NULL DEFAULT 1,
    is_deleted     INTEGER NOT NULL DEFAULT 0 CHECK (is_deleted IN (0,1))
);

CREATE TABLE IF NOT EXISTS customers (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT    NOT NULL COLLATE NOCASE,
    phone           TEXT    UNIQUE,
    email           TEXT    COLLATE NOCASE,
    address         TEXT,
    gstin           TEXT,           -- for B2B tax invoices
    loyalty_points  INTEGER NOT NULL DEFAULT 0 CHECK (loyalty_points >= 0),
    -- LWW columns
    updated_at_us  INTEGER NOT NULL DEFAULT (CAST((julianday('now') - 2440587.5)*86400.0*1e6 AS INTEGER)),
    device_id      TEXT    NOT NULL DEFAULT '',
    row_version    INTEGER NOT NULL DEFAULT 1,
    is_deleted     INTEGER NOT NULL DEFAULT 0 CHECK (is_deleted IN (0,1))
);

-- ─────────────────────────────────────────────────────────────────────────────
-- 2.  PRODUCT CATALOGUE
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS products (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    sku             TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    name            TEXT    NOT NULL COLLATE NOCASE,
    generic_name    TEXT    COLLATE NOCASE,
    manufacturer    TEXT,
    category_id     INTEGER REFERENCES categories(id) ON DELETE SET NULL,
    hsn_code        TEXT    NOT NULL,               -- GST HSN/SAC code
    gst_rate        REAL    NOT NULL DEFAULT 12.0   -- valid Indian pharma GST slabs
                    CHECK (gst_rate IN (0.0, 0.1, 0.25, 1.5, 3.0, 5.0, 12.0, 18.0, 28.0)),
    unit            TEXT    NOT NULL DEFAULT 'Strip',
    pack_size       INTEGER NOT NULL DEFAULT 1 CHECK (pack_size > 0),
    mrp             REAL    NOT NULL CHECK (mrp > 0),
    purchase_price  REAL    NOT NULL CHECK (purchase_price > 0),
    reorder_level   INTEGER NOT NULL DEFAULT 10 CHECK (reorder_level >= 0),
    -- 0=OTC  1=Schedule H  2=Schedule H1  3=Schedule X
    is_scheduled    INTEGER NOT NULL DEFAULT 0 CHECK (is_scheduled IN (0,1,2,3)),
    -- LWW columns
    updated_at_us  INTEGER NOT NULL DEFAULT (CAST((julianday('now') - 2440587.5)*86400.0*1e6 AS INTEGER)),
    device_id      TEXT    NOT NULL DEFAULT '',
    row_version    INTEGER NOT NULL DEFAULT 1,
    is_deleted     INTEGER NOT NULL DEFAULT 0 CHECK (is_deleted IN (0,1))
);

-- One product can have multiple concurrent stock batches (FEFO dispatch)
CREATE TABLE IF NOT EXISTS stock_batches (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    product_id      INTEGER NOT NULL REFERENCES products(id) ON DELETE CASCADE,
    batch_number    TEXT    NOT NULL COLLATE NOCASE,
    expiry_date     TEXT    NOT NULL,               -- YYYY-MM (ISO 8601 partial date)
    quantity        INTEGER NOT NULL DEFAULT 0 CHECK (quantity >= 0),
    purchase_price  REAL    NOT NULL CHECK (purchase_price > 0),
    mrp             REAL    NOT NULL CHECK (mrp > 0),
    supplier_id     INTEGER REFERENCES suppliers(id) ON DELETE SET NULL,
    received_date   TEXT    NOT NULL DEFAULT (date('now')),
    -- LWW columns
    updated_at_us  INTEGER NOT NULL DEFAULT (CAST((julianday('now') - 2440587.5)*86400.0*1e6 AS INTEGER)),
    device_id      TEXT    NOT NULL DEFAULT '',
    row_version    INTEGER NOT NULL DEFAULT 1,
    is_deleted     INTEGER NOT NULL DEFAULT 0 CHECK (is_deleted IN (0,1)),
    UNIQUE (product_id, batch_number)
);

-- ─────────────────────────────────────────────────────────────────────────────
-- 3.  BILLING
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS bills (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    bill_number      TEXT    NOT NULL UNIQUE,            -- BILL-YYYYMMDD-NNNN
    bill_date        TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%S','now')),
    customer_id      INTEGER REFERENCES customers(id) ON DELETE SET NULL,
    customer_name    TEXT,                               -- denormalized for walk-in
    total_amount     REAL    NOT NULL CHECK (total_amount >= 0),
    discount_amount  REAL    NOT NULL DEFAULT 0 CHECK (discount_amount >= 0),
    taxable_amount   REAL    NOT NULL CHECK (taxable_amount >= 0),
    cgst_amount      REAL    NOT NULL DEFAULT 0,
    sgst_amount      REAL    NOT NULL DEFAULT 0,
    igst_amount      REAL    NOT NULL DEFAULT 0,
    total_gst_amount REAL    NOT NULL DEFAULT 0,
    grand_total      REAL    NOT NULL CHECK (grand_total >= 0),
    payment_mode     TEXT    NOT NULL DEFAULT 'CASH'
                     CHECK (payment_mode IN ('CASH','UPI','CARD','CREDIT','CHEQUE')),
    payment_ref      TEXT,
    is_cancelled     INTEGER NOT NULL DEFAULT 0 CHECK (is_cancelled IN (0,1)),
    cancel_reason    TEXT,
    -- LWW columns
    updated_at_us  INTEGER NOT NULL DEFAULT (CAST((julianday('now') - 2440587.5)*86400.0*1e6 AS INTEGER)),
    device_id      TEXT    NOT NULL DEFAULT '',
    row_version    INTEGER NOT NULL DEFAULT 1,
    is_deleted     INTEGER NOT NULL DEFAULT 0 CHECK (is_deleted IN (0,1))
);

CREATE TABLE IF NOT EXISTS bill_items (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    bill_id         INTEGER NOT NULL REFERENCES bills(id) ON DELETE CASCADE,
    product_id      INTEGER NOT NULL REFERENCES products(id),
    batch_id        INTEGER NOT NULL REFERENCES stock_batches(id),
    -- Snapshot values at moment of sale (price/GST may change later)
    product_name    TEXT    NOT NULL,
    hsn_code        TEXT    NOT NULL,
    batch_number    TEXT    NOT NULL,
    expiry_date     TEXT    NOT NULL,
    quantity        INTEGER NOT NULL CHECK (quantity > 0),
    mrp             REAL    NOT NULL CHECK (mrp > 0),
    discount_pct    REAL    NOT NULL DEFAULT 0 CHECK (discount_pct BETWEEN 0 AND 100),
    unit_price      REAL    NOT NULL,       -- mrp × (1 − discount_pct/100); GST-inclusive
    gst_rate        REAL    NOT NULL,
    -- Indian retail: MRP is GST-inclusive. taxable_value = line_total / (1 + gst_rate/100)
    taxable_value   REAL    NOT NULL,
    cgst_amount     REAL    NOT NULL DEFAULT 0,
    sgst_amount     REAL    NOT NULL DEFAULT 0,
    igst_amount     REAL    NOT NULL DEFAULT 0,
    line_total      REAL    NOT NULL,
    -- LWW columns
    updated_at_us  INTEGER NOT NULL DEFAULT (CAST((julianday('now') - 2440587.5)*86400.0*1e6 AS INTEGER)),
    device_id      TEXT    NOT NULL DEFAULT '',
    row_version    INTEGER NOT NULL DEFAULT 1,
    is_deleted     INTEGER NOT NULL DEFAULT 0 CHECK (is_deleted IN (0,1))
);

-- ─────────────────────────────────────────────────────────────────────────────
-- 4.  SYNC INFRASTRUCTURE
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS devices (
    device_id    TEXT    PRIMARY KEY,
    device_name  TEXT    NOT NULL,
    last_sync_us INTEGER,
    is_active    INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    created_at   TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%S','now'))
);

-- Append-only replication queue.  Written exclusively by triggers.
-- The sync agent reads WHERE is_transmitted=0, ships to PostgreSQL, then
-- sets is_transmitted=1.  Rows are NEVER deleted (audit trail).
CREATE TABLE IF NOT EXISTS sync_outbox (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    table_name       TEXT    NOT NULL,
    operation        TEXT    NOT NULL CHECK (operation IN ('INSERT','UPDATE','DELETE')),
    row_pk           TEXT    NOT NULL,   -- JSON: {"id":42}
    payload          TEXT    NOT NULL,   -- JSON: full row snapshot
    created_at_us    INTEGER NOT NULL DEFAULT (CAST((julianday('now') - 2440587.5)*86400.0*1e6 AS INTEGER)),
    device_id        TEXT    NOT NULL DEFAULT '',
    is_transmitted   INTEGER NOT NULL DEFAULT 0 CHECK (is_transmitted IN (0,1)),
    transmitted_at_us INTEGER,
    retry_count      INTEGER NOT NULL DEFAULT 0,
    error_message    TEXT
);

CREATE INDEX IF NOT EXISTS idx_sync_outbox_pending
    ON sync_outbox (is_transmitted, created_at_us)
    WHERE is_transmitted = 0;

-- GST export audit
CREATE TABLE IF NOT EXISTS gstr1_exports (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    period        TEXT    NOT NULL,         -- MMYYYY
    exported_at   TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%S','now')),
    row_count     INTEGER NOT NULL DEFAULT 0,
    file_path     TEXT,
    status        TEXT    NOT NULL DEFAULT 'PENDING'
                  CHECK (status IN ('PENDING','COMPLETE','ERROR')),
    error_message TEXT
);

-- ─────────────────────────────────────────────────────────────────────────────
-- 5.  PERFORMANCE INDEXES
-- ─────────────────────────────────────────────────────────────────────────────

CREATE INDEX IF NOT EXISTS idx_products_name    ON products (name);
CREATE INDEX IF NOT EXISTS idx_products_hsn     ON products (hsn_code);
CREATE INDEX IF NOT EXISTS idx_stock_product    ON stock_batches (product_id, expiry_date);
CREATE INDEX IF NOT EXISTS idx_bills_date       ON bills (bill_date);
CREATE INDEX IF NOT EXISTS idx_bills_customer   ON bills (customer_id);
CREATE INDEX IF NOT EXISTS idx_bill_items_bill  ON bill_items (bill_id);

-- ─────────────────────────────────────────────────────────────────────────────
-- 6.  SYNC OUTBOX TRIGGERS
--
-- Each AFTER trigger fires inside the same transaction that mutated the row.
-- If the outer transaction rolls back, the sync_outbox INSERT also rolls back —
-- guaranteeing the outbox never contains data that was not committed to the DB.
-- ─────────────────────────────────────────────────────────────────────────────

-- ── products ─────────────────────────────────────────────────────────────────

CREATE TRIGGER IF NOT EXISTS trg_products_insert AFTER INSERT ON products BEGIN
    INSERT INTO sync_outbox(table_name,operation,row_pk,payload,device_id) VALUES(
        'products','INSERT',
        json_object('id',NEW.id),
        json_object(
            'id',NEW.id,'sku',NEW.sku,'name',NEW.name,'generic_name',NEW.generic_name,
            'hsn_code',NEW.hsn_code,'gst_rate',NEW.gst_rate,'unit',NEW.unit,
            'pack_size',NEW.pack_size,'mrp',NEW.mrp,'purchase_price',NEW.purchase_price,
            'reorder_level',NEW.reorder_level,'is_scheduled',NEW.is_scheduled,
            'updated_at_us',NEW.updated_at_us,'device_id',NEW.device_id,
            'row_version',NEW.row_version,'is_deleted',NEW.is_deleted),
        NEW.device_id);
END;

CREATE TRIGGER IF NOT EXISTS trg_products_update AFTER UPDATE ON products BEGIN
    INSERT INTO sync_outbox(table_name,operation,row_pk,payload,device_id) VALUES(
        'products','UPDATE',
        json_object('id',NEW.id),
        json_object(
            'id',NEW.id,'sku',NEW.sku,'name',NEW.name,'generic_name',NEW.generic_name,
            'hsn_code',NEW.hsn_code,'gst_rate',NEW.gst_rate,'unit',NEW.unit,
            'pack_size',NEW.pack_size,'mrp',NEW.mrp,'purchase_price',NEW.purchase_price,
            'reorder_level',NEW.reorder_level,'is_scheduled',NEW.is_scheduled,
            'updated_at_us',NEW.updated_at_us,'device_id',NEW.device_id,
            'row_version',NEW.row_version,'is_deleted',NEW.is_deleted),
        NEW.device_id);
END;

-- ── stock_batches ─────────────────────────────────────────────────────────────

CREATE TRIGGER IF NOT EXISTS trg_batches_insert AFTER INSERT ON stock_batches BEGIN
    INSERT INTO sync_outbox(table_name,operation,row_pk,payload,device_id) VALUES(
        'stock_batches','INSERT',
        json_object('id',NEW.id),
        json_object(
            'id',NEW.id,'product_id',NEW.product_id,'batch_number',NEW.batch_number,
            'expiry_date',NEW.expiry_date,'quantity',NEW.quantity,
            'purchase_price',NEW.purchase_price,'mrp',NEW.mrp,
            'supplier_id',NEW.supplier_id,'received_date',NEW.received_date,
            'updated_at_us',NEW.updated_at_us,'device_id',NEW.device_id,
            'row_version',NEW.row_version,'is_deleted',NEW.is_deleted),
        NEW.device_id);
END;

CREATE TRIGGER IF NOT EXISTS trg_batches_update AFTER UPDATE ON stock_batches BEGIN
    INSERT INTO sync_outbox(table_name,operation,row_pk,payload,device_id) VALUES(
        'stock_batches','UPDATE',
        json_object('id',NEW.id),
        json_object(
            'id',NEW.id,'product_id',NEW.product_id,'batch_number',NEW.batch_number,
            'expiry_date',NEW.expiry_date,'quantity',NEW.quantity,
            'purchase_price',NEW.purchase_price,'mrp',NEW.mrp,
            'supplier_id',NEW.supplier_id,'received_date',NEW.received_date,
            'updated_at_us',NEW.updated_at_us,'device_id',NEW.device_id,
            'row_version',NEW.row_version,'is_deleted',NEW.is_deleted),
        NEW.device_id);
END;

-- ── bills ─────────────────────────────────────────────────────────────────────

CREATE TRIGGER IF NOT EXISTS trg_bills_insert AFTER INSERT ON bills BEGIN
    INSERT INTO sync_outbox(table_name,operation,row_pk,payload,device_id) VALUES(
        'bills','INSERT',
        json_object('id',NEW.id),
        json_object(
            'id',NEW.id,'bill_number',NEW.bill_number,'bill_date',NEW.bill_date,
            'customer_id',NEW.customer_id,'customer_name',NEW.customer_name,
            'total_amount',NEW.total_amount,'discount_amount',NEW.discount_amount,
            'taxable_amount',NEW.taxable_amount,'cgst_amount',NEW.cgst_amount,
            'sgst_amount',NEW.sgst_amount,'igst_amount',NEW.igst_amount,
            'total_gst_amount',NEW.total_gst_amount,'grand_total',NEW.grand_total,
            'payment_mode',NEW.payment_mode,'payment_ref',NEW.payment_ref,
            'is_cancelled',NEW.is_cancelled,'updated_at_us',NEW.updated_at_us,
            'device_id',NEW.device_id,'row_version',NEW.row_version,'is_deleted',NEW.is_deleted),
        NEW.device_id);
END;

CREATE TRIGGER IF NOT EXISTS trg_bills_update AFTER UPDATE ON bills BEGIN
    INSERT INTO sync_outbox(table_name,operation,row_pk,payload,device_id) VALUES(
        'bills','UPDATE',
        json_object('id',NEW.id),
        json_object(
            'id',NEW.id,'bill_number',NEW.bill_number,'bill_date',NEW.bill_date,
            'customer_id',NEW.customer_id,'customer_name',NEW.customer_name,
            'total_amount',NEW.total_amount,'discount_amount',NEW.discount_amount,
            'taxable_amount',NEW.taxable_amount,'cgst_amount',NEW.cgst_amount,
            'sgst_amount',NEW.sgst_amount,'igst_amount',NEW.igst_amount,
            'total_gst_amount',NEW.total_gst_amount,'grand_total',NEW.grand_total,
            'payment_mode',NEW.payment_mode,'payment_ref',NEW.payment_ref,
            'is_cancelled',NEW.is_cancelled,'updated_at_us',NEW.updated_at_us,
            'device_id',NEW.device_id,'row_version',NEW.row_version,'is_deleted',NEW.is_deleted),
        NEW.device_id);
END;

-- ── bill_items ────────────────────────────────────────────────────────────────

CREATE TRIGGER IF NOT EXISTS trg_bill_items_insert AFTER INSERT ON bill_items BEGIN
    INSERT INTO sync_outbox(table_name,operation,row_pk,payload,device_id) VALUES(
        'bill_items','INSERT',
        json_object('id',NEW.id),
        json_object(
            'id',NEW.id,'bill_id',NEW.bill_id,'product_id',NEW.product_id,
            'batch_id',NEW.batch_id,'product_name',NEW.product_name,
            'hsn_code',NEW.hsn_code,'batch_number',NEW.batch_number,
            'expiry_date',NEW.expiry_date,'quantity',NEW.quantity,'mrp',NEW.mrp,
            'discount_pct',NEW.discount_pct,'unit_price',NEW.unit_price,
            'gst_rate',NEW.gst_rate,'taxable_value',NEW.taxable_value,
            'cgst_amount',NEW.cgst_amount,'sgst_amount',NEW.sgst_amount,
            'igst_amount',NEW.igst_amount,'line_total',NEW.line_total,
            'updated_at_us',NEW.updated_at_us,'device_id',NEW.device_id,
            'row_version',NEW.row_version,'is_deleted',NEW.is_deleted),
        NEW.device_id);
END;

-- ─────────────────────────────────────────────────────────────────────────────
-- 7.  REPORTING VIEWS
-- ─────────────────────────────────────────────────────────────────────────────

-- Per-bill GST breakdown keyed by HSN code (feed into GSTR-1)
CREATE VIEW IF NOT EXISTS v_bill_gst_summary AS
SELECT
    b.id                        AS bill_id,
    b.bill_number,
    b.bill_date,
    bi.hsn_code,
    bi.gst_rate,
    SUM(bi.taxable_value)       AS taxable_value,
    SUM(bi.cgst_amount)         AS cgst,
    SUM(bi.sgst_amount)         AS sgst,
    SUM(bi.igst_amount)         AS igst,
    SUM(bi.line_total)          AS line_total
FROM bills b
JOIN bill_items bi ON bi.bill_id = b.id
WHERE b.is_cancelled = 0 AND bi.is_deleted = 0
GROUP BY b.id, bi.hsn_code, bi.gst_rate;

-- GSTR-1 B2C aggregate (HSN-wise monthly summary)
CREATE VIEW IF NOT EXISTS v_gstr1_b2c AS
SELECT
    strftime('%m', b.bill_date)   AS month,
    strftime('%Y', b.bill_date)   AS year,
    bi.hsn_code,
    bi.gst_rate,
    SUM(bi.taxable_value)         AS taxable_value,
    SUM(bi.cgst_amount)           AS total_cgst,
    SUM(bi.sgst_amount)           AS total_sgst,
    SUM(bi.igst_amount)           AS total_igst,
    COUNT(DISTINCT b.id)          AS invoice_count
FROM bills b
JOIN bill_items bi ON bi.bill_id = b.id
WHERE b.is_cancelled = 0 AND b.customer_id IS NULL AND bi.is_deleted = 0
GROUP BY strftime('%m%Y', b.bill_date), bi.hsn_code, bi.gst_rate;

-- Low-stock alert: products whose live stock <= reorder_level
CREATE VIEW IF NOT EXISTS v_low_stock AS
SELECT
    p.id,
    p.sku,
    p.name,
    p.reorder_level,
    COALESCE(SUM(sb.quantity), 0)  AS total_stock,
    p.hsn_code,
    p.gst_rate
FROM products p
LEFT JOIN stock_batches sb
       ON sb.product_id = p.id
      AND sb.is_deleted = 0
      AND sb.expiry_date > strftime('%Y-%m','now')
WHERE p.is_deleted = 0
GROUP BY p.id
HAVING total_stock <= p.reorder_level;

-- =============================================================================
-- LWW MERGE NOTES FOR POSTGRESQL LAYER
-- =============================================================================
-- When an inbound sync_outbox payload arrives at the PostgreSQL upsert layer:
--
--   INSERT INTO products (...)
--   VALUES (%(payload)s)
--   ON CONFLICT (id) DO UPDATE SET
--       sku            = EXCLUDED.sku,
--       ...all business columns...,
--       updated_at_us  = EXCLUDED.updated_at_us,
--       device_id      = EXCLUDED.device_id,
--       row_version    = EXCLUDED.row_version,
--       is_deleted     = EXCLUDED.is_deleted
--   WHERE
--       -- Accept the incoming row only if it is strictly newer
--       EXCLUDED.updated_at_us > products.updated_at_us
--       OR (EXCLUDED.updated_at_us = products.updated_at_us
--           AND EXCLUDED.row_version > products.row_version)
--       OR (EXCLUDED.updated_at_us = products.updated_at_us
--           AND EXCLUDED.row_version = products.row_version
--           AND EXCLUDED.device_id  > products.device_id);  -- lexicographic tiebreak
--
-- This WHERE clause means "only overwrite if the incoming row wins the LWW race."
-- =============================================================================
