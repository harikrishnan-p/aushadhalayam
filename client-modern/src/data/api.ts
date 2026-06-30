/**
 * Data access layer.
 * Inside Tauri: calls invoke() which talks to the Rust backend.
 * In a plain browser (npm run dev without tauri): falls back to mock data.
 */

import { invoke as tauriInvoke } from "@tauri-apps/api/core";
import type {
  Product, StockBatch, LowStockAlert, Customer,
  PeriodSummary, Gstr1B2cRow,
} from "./types";
import * as mock from "./mock";

async function invoke<T>(cmd: string, args?: Record<string, unknown>): Promise<T> {
  return tauriInvoke<T>(cmd, args);
}

// ── Inventory ────────────────────────────────────────────────────────────────

export async function searchProducts(query: string): Promise<Product[]> {
  try {
    return await invoke<Product[]>("search_products", { query });
  } catch {
    return mock.products.filter(p =>
      p.name.toLowerCase().includes(query.toLowerCase()) ||
      (p.generic_name ?? "").toLowerCase().includes(query.toLowerCase())
    );
  }
}

export async function getStockBatches(productId: number): Promise<StockBatch[]> {
  try {
    return await invoke<StockBatch[]>("get_stock_batches", { product_id: productId });
  } catch {
    return mock.batches.filter(b => b.product_id === productId);
  }
}

export async function getLowStockAlerts(): Promise<LowStockAlert[]> {
  try {
    return await invoke<LowStockAlert[]>("get_low_stock_alerts");
  } catch {
    return mock.lowStockAlerts;
  }
}

// ── Billing ──────────────────────────────────────────────────────────────────

/** Shape must match Rust CheckoutRequest + CartItem exactly. */
export interface CheckoutPayload {
  customer_name?:    string;
  customer_id?:      number;
  payment_mode:      string;
  bill_discount_pct: number;
  is_interstate:     boolean;
  items: {
    product_id:   number;
    batch_id:     number;
    product_name: string;
    hsn_code:     string;
    batch_number: string;
    expiry_date:  string;
    quantity:     number;
    mrp:          number;
    discount_pct: number;
    gst_rate:     number;
  }[];
}

export interface BillResult {
  bill_id:         number;
  bill_number:     string;
  grand_total:     number;
  cgst_amount:     number;
  sgst_amount:     number;
  igst_amount:     number;
}

export async function processCheckout(payload: CheckoutPayload): Promise<BillResult> {
  try {
    return await invoke<BillResult>("process_checkout", { req: payload });
  } catch {
    return {
      bill_id:     Date.now(),
      bill_number: `MOCK-${Date.now()}`,
      grand_total: 0,
      cgst_amount: 0,
      sgst_amount: 0,
      igst_amount: 0,
    };
  }
}

export async function getPendingSyncCount(): Promise<number> {
  try {
    return await invoke<number>("get_pending_sync_count");
  } catch {
    return 0;
  }
}

// ── GST Reports ──────────────────────────────────────────────────────────────

export async function getPeriodSummary(month: string): Promise<PeriodSummary> {
  try {
    return await invoke<PeriodSummary>("get_period_summary", { month });
  } catch {
    return mock.periodSummary;
  }
}

export async function getGstr1B2cSummary(month: string): Promise<Gstr1B2cRow[]> {
  try {
    return await invoke<Gstr1B2cRow[]>("get_gstr1_b2c_summary", { month });
  } catch {
    return mock.gstr1Rows;
  }
}

// ── UI-only (mock) ───────────────────────────────────────────────────────────

export async function getCustomers(): Promise<Customer[]> {
  return mock.customers;
}

export async function searchCustomers(query: string): Promise<Customer[]> {
  return mock.customers.filter(c =>
    c.name.toLowerCase().includes(query.toLowerCase()) ||
    c.phone.includes(query)
  );
}
