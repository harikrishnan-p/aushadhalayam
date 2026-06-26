import type { Product, StockBatch, LowStockAlert, Customer, PeriodSummary, Gstr1B2cRow } from "./types";

export const products: Product[] = [
  { id: "p1", name: "Paracetamol 500mg", generic_name: "Paracetamol", manufacturer: "Cipla", hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, reorder_level: 50, unit: "strip" },
  { id: "p2", name: "Amoxicillin 250mg", generic_name: "Amoxicillin", manufacturer: "Sun Pharma", hsn_code: "30041090", gst_rate: 12, is_scheduled: 1, reorder_level: 30, unit: "strip" },
  { id: "p3", name: "Alprazolam 0.25mg", generic_name: "Alprazolam", manufacturer: "Torrent", hsn_code: "30049019", gst_rate: 12, is_scheduled: 3, reorder_level: 10, unit: "strip" },
  { id: "p4", name: "Pregabalin 75mg", generic_name: "Pregabalin", manufacturer: "Dr. Reddy's", hsn_code: "30042090", gst_rate: 12, is_scheduled: 2, reorder_level: 20, unit: "strip" },
  { id: "p5", name: "Atorvastatin 10mg", generic_name: "Atorvastatin", manufacturer: "Cipla", hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, reorder_level: 40, unit: "strip" },
  { id: "p6", name: "Metformin 500mg", generic_name: "Metformin", manufacturer: "USV", hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, reorder_level: 60, unit: "strip" },
  { id: "p7", name: "Cetirizine 10mg", generic_name: "Cetirizine", manufacturer: "Mankind", hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, reorder_level: 30, unit: "strip" },
  { id: "p8", name: "Omeprazole 20mg", generic_name: "Omeprazole", manufacturer: "Intas", hsn_code: "30049099", gst_rate: 12, is_scheduled: 0, reorder_level: 40, unit: "strip" },
];

export const batches: StockBatch[] = [
  { id: "b1", product_id: "p1", batch_no: "BTH001", expiry_date: "2026-06-30", mrp: 25.00, purchase_rate: 18.00, quantity: 120 },
  { id: "b2", product_id: "p1", batch_no: "BTH002", expiry_date: "2027-03-31", mrp: 26.00, purchase_rate: 19.00, quantity: 80 },
  { id: "b3", product_id: "p2", batch_no: "BTH003", expiry_date: "2026-12-31", mrp: 85.00, purchase_rate: 60.00, quantity: 45 },
  { id: "b4", product_id: "p3", batch_no: "BTH004", expiry_date: "2026-09-30", mrp: 45.00, purchase_rate: 30.00, quantity: 30 },
  { id: "b5", product_id: "p4", batch_no: "BTH005", expiry_date: "2027-01-31", mrp: 120.00, purchase_rate: 85.00, quantity: 25 },
  { id: "b6", product_id: "p5", batch_no: "BTH006", expiry_date: "2026-11-30", mrp: 55.00, purchase_rate: 38.00, quantity: 90 },
  { id: "b7", product_id: "p6", batch_no: "BTH007", expiry_date: "2027-06-30", mrp: 40.00, purchase_rate: 28.00, quantity: 150 },
  { id: "b8", product_id: "p7", batch_no: "BTH008", expiry_date: "2026-08-31", mrp: 30.00, purchase_rate: 20.00, quantity: 60 },
  { id: "b9", product_id: "p8", batch_no: "BTH009", expiry_date: "2027-04-30", mrp: 65.00, purchase_rate: 45.00, quantity: 75 },
];

export const lowStockAlerts: LowStockAlert[] = [
  { product_id: "p3", product_name: "Alprazolam 0.25mg", current_stock: 8, reorder_level: 10 },
  { product_id: "p4", product_name: "Pregabalin 75mg", current_stock: 12, reorder_level: 20 },
];

export const customers: Customer[] = [
  { id: "c1", name: "Rajesh Kumar", phone: "9876543210", address: "12, MG Road, Bengaluru", gstin: "" },
  { id: "c2", name: "Priya Sharma", phone: "9812345678", address: "45, Jayanagar, Bengaluru", gstin: "" },
  { id: "c3", name: "Anil Medical Store", phone: "9800001111", address: "56, Commercial St", gstin: "29AABCT1332L1ZV" },
  { id: "c4", name: "Suresh Nair", phone: "9988776655", address: "78, Koramangala", gstin: "" },
  { id: "c5", name: "Meera Pillai", phone: "9900112233", address: "23, Whitefield", gstin: "" },
];

const today = new Date();
const days = Array.from({ length: 7 }, (_, i) => {
  const d = new Date(today);
  d.setDate(d.getDate() - 6 + i);
  return d.toISOString().slice(0, 10);
});

export const periodSummary: PeriodSummary = {
  total_sales: 84320,
  total_bills: 312,
  total_gst: 9080,
  avg_bill: 270.25,
  sales_by_day: days.map((date, i) => ({
    date,
    amount: 8000 + Math.sin(i * 1.2) * 3000 + i * 1200,
  })),
};

export const gstr1Rows: Gstr1B2cRow[] = [
  { month: "2026-06", hsn_code: "30049099", gst_rate: 12, taxable_value: 45000, igst: 0, cgst: 2700, sgst: 2700 },
  { month: "2026-06", hsn_code: "30041090", gst_rate: 12, taxable_value: 18000, igst: 0, cgst: 1080, sgst: 1080 },
];
