import { useEffect, useState } from "react";
import { IconDownload } from "@tabler/icons-react";
import { getPeriodSummary, getGstr1B2cSummary } from "../data/api";
import type { PeriodSummary, Gstr1B2cRow } from "../data/types";

function fmt(n: number) {
  return "₹" + n.toLocaleString("en-IN", { maximumFractionDigits: 2, minimumFractionDigits: 2 });
}

export default function Sales() {
  const [month, setMonth] = useState(() => new Date().toISOString().slice(0, 7));
  const [summary, setSummary] = useState<PeriodSummary | null>(null);
  const [gstr1, setGstr1] = useState<Gstr1B2cRow[]>([]);

  useEffect(() => {
    setSummary(null);
    getPeriodSummary(month).then(setSummary);
    getGstr1B2cSummary(month).then(setGstr1);
  }, [month]);

  return (
    <>
      <div className="section-header">
        <div>
          <div className="section-title">Sales Reports</div>
          <div className="section-sub">Monthly summary and GSTR-1 B2C</div>
        </div>
        <div className="flex gap-8">
          <input type="month" value={month} onChange={e => setMonth(e.target.value)} />
          <button className="btn btn-secondary">
            <IconDownload size={15} /> Export CSV
          </button>
        </div>
      </div>

      <div className="kpi-grid">
        <div className="kpi-card">
          <div className="kpi-label">Total Sales</div>
          <div className="kpi-value">{summary ? fmt(summary.total_sales) : "—"}</div>
        </div>
        <div className="kpi-card">
          <div className="kpi-label">Bills</div>
          <div className="kpi-value">{summary?.total_bills ?? "—"}</div>
        </div>
        <div className="kpi-card">
          <div className="kpi-label">GST Collected</div>
          <div className="kpi-value">{summary ? fmt(summary.total_gst) : "—"}</div>
        </div>
        <div className="kpi-card">
          <div className="kpi-label">Avg Bill Value</div>
          <div className="kpi-value">{summary ? fmt(summary.avg_bill) : "—"}</div>
        </div>
      </div>

      <div className="card">
        <div className="card-header">
          <div className="card-title">GSTR-1 B2C — HSN Summary</div>
          <span className="badge badge-blue">Outward Supplies</span>
        </div>
        {gstr1.length === 0 ? (
          <div className="empty-state"><p>No data for selected month</p></div>
        ) : (
          <div className="table-wrap" style={{ border: "none", boxShadow: "none" }}>
            <table>
              <thead>
                <tr>
                  <th>HSN Code</th>
                  <th>GST Rate</th>
                  <th>Taxable Value</th>
                  <th>CGST</th>
                  <th>SGST</th>
                  <th>IGST</th>
                  <th>Total Tax</th>
                </tr>
              </thead>
              <tbody>
                {gstr1.map((r, i) => (
                  <tr key={i}>
                    <td className="mono">{r.hsn_code}</td>
                    <td>{r.gst_rate}%</td>
                    <td>{fmt(r.taxable_value)}</td>
                    <td>{fmt(r.cgst)}</td>
                    <td>{fmt(r.sgst)}</td>
                    <td>{fmt(r.igst)}</td>
                    <td className="fw-600">{fmt(r.cgst + r.sgst + r.igst)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {summary && (
        <div className="card">
          <div className="card-header">
            <div className="card-title">Daily Sales — {month}</div>
          </div>
          <div className="table-wrap" style={{ border: "none", boxShadow: "none" }}>
            <table>
              <thead>
                <tr><th>Date</th><th>Revenue</th></tr>
              </thead>
              <tbody>
                {summary.sales_by_day.map(d => (
                  <tr key={d.date}>
                    <td>{d.date}</td>
                    <td>{fmt(d.amount)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </>
  );
}
