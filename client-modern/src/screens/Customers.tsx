import { useEffect, useState } from "react";
import { IconUsers, IconPlus, IconSearch } from "@tabler/icons-react";
import { getCustomers } from "../data/api";
import type { Customer } from "../data/types";

export default function Customers() {
  const [customers, setCustomers] = useState<Customer[]>([]);
  const [query, setQuery] = useState("");

  useEffect(() => { getCustomers().then(setCustomers); }, []);

  const filtered = customers.filter(c =>
    c.name.toLowerCase().includes(query.toLowerCase()) ||
    c.phone.includes(query)
  );

  return (
    <>
      <div className="section-header">
        <div>
          <div className="section-title">Customers</div>
          <div className="section-sub flex gap-8" style={{ alignItems: "center" }}>
            Customer directory
            <span className="badge badge-yellow">UI preview — CRUD backend coming soon</span>
          </div>
        </div>
        <button className="btn btn-primary">
          <IconPlus size={15} /> Add Customer
        </button>
      </div>

      <div className="search-bar" style={{ maxWidth: 360 }}>
        <IconSearch size={15} className="search-icon" />
        <input
          placeholder="Search by name or phone…"
          value={query}
          onChange={e => setQuery(e.target.value)}
        />
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Phone</th>
              <th>Address</th>
              <th>GSTIN</th>
              <th>Type</th>
            </tr>
          </thead>
          <tbody>
            {filtered.length === 0 ? (
              <tr><td colSpan={5}>
                <div className="empty-state">
                  <IconUsers size={32} />
                  <p>No customers found</p>
                </div>
              </td></tr>
            ) : filtered.map(c => (
              <tr key={c.id}>
                <td className="fw-600">{c.name}</td>
                <td className="mono text-sm">{c.phone}</td>
                <td className="text-2">{c.address}</td>
                <td className="mono text-sm">{c.gstin || "—"}</td>
                <td>
                  {c.gstin
                    ? <span className="badge badge-blue">B2B</span>
                    : <span className="badge badge-gray">B2C</span>
                  }
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}
