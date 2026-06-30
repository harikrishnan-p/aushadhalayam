import { useState } from "react";
import { IconPlus } from "@tabler/icons-react";
import { batches, products } from "../data/mock";

// UI-only screen — Purchase Orders backend not yet implemented.
// Displays existing stock batches as "received" history.

export default function Purchase() {
  const [showForm, setShowForm] = useState(false);

  return (
    <>
      <div className="section-header">
        <div>
          <div className="section-title">Purchase Orders</div>
          <div className="section-sub flex gap-8" style={{ alignItems: "center" }}>
            Receive stock and manage supplier orders
            <span className="badge badge-yellow">UI preview — backend coming soon</span>
          </div>
        </div>
        <button className="btn btn-primary" onClick={() => setShowForm(v => !v)}>
          <IconPlus size={15} /> Receive Stock
        </button>
      </div>

      {showForm && (
        <div className="card">
          <div className="card-title" style={{ marginBottom: 12 }}>Receive New Stock</div>
          <div className="input-row">
            <div className="form-group">
              <label>Medicine</label>
              <input placeholder="Search medicine…" />
            </div>
            <div className="form-group">
              <label>Supplier</label>
              <input placeholder="Supplier name" />
            </div>
            <div className="form-group">
              <label>Batch No.</label>
              <input placeholder="BTH001" />
            </div>
          </div>
          <div className="input-row" style={{ marginTop: 10 }}>
            <div className="form-group">
              <label>Expiry Date</label>
              <input type="month" />
            </div>
            <div className="form-group">
              <label>Quantity</label>
              <input type="number" min={1} placeholder="100" />
            </div>
            <div className="form-group">
              <label>Purchase Rate (₹)</label>
              <input type="number" step="0.01" placeholder="0.00" />
            </div>
            <div className="form-group">
              <label>MRP (₹)</label>
              <input type="number" step="0.01" placeholder="0.00" />
            </div>
          </div>
          <div className="flex gap-8 mt-12">
            <button className="btn btn-primary">Save</button>
            <button className="btn btn-secondary" onClick={() => setShowForm(false)}>Cancel</button>
          </div>
        </div>
      )}

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Medicine</th>
              <th>Batch</th>
              <th>Expiry</th>
              <th>Qty Received</th>
              <th>Purchase Rate</th>
              <th>MRP</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            {batches.map(b => {
              const prod = products.find(p => p.id === b.product_id);
              return (
                <tr key={b.id}>
                  <td className="fw-600">{prod?.name ?? b.product_id}</td>
                  <td className="mono text-sm">{b.batch_number}</td>
                  <td>{b.expiry_date.slice(0, 7)}</td>
                  <td>{b.quantity}</td>
                  <td>₹{b.purchase_price.toFixed(2)}</td>
                  <td>₹{b.mrp.toFixed(2)}</td>
                  <td><span className="badge badge-green">Received</span></td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </>
  );
}
