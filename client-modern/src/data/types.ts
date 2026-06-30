// Shared TypeScript types — field names and numeric types mirror the Rust DTOs
// in src-tauri/src/commands/inventory.rs and billing.rs exactly so that Tauri
// IPC deserialisation never silently coerces or drops values.

export type Schedule = 0 | 1 | 2 | 3; // 0=OTC 1=H 2=H1 3=X

export interface Product {
  id:            number;
  sku:           string;
  name:          string;
  generic_name:  string | null;
  manufacturer:  string;
  hsn_code:      string;
  gst_rate:      number;
  is_scheduled:  Schedule;
  unit:          string;
  mrp:           number;
  reorder_level: number;
  total_qty:     number;
}

export interface StockBatch {
  id:             number;
  batch_number:   string;   // was batch_no — matches Rust StockBatch.batch_number
  expiry_date:    string;   // ISO date YYYY-MM-DD
  mrp:            number;
  purchase_price: number;   // was purchase_rate
  quantity:       number;
}

export interface LowStockAlert {
  id:            number;   // was product_id
  sku:           string;
  name:          string;   // was product_name
  reorder_level: number;
  total_stock:   number;   // was current_stock
}

export interface Customer {
  id:      string;
  name:    string;
  phone:   string;
  address: string;
  gstin:   string;
}

export interface BillItem {
  product_id:       number;
  product_name:     string;
  is_scheduled:     Schedule;
  batch_id:         number;
  batch_number:     string;   // was batch_no
  expiry_date:      string;
  mrp:              number;
  quantity:         number;
  discount_pct:     number;
  gst_rate:         number;
  hsn_code:         string;
  available_batches: StockBatch[];
}

export interface PeriodSummary {
  total_sales:   number;
  total_bills:   number;
  total_gst:     number;
  avg_bill:      number;
  sales_by_day:  { date: string; amount: number }[];
}

export interface Gstr1B2cRow {
  month:         string;
  hsn_code:      string;
  gst_rate:      number;
  taxable_value: number;
  igst:          number;
  cgst:          number;
  sgst:          number;
}
