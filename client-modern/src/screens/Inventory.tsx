import { useEffect, useState } from "react";
import { IconAlertTriangle, IconPackage } from "@tabler/icons-react";
import { getLowStockAlerts } from "../data/api";
import { batches, products } from "../data/mock";
import type { LowStockAlert } from "../data/types";

function expiryClass(dateStr: string) {
  const diff = (new Date(dateStr).getTime() - Date.now()) / 86400_000;
  if (diff < 0)   return "badge-red";
  if (diff < 90)  return "badge-yellow";
  return "badge-green";
}

function expiryLabel(dateStr: string) {
  const diff = Math.ceil((new Date(dateStr).getTime() - Date.now()) / 86400_000);
  if (diff < 0)  return "Expired";
  if (diff < 30) return `${diff}d left`;
  return dateStr.slice(0, 7);
}

export default function Inventory() {
  const [alerts, setAlerts] = useState<LowStockAlert[]>([]);

  useEffect(() => { getLowStockAlerts().then(setAlerts); }, []);

  return (
    <>
      {alerts.length > 0 && (
        <div className="card" style={{ borderColor: "var(--warning)", background: "var(--warning-bg)" }}>
          <div className="flex gap-8" style={{ alignItems: "center" }}>
            <IconAlertTriangle size={18} color="var(--warning)" />
            <span className="fw-600" style={{ color: "var(--warning)" }}>
              {alerts.length} item{alerts.length > 1 ? "s" : ""} below reorder level
            </span>
            <div style={{ marginLeft: "auto", display: "flex", gap: 8 }}>
              {alerts.map(a => (
                <span key={a.id} className="badge badge-yellow">
                  {a.name}: {a.total_stock} left
                </span>
              ))}
            </div>
          </div>
        </div>
      )}

      <div className="section-header">
        <div>
          <div className="section-title">Stock Batches</div>
          <div className="section-sub">FEFO — earliest expiry dispatched first</div>
        </div>
        <button className="btn btn-primary">
          <IconPackage size={15} /> Receive Stock
        </button>
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Medicine</th>
              <th>Batch No.</th>
              <th>Expiry</th>
              <th>MRP (₹)</th>
              <th>Purchase Rate (₹)</th>
              <th>Qty</th>
              <th>Margin</th>
            </tr>
          </thead>
          <tbody>
            {batches.map(b => {
              const prod = products.find(p => p.id === b.product_id);
              const margin = ((b.mrp - b.purchase_price) / b.mrp * 100).toFixed(1);
              return (
                <tr key={b.id}>
                  <td className="fw-600">{prod?.name ?? `Product #${b.product_id}`}</td>
                  <td className="mono text-sm">{b.batch_number}</td>
                  <td>
                    <span className={`badge ${expiryClass(b.expiry_date)}`}>
                      {expiryLabel(b.expiry_date)}
                    </span>
                  </td>
                  <td>₹{b.mrp.toFixed(2)}</td>
                  <td>₹{b.purchase_price.toFixed(2)}</td>
                  <td>
                    <span className={b.quantity <= 10 ? "badge badge-red" : "badge badge-gray"}>
                      {b.quantity}
                    </span>
                  </td>
                  <td className="text-2">{margin}%</td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </>
  );
}
