// Mock data — field names and types must mirror the TypeScript DTOs in types.ts
// so that the mock fallback path is structurally identical to the live Rust path.

import type { Product, StockBatch, LowStockAlert, Customer, PeriodSummary, Gstr1B2cRow } from "./types";

export const products: (Product & { product_id?: number })[] = [
  { id: 1, sku: "PARA500", name: "Paracetamol 500mg",  generic_name: "Paracetamol", manufacturer: "Cipla",      hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, unit: "strip", mrp: 25.00, reorder_level: 50, total_qty: 200 },
  { id: 2, sku: "AMOX250", name: "Amoxicillin 250mg",  generic_name: "Amoxicillin",  manufacturer: "Sun Pharma", hsn_code: "30041090", gst_rate: 12, is_scheduled: 1, unit: "strip", mrp: 85.00, reorder_level: 30, total_qty: 45  },
  { id: 3, sku: "ALPR025", name: "Alprazolam 0.25mg",  generic_name: "Alprazolam",   manufacturer: "Torrent",    hsn_code: "30049019", gst_rate: 12, is_scheduled: 3, unit: "strip", mrp: 45.00, reorder_level: 10, total_qty: 8   },
  { id: 4, sku: "PREG075", name: "Pregabalin 75mg",    generic_name: "Pregabalin",   manufacturer: "Dr. Reddy's",hsn_code: "30042090", gst_rate: 12, is_scheduled: 2, unit: "strip", mrp: 120.00,reorder_level: 20, total_qty: 12  },
  { id: 5, sku: "ATOR010", name: "Atorvastatin 10mg",  generic_name: "Atorvastatin", manufacturer: "Cipla",      hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, unit: "strip", mrp: 55.00, reorder_level: 40, total_qty: 90  },
  { id: 6, sku: "METF500", name: "Metformin 500mg",    generic_name: "Metformin",    manufacturer: "USV",        hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, unit: "strip", mrp: 40.00, reorder_level: 60, total_qty: 150 },
  { id: 7, sku: "CETI010", name: "Cetirizine 10mg",    generic_name: "Cetirizine",   manufacturer: "Mankind",    hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, unit: "strip", mrp: 30.00, reorder_level: 30, total_qty: 60  },
  { id: 8, sku: "OMEP020", name: "Omeprazole 20mg",    generic_name: "Omeprazole",   manufacturer: "Intas",      hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, unit: "strip", mrp: 65.00, reorder_level: 40, total_qty: 75  },
];

// product_id is extra on the mock to allow Inventory.tsx to join batches → products
export const batches: (StockBatch & { product_id: number })[] = [
  { id: 1, product_id: 1, batch_number: "BTH001", expiry_date: "2026-06-30", mrp: 25.00, purchase_price: 18.00, quantity: 120 },
  { id: 2, product_id: 1, batch_number: "BTH002", expiry_date: "2027-03-31", mrp: 26.00, purchase_price: 19.00, quantity: 80  },
  { id: 3, product_id: 2, batch_number: "BTH003", expiry_date: "2026-12-31", mrp: 85.00, purchase_price: 60.00, quantity: 45  },
  { id: 4, product_id: 3, batch_number: "BTH004", expiry_date: "2026-09-30", mrp: 45.00, purchase_price: 30.00, quantity: 30  },
  { id: 5, product_id: 4, batch_number: "BTH005", expiry_date: "2027-01-31", mrp: 120.00,purchase_price: 85.00, quantity: 25  },
  { id: 6, product_id: 5, batch_number: "BTH006", expiry_date: "2026-11-30", mrp: 55.00, purchase_price: 38.00, quantity: 90  },
  { id: 7, product_id: 6, batch_number: "BTH007", expiry_date: "2027-06-30", mrp: 40.00, purchase_price: 28.00, quantity: 150 },
  { id: 8, product_id: 7, batch_number: "BTH008", expiry_date: "2026-08-31", mrp: 30.00, purchase_price: 20.00, quantity: 60  },
  { id: 9, product_id: 8, batch_number: "BTH009", expiry_date: "2027-04-30", mrp: 65.00, purchase_price: 45.00, quantity: 75  },
];

export const lowStockAlerts: LowStockAlert[] = [
  { id: 3, sku: "ALPR025", name: "Alprazolam 0.25mg", total_stock: 8,  reorder_level: 10 },
  { id: 4, sku: "PREG075", name: "Pregabalin 75mg",   total_stock: 12, reorder_level: 20 },
];

export const customers: Customer[] = [
  { id: "c1", name: "Rajesh Kumar",       phone: "9876543210", address: "12, MG Road, Bengaluru",  gstin: "" },
  { id: "c2", name: "Priya Sharma",       phone: "9812345678", address: "45, Jayanagar, Bengaluru", gstin: "" },
  { id: "c3", name: "Anil Medical Store", phone: "9800001111", address: "56, Commercial St",        gstin: "29AABCT1332L1ZV" },
  { id: "c4", name: "Suresh Nair",        phone: "9988776655", address: "78, Koramangala",          gstin: "" },
  { id: "c5", name: "Meera Pillai",       phone: "9900112233", address: "23, Whitefield",           gstin: "" },
];

const today = new Date();
const days = Array.from({ length: 7 }, (_, i) => {
  const d = new Date(today);
  d.setDate(d.getDate() - 6 + i);
  return d.toISOString().slice(0, 10);
});

export const periodSummary: PeriodSummary = {
  total_sales:  84320,
  total_bills:  312,
  total_gst:    9080,
  avg_bill:     270.25,
  sales_by_day: days.map((date, i) => ({
    date,
    amount: 8000 + Math.sin(i * 1.2) * 3000 + i * 1200,
  })),
};

export const gstr1Rows: Gstr1B2cRow[] = [
  { month: "2026-06", hsn_code: "30049099", gst_rate: 12, taxable_value: 45000, igst: 0, cgst: 2700, sgst: 2700 },
  { month: "2026-06", hsn_code: "30041090", gst_rate: 12, taxable_value: 18000, igst: 0, cgst: 1080, sgst: 1080 },
];
