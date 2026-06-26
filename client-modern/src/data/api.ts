/**
 * Data access layer.
 * When running inside Tauri, calls `invoke` for supported commands.
 * Otherwise (browser dev / unsupported commands) falls back to mock data.
 *
 * Commands backed by Rust:  search_products, get_stock_batches,
 *   get_low_stock_alerts, process_checkout, cancel_bill,
 *   get_pending_sync_count, get_gstr1_b2c_summary,
 *   get_period_summary, get_gstr1_bill_details
 *
 * UI-only (mock only):  Customer CRUD, Purchase Orders, Doctors, Settings persistence
 */

import type {
  Product, StockBatch, LowStockAlert, Customer,
  PeriodSummary, Gstr1B2cRow,
} from "./types";
import * as mock from "./mock";

// Tauri invoke — exists only inside the desktop shell.
async function invoke<T>(cmd: string, args?: unknown): Promise<T> {
  const w = window as unknown as { __TAURI__?: { core: { invoke: (c: string, a?: unknown) => Promise<T> } } };
  if (w.__TAURI__?.core?.invoke) {
    return w.__TAURI__.core.invoke(cmd, args);
  }
  throw new Error(`tauri_unavailable:${cmd}`);
}

export async function searchProducts(query: string): Promise<Product[]> {
  try {
    return await invoke<Product[]>("search_products", { query });
  } catch {
    return mock.products.filter(p =>
      p.name.toLowerCase().includes(query.toLowerCase()) ||
      p.generic_name.toLowerCase().includes(query.toLowerCase())
    );
  }
}

export async function getStockBatches(productId: string): Promise<StockBatch[]> {
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

export interface CheckoutPayload {
  customer_id?: string;
  doctor_name?: string;
  prescription_no?: string;
  notes?: string;
  payment_mode: string;
  items: {
    product_id: string;
    batch_id: string;
    quantity: number;
    discount_pct: number;
  }[];
}

export async function processCheckout(payload: CheckoutPayload): Promise<{ bill_id: string }> {
  try {
    return await invoke<{ bill_id: string }>("process_checkout", payload);
  } catch {
    return { bill_id: `MOCK-${Date.now()}` };
  }
}

export async function getPendingSyncCount(): Promise<number> {
  try {
    return await invoke<number>("get_pending_sync_count");
  } catch {
    return 0;
  }
}

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
