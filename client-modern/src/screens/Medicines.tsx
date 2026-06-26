import { useEffect, useRef, useState } from "react";
import { IconSearch, IconPlus, IconPill } from "@tabler/icons-react";
import { searchProducts } from "../data/api";
import type { Product, Schedule } from "../data/types";

export const SCHEDULE_LABELS: Record<Schedule, string> = {
  0: "OTC",
  1: "Sch H",
  2: "Sch H1",
  3: "Sch X",
};

const SCHEDULE_PILL: Record<Schedule, string> = {
  0: "pill-otc",
  1: "pill-h",
  2: "pill-h1",
  3: "pill-x",
};

export default function Medicines() {
  const [query, setQuery] = useState("");
  const [products, setProducts] = useState<Product[]>([]);
  const [loading, setLoading] = useState(false);
  const debounce = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    setLoading(true);
    if (debounce.current) clearTimeout(debounce.current);
    debounce.current = setTimeout(async () => {
      const res = await searchProducts(query);
      setProducts(res);
      setLoading(false);
    }, 250);
    return () => { if (debounce.current) clearTimeout(debounce.current); };
  }, [query]);

  return (
    <>
      <div className="section-header">
        <div>
          <div className="section-title">Medicine Catalogue</div>
          <div className="section-sub">Schedule classification, GST rate, and stock details</div>
        </div>
        <button className="btn btn-primary">
          <IconPlus size={15} /> Add Medicine
        </button>
      </div>

      <div className="search-bar" style={{ maxWidth: 360 }}>
        <IconSearch size={15} className="search-icon" />
        <input
          placeholder="Search by name or generic…"
          value={query}
          onChange={e => setQuery(e.target.value)}
        />
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Generic</th>
              <th>Manufacturer</th>
              <th>Schedule</th>
              <th>HSN</th>
              <th>GST%</th>
              <th>Reorder</th>
              <th>Unit</th>
            </tr>
          </thead>
          <tbody>
            {loading ? (
              <tr><td colSpan={8} style={{ textAlign: "center", padding: 24 }}><div className="spinner" style={{ margin: "auto" }} /></td></tr>
            ) : products.length === 0 ? (
              <tr><td colSpan={8}>
                <div className="empty-state">
                  <IconPill size={32} />
                  <p>No medicines found</p>
                </div>
              </td></tr>
            ) : products.map(p => (
              <tr key={p.id}>
                <td className="fw-600">{p.name}</td>
                <td className="text-2">{p.generic_name}</td>
                <td className="text-2">{p.manufacturer}</td>
                <td>
                  <span className={SCHEDULE_PILL[p.is_scheduled]}>
                    {SCHEDULE_LABELS[p.is_scheduled]}
                  </span>
                </td>
                <td className="mono text-sm">{p.hsn_code}</td>
                <td>{p.gst_rate}%</td>
                <td>{p.reorder_level}</td>
                <td className="text-2">{p.unit}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}
