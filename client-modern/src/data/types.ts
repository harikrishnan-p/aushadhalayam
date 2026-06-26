// Shared TypeScript types mirroring the Rust/SQLite schema.

export type Schedule = 0 | 1 | 2 | 3; // 0=OTC 1=H 2=H1 3=X

export interface Product {
  id: string;
  name: string;
  generic_name: string;
  manufacturer: string;
  hsn_code: string;
  gst_rate: number;
  is_scheduled: Schedule;
  reorder_level: number;
  unit: string;
}

export interface StockBatch {
  id: string;
  product_id: string;
  batch_no: string;
  expiry_date: string; // ISO date
  mrp: number;
  purchase_rate: number;
  quantity: number;
}

export interface LowStockAlert {
  product_id: string;
  product_name: string;
  current_stock: number;
  reorder_level: number;
}

export interface Customer {
  id: string;
  name: string;
  phone: string;
  address: string;
  gstin: string;
}

export interface BillItem {
  product_id: string;
  product_name: string;
  is_scheduled: Schedule;
  batch_id: string;
  batch_no: string;
  expiry_date: string;
  mrp: number;
  quantity: number;
  discount_pct: number;
  gst_rate: number;
  hsn_code: string;
  // computed client-side:
  available_batches: StockBatch[];
}

export interface PeriodSummary {
  total_sales: number;
  total_bills: number;
  total_gst: number;
  avg_bill: number;
  sales_by_day: { date: string; amount: number }[];
}

export interface Gstr1B2cRow {
  month: string;
  hsn_code: string;
  gst_rate: number;
  taxable_value: number;
  igst: number;
  cgst: number;
  sgst: number;
}
