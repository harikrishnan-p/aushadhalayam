import { useEffect, useState } from "react";
import {
  IconCurrencyRupee, IconReceipt2, IconTrendingUp, IconAlertTriangle,
} from "@tabler/icons-react";
import { getPeriodSummary, getLowStockAlerts } from "../data/api";
import type { PeriodSummary, LowStockAlert } from "../data/types";
import { useApp } from "../App";
function fmt(n: number) {
  return "₹" + n.toLocaleString("en-IN", { maximumFractionDigits: 0 });
}

function BarChart({ data }: { data: { date: string; amount: number }[] }) {
  const max = Math.max(...data.map(d => d.amount), 1);
  return (
    <div className="bar-chart">
      {data.map(d => (
        <div className="bar-col" key={d.date}>
          <div
            className="bar"
            style={{ height: `${(d.amount / max) * 80}px` }}
            title={`${d.date}: ${fmt(d.amount)}`}
          />
          <span className="bar-label">
            {new Date(d.date + "T00:00:00").toLocaleDateString("en-IN", { day: "2-digit", month: "short" }).slice(0, 6)}
          </span>
        </div>
      ))}
    </div>
  );
}

export default function Dashboard() {
  const { setScreen } = useApp();
  const [summary, setSummary] = useState<PeriodSummary | null>(null);
  const [alerts, setAlerts] = useState<LowStockAlert[]>([]);

  useEffect(() => {
    const month = new Date().toISOString().slice(0, 7);
    getPeriodSummary(month).then(setSummary);
    getLowStockAlerts().then(setAlerts);
  }, []);

  const s = summary;

  return (
    <>
      <div className="kpi-grid">
        <div className="kpi-card">
          <div className="kpi-icon" style={{ background: "var(--brand-light)" }}>
            <IconCurrencyRupee size={20} color="var(--brand)" />
          </div>
          <div className="kpi-label">This Month Sales</div>
          <div className="kpi-value">{s ? fmt(s.total_sales) : "—"}</div>
          <div className="kpi-sub">GST: {s ? fmt(s.total_gst) : "—"}</div>
        </div>

        <div className="kpi-card">
          <div className="kpi-icon" style={{ background: "var(--success-bg)" }}>
            <IconReceipt2 size={20} color="var(--success)" />
          </div>
          <div className="kpi-label">Total Bills</div>
          <div className="kpi-value">{s?.total_bills ?? "—"}</div>
          <div className="kpi-sub">Avg: {s ? fmt(s.avg_bill) : "—"} / bill</div>
        </div>

        <div className="kpi-card">
          <div className="kpi-icon" style={{ background: "var(--warning-bg)" }}>
            <IconAlertTriangle size={20} color="var(--warning)" />
          </div>
          <div className="kpi-label">Low Stock Items</div>
          <div className="kpi-value">{alerts.length}</div>
          <div className="kpi-sub">
            <button className="btn btn-ghost btn-sm" style={{ padding: "0" }} onClick={() => setScreen("inventory")}>
              View alerts →
            </button>
          </div>
        </div>

        <div className="kpi-card">
          <div className="kpi-icon" style={{ background: "#f0fdf4" }}>
            <IconTrendingUp size={20} color="var(--success)" />
          </div>
          <div className="kpi-label">Avg Daily Sales</div>
          <div className="kpi-value">
            {s ? fmt(s.total_sales / Math.max(s.sales_by_day.length, 1)) : "—"}
          </div>
          <div className="kpi-sub">Last 7 days</div>
        </div>
      </div>

      <div className="grid-2">
        <div className="card">
          <div className="card-header">
            <div>
              <div className="card-title">Sales — Last 7 Days</div>
              <div className="card-subtitle">Daily revenue trend</div>
            </div>
          </div>
          {s ? <BarChart data={s.sales_by_day} /> : <div className="spinner" />}
        </div>

        <div className="card">
          <div className="card-header">
            <div className="card-title">Low Stock Alerts</div>
            <button className="btn btn-secondary btn-sm" onClick={() => setScreen("inventory")}>
              View all
            </button>
          </div>
          {alerts.length === 0 ? (
            <div className="empty-state" style={{ padding: "24px" }}>
              <p>All items well-stocked</p>
            </div>
          ) : (
            <div className="table-wrap" style={{ border: "none", boxShadow: "none" }}>
              <table>
                <thead>
                  <tr>
                    <th>Medicine</th>
                    <th>Stock</th>
                    <th>Reorder</th>
                  </tr>
                </thead>
                <tbody>
                  {alerts.map(a => (
                    <tr key={a.product_id}>
                      <td>{a.product_name}</td>
                      <td>
                        <span className="badge badge-red">{a.current_stock}</span>
                      </td>
                      <td className="text-2">{a.reorder_level}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      </div>

      <div className="card">
        <div className="card-header">
          <div className="card-title">Quick Actions</div>
        </div>
        <div className="flex gap-8">
          <button className="btn btn-primary" onClick={() => setScreen("billing")}>
            New Bill
          </button>
          <button className="btn btn-secondary" onClick={() => setScreen("purchase")}>
            Receive Stock
          </button>
          <button className="btn btn-secondary" onClick={() => setScreen("medicines")}>
            Add Medicine
          </button>
          <button className="btn btn-secondary" onClick={() => setScreen("sales")}>
            GSTR-1 Report
          </button>
        </div>
      </div>
    </>
  );
}

