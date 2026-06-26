import { useCallback, useRef, useState } from "react";
import {
  IconSearch, IconTrash, IconReceipt2,
} from "@tabler/icons-react";
import { searchProducts, getStockBatches, searchCustomers, processCheckout } from "../data/api";
import type { BillItem, Customer, Product, Schedule, StockBatch } from "../data/types";
import { SCHEDULE_LABELS } from "./Medicines";

const SCHEDULE_PILL: Record<Schedule, string> = {
  0: "pill-otc", 1: "pill-h", 2: "pill-h1", 3: "pill-x",
};

const NEEDS_PRESCRIPTION: Schedule[] = [1, 2, 3];

type PayMode = "CASH" | "UPI" | "CARD" | "CREDIT" | "CHEQUE";

function expiryBadge(dateStr: string) {
  const days = Math.ceil((new Date(dateStr).getTime() - Date.now()) / 86400_000);
  if (days < 0)   return <span className="badge badge-red">Expired</span>;
  if (days < 90)  return <span className="badge badge-yellow">{days}d</span>;
  return <span className="badge badge-gray">{dateStr.slice(0, 7)}</span>;
}

// ── Customer typeahead ────────────────────────────────────────────────────────
function CustomerField({ onSelect }: { onSelect: (c: Customer | null, _name: string) => void }) {
  const [value, setValue] = useState("");
  const [results, setResults] = useState<Customer[]>([]);
  const [open, setOpen] = useState(false);
  const debounce = useRef<ReturnType<typeof setTimeout> | null>(null);

  const handleChange = (v: string) => {
    setValue(v);
    onSelect(null, v);
    if (debounce.current) clearTimeout(debounce.current);
    if (v.length < 1) { setResults([]); setOpen(false); return; }
    debounce.current = setTimeout(async () => {
      const res = await searchCustomers(v);
      setResults(res);
      setOpen(res.length > 0);
    }, 200);
  };

  const pick = (c: Customer) => {
    setValue(c.name);
    onSelect(c, c.name);
    setOpen(false);
    setResults([]);
  };

  return (
    <div className="autocomplete-wrap">
      <input
        placeholder="Customer name / phone (optional)"
        value={value}
        onChange={e => handleChange(e.target.value)}
        onFocus={() => results.length > 0 && setOpen(true)}
        onBlur={() => setTimeout(() => setOpen(false), 150)}
      />
      {open && (
        <div className="autocomplete-list">
          {results.map(c => (
            <div className="autocomplete-item" key={c.id} onMouseDown={() => pick(c)}>
              <div>
                <div className="autocomplete-item-main">{c.name}</div>
                <div className="autocomplete-item-sub">{c.phone}</div>
              </div>
              {c.gstin && <span className="badge badge-blue">GST</span>}
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

// ── Medicine search ───────────────────────────────────────────────────────────
function MedSearch({ onAdd }: { onAdd: (p: Product, b: StockBatch) => void }) {
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<Product[]>([]);
  const [open, setOpen] = useState(false);
  const debounce = useRef<ReturnType<typeof setTimeout> | null>(null);

  const handleChange = (v: string) => {
    setQuery(v);
    if (debounce.current) clearTimeout(debounce.current);
    if (v.length < 1) { setResults([]); setOpen(false); return; }
    debounce.current = setTimeout(async () => {
      const res = await searchProducts(v);
      setResults(res);
      setOpen(res.length > 0);
    }, 200);
  };

  const pick = async (p: Product) => {
    const batches = await getStockBatches(p.id);
    if (batches.length === 0) return;
    // FEFO: pick earliest non-expired batch
    const sorted = [...batches].sort((a, b) =>
      new Date(a.expiry_date).getTime() - new Date(b.expiry_date).getTime()
    );
    const valid = sorted.find(b => b.quantity > 0 && new Date(b.expiry_date) > new Date());
    if (valid) {
      onAdd(p, valid);
    }
    setQuery("");
    setResults([]);
    setOpen(false);
  };

  return (
    <div className="autocomplete-wrap" style={{ flex: 1 }}>
      <div className="search-bar">
        <IconSearch size={14} className="search-icon" />
        <input
          placeholder="Search medicine to add…"
          value={query}
          onChange={e => handleChange(e.target.value)}
          onFocus={() => results.length > 0 && setOpen(true)}
          onBlur={() => setTimeout(() => setOpen(false), 150)}
          style={{ paddingLeft: 34 }}
        />
      </div>
      {open && (
        <div className="autocomplete-list">
          {results.map(p => (
            <div className="autocomplete-item" key={p.id} onMouseDown={() => pick(p)}>
              <div>
                <div className="autocomplete-item-main">{p.name}</div>
                <div className="autocomplete-item-sub">{p.manufacturer} · {p.generic_name}</div>
              </div>
              <span className={SCHEDULE_PILL[p.is_scheduled]}>
                {SCHEDULE_LABELS[p.is_scheduled]}
              </span>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

// ── Main Billing screen ───────────────────────────────────────────────────────
export default function Billing() {
  const [customer, setCustomer] = useState<Customer | null>(null);
  const [doctor, setDoctor] = useState("");
  const [prescriptionNo, setPrescriptionNo] = useState("");
  const [notes, setNotes] = useState("");
  const [payMode, setPayMode] = useState<PayMode>("CASH");
  const [items, setItems] = useState<BillItem[]>([]);
  const [processing, setProcessing] = useState(false);
  const [success, setSuccess] = useState<string | null>(null);

  const needsRx = items.some(i => NEEDS_PRESCRIPTION.includes(i.is_scheduled));

  const addItem = useCallback(async (p: Product, b: StockBatch) => {
    const batches = await getStockBatches(p.id);
    setItems(prev => {
      const existing = prev.findIndex(i => i.product_id === p.id && i.batch_id === b.id);
      if (existing >= 0) {
        const next = [...prev];
        next[existing] = { ...next[existing], quantity: next[existing].quantity + 1 };
        return next;
      }
      const newItem: BillItem = {
        product_id: p.id,
        product_name: p.name,
        is_scheduled: p.is_scheduled,
        batch_id: b.id,
        batch_no: b.batch_no,
        expiry_date: b.expiry_date,
        mrp: b.mrp,
        quantity: 1,
        discount_pct: 0,
        gst_rate: p.gst_rate,
        hsn_code: p.hsn_code,
        available_batches: batches,
      };
      return [...prev, newItem];
    });
  }, []);

  const updateItem = (idx: number, patch: Partial<BillItem>) => {
    setItems(prev => {
      const next = [...prev];
      next[idx] = { ...next[idx], ...patch };
      return next;
    });
  };

  const changeBatch = async (idx: number, batchId: string) => {
    const item = items[idx];
    const b = item.available_batches.find(x => x.id === batchId);
    if (b) updateItem(idx, { batch_id: b.id, batch_no: b.batch_no, expiry_date: b.expiry_date, mrp: b.mrp });
  };

  const removeItem = (idx: number) => setItems(prev => prev.filter((_, i) => i !== idx));

  // totals
  const subtotal = items.reduce((s, i) => s + i.mrp * i.quantity * (1 - i.discount_pct / 100), 0);
  const totalGst = items.reduce((s, i) => {
    const taxable = i.mrp * i.quantity * (1 - i.discount_pct / 100) / (1 + i.gst_rate / 100);
    return s + taxable * i.gst_rate / 100;
  }, 0);
  const grandTotal = subtotal;

  const checkout = async () => {
    if (items.length === 0) return;
    setProcessing(true);
    const { bill_id } = await processCheckout({
      customer_id: customer?.id,
      doctor_name: doctor || undefined,
      prescription_no: prescriptionNo || undefined,
      notes: notes || undefined,
      payment_mode: payMode,
      items: items.map(i => ({
        product_id: i.product_id,
        batch_id: i.batch_id,
        quantity: i.quantity,
        discount_pct: i.discount_pct,
      })),
    });
    setSuccess(bill_id);
    setItems([]);
    setDoctor("");
    setPrescriptionNo("");
    setNotes("");
    setProcessing(false);
  };

  const clearSuccess = () => setSuccess(null);

  return (
    <div className="billing-layout" style={{ height: "calc(100vh - 52px - 40px)" }}>
      {/* ── Left: items area ─── */}
      <div className="bill-items-area">
        {success && (
          <div className="card" style={{ background: "var(--success-bg)", borderColor: "var(--success)" }}>
            <div className="flex gap-8" style={{ alignItems: "center" }}>
              <IconReceipt2 size={18} color="var(--success)" />
              <span className="fw-600" style={{ color: "var(--success)", flex: 1 }}>
                Bill saved: {success}
              </span>
              <button className="btn btn-ghost btn-sm" onClick={clearSuccess}>×</button>
            </div>
          </div>
        )}

        <div className="item-search-row">
          <MedSearch onAdd={addItem} />
        </div>

        <div className="bill-table-wrap">
          {items.length === 0 ? (
            <div className="empty-state">
              <IconReceipt2 size={32} />
              <p>Search and add medicines above</p>
            </div>
          ) : (
            <table>
              <thead>
                <tr>
                  <th>Medicine</th>
                  <th>Batch / Expiry</th>
                  <th>MRP</th>
                  <th>Qty</th>
                  <th>Disc%</th>
                  <th>Amount</th>
                  <th></th>
                </tr>
              </thead>
              <tbody>
                {items.map((item, idx) => {
                  const lineAmt = item.mrp * item.quantity * (1 - item.discount_pct / 100);
                  return (
                    <tr key={`${item.product_id}-${item.batch_id}`}>
                      <td>
                        <div className="fw-600">{item.product_name}</div>
                        <span className={SCHEDULE_PILL[item.is_scheduled]} style={{ fontSize: 10 }}>
                          {SCHEDULE_LABELS[item.is_scheduled]}
                        </span>
                      </td>
                      <td>
                        <select
                          value={item.batch_id}
                          onChange={e => changeBatch(idx, e.target.value)}
                          style={{ fontSize: 12, padding: "3px 6px" }}
                        >
                          {item.available_batches.map(b => (
                            <option key={b.id} value={b.id}>
                              {b.batch_no} ({b.expiry_date.slice(0, 7)})
                            </option>
                          ))}
                        </select>
                        <div style={{ marginTop: 2 }}>{expiryBadge(item.expiry_date)}</div>
                      </td>
                      <td>₹{item.mrp.toFixed(2)}</td>
                      <td>
                        <input
                          type="number"
                          className="qty-input"
                          min={1}
                          max={999}
                          value={item.quantity}
                          onChange={e => updateItem(idx, { quantity: Math.max(1, +e.target.value) })}
                        />
                      </td>
                      <td>
                        <input
                          type="number"
                          className="discount-input"
                          min={0}
                          max={100}
                          step={0.5}
                          value={item.discount_pct}
                          onChange={e => updateItem(idx, { discount_pct: Math.min(100, Math.max(0, +e.target.value)) })}
                        />
                      </td>
                      <td className="fw-600">₹{lineAmt.toFixed(2)}</td>
                      <td>
                        <button className="btn btn-ghost btn-sm" onClick={() => removeItem(idx)}>
                          <IconTrash size={14} color="var(--danger)" />
                        </button>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          )}
        </div>
      </div>

      {/* ── Right: bill panel ─── */}
      <div className="bill-panel">
        {/* Customer */}
        <div className="card">
          <div className="card-title" style={{ marginBottom: 10 }}>Customer</div>
          <CustomerField onSelect={(c, _name) => { setCustomer(c); }} />
        </div>

        {/* Doctor / Rx — shown when any scheduled item present */}
        {needsRx && (
          <div className="card" style={{ borderColor: "var(--warning)" }}>
            <div className="flex gap-8" style={{ alignItems: "center", marginBottom: 8 }}>
              <div className="card-title">Prescription Required</div>
              <span className="badge badge-yellow">Sch H / H1 / X</span>
            </div>
            <div className="form-group" style={{ marginBottom: 8 }}>
              <label>Doctor Name *</label>
              <input
                placeholder="Dr. Ravi Kumar"
                value={doctor}
                onChange={e => setDoctor(e.target.value)}
              />
            </div>
            <div className="form-group">
              <label>Prescription No.</label>
              <input
                placeholder="RX-2026-001"
                value={prescriptionNo}
                onChange={e => setPrescriptionNo(e.target.value)}
              />
            </div>
          </div>
        )}

        {/* Notes */}
        <div className="card">
          <div className="form-group">
            <label>Notes</label>
            <textarea
              rows={2}
              placeholder="Optional note for this bill…"
              value={notes}
              onChange={e => setNotes(e.target.value)}
              style={{ resize: "vertical" }}
            />
          </div>
        </div>

        {/* Totals */}
        <div className="bill-total-block">
          <div className="total-row">
            <span>Subtotal (incl. GST)</span>
            <span>₹{subtotal.toFixed(2)}</span>
          </div>
          <div className="total-row">
            <span>GST collected</span>
            <span>₹{totalGst.toFixed(2)}</span>
          </div>
          <div className="total-row grand">
            <span>Total</span>
            <span>₹{grandTotal.toFixed(2)}</span>
          </div>
        </div>

        {/* Payment mode */}
        <div className="card">
          <div className="card-title" style={{ marginBottom: 8 }}>Payment Mode</div>
          <div className="pay-modes">
            {(["CASH", "UPI", "CARD", "CREDIT", "CHEQUE"] as PayMode[]).map(m => (
              <button
                key={m}
                className={`pay-mode-btn${payMode === m ? " active" : ""}`}
                onClick={() => setPayMode(m)}
              >
                {m}
              </button>
            ))}
          </div>
        </div>

        {/* Checkout */}
        <button
          className="btn btn-primary btn-lg w-full"
          disabled={items.length === 0 || processing || (needsRx && !doctor)}
          onClick={checkout}
        >
          {processing ? <span className="spinner" /> : <IconReceipt2 size={17} />}
          {processing ? "Processing…" : `Checkout — ₹${grandTotal.toFixed(2)}`}
        </button>
        {needsRx && !doctor && (
          <div className="text-sm" style={{ color: "var(--warning)", textAlign: "center" }}>
            Doctor name required for scheduled medicines
          </div>
        )}
      </div>
    </div>
  );
}
