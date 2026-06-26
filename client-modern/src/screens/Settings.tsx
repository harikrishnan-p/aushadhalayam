import { useState } from "react";
import { useApp } from "../App";

type Tab = "appearance" | "billing" | "sync" | "hardware" | "gst";

const THEMES = [
  { id: "default", label: "Blue",    color: "#2563eb" },
  { id: "dark",    label: "Dark",    color: "#1e293b" },
  { id: "emerald", label: "Emerald", color: "#059669" },
  { id: "rose",    label: "Rose",    color: "#e11d48" },
  { id: "violet",  label: "Violet",  color: "#7c3aed" },
] as const;

function Toggle({ checked, onChange }: { checked: boolean; onChange: (v: boolean) => void }) {
  return (
    <label className="toggle">
      <input type="checkbox" checked={checked} onChange={e => onChange(e.target.checked)} />
      <div className="toggle-track" />
    </label>
  );
}

export default function Settings() {
  const { theme, setTheme, animEnabled, setAnimEnabled } = useApp();
  const [tab, setTab] = useState<Tab>("appearance");

  const tabs: { id: Tab; label: string }[] = [
    { id: "appearance", label: "Appearance" },
    { id: "billing",    label: "Billing" },
    { id: "sync",       label: "Sync" },
    { id: "hardware",   label: "Hardware" },
    { id: "gst",        label: "GST / Tax" },
  ];

  return (
    <>
      <div className="section-title" style={{ marginBottom: 16 }}>Settings</div>

      <div className="settings-tabs">
        {tabs.map(t => (
          <button
            key={t.id}
            className={`settings-tab${tab === t.id ? " active" : ""}`}
            onClick={() => setTab(t.id)}
          >
            {t.label}
          </button>
        ))}
      </div>

      {tab === "appearance" && (
        <div className="card">
          <div className="card-title" style={{ marginBottom: 14 }}>Colour Theme</div>
          <div className="theme-swatches">
            {THEMES.map(t => (
              <div className="theme-swatch" key={t.id} onClick={() => setTheme(t.id)}>
                <div
                  className={`swatch-circle${theme === t.id ? " active" : ""}`}
                  style={{ background: t.color }}
                />
                <span className="swatch-label">{t.label}</span>
              </div>
            ))}
          </div>

          <div style={{ marginTop: 20 }}>
            <div className="card-title" style={{ marginBottom: 10 }}>Interface</div>
            <div className="toggle-row">
              <div className="toggle-info">
                <div className="toggle-name">Animations</div>
                <div className="toggle-desc">CSS transitions and fade effects</div>
              </div>
              <Toggle checked={animEnabled} onChange={setAnimEnabled} />
            </div>
          </div>
        </div>
      )}

      {tab === "billing" && (
        <div className="card">
          <div className="card-title" style={{ marginBottom: 14 }}>Billing Defaults</div>
          <div className="toggle-row">
            <div className="toggle-info">
              <div className="toggle-name">Auto-print receipt</div>
              <div className="toggle-desc">Print thermal receipt after checkout</div>
            </div>
            <Toggle checked={false} onChange={() => {}} />
          </div>
          <div className="toggle-row">
            <div className="toggle-info">
              <div className="toggle-name">Ask for customer on every bill</div>
              <div className="toggle-desc">Prompt customer selection before billing</div>
            </div>
            <Toggle checked={false} onChange={() => {}} />
          </div>
          <div className="toggle-row">
            <div className="toggle-info">
              <div className="toggle-name">Warn on expiry &lt; 90 days</div>
              <div className="toggle-desc">Highlight batches expiring soon</div>
            </div>
            <Toggle checked={true} onChange={() => {}} />
          </div>
          <div className="mt-12 form-group" style={{ maxWidth: 240 }}>
            <label>Default Discount %</label>
            <input type="number" min={0} max={100} defaultValue={0} />
          </div>
        </div>
      )}

      {tab === "sync" && (
        <div className="card">
          <div className="card-title" style={{ marginBottom: 14 }}>Cloud Sync</div>
          <div className="toggle-row">
            <div className="toggle-info">
              <div className="toggle-name">Enable sync</div>
              <div className="toggle-desc">Push changes to PostgreSQL cloud</div>
            </div>
            <Toggle checked={true} onChange={() => {}} />
          </div>
          <div className="mt-12 form-group">
            <label>Sync Endpoint URL</label>
            <input defaultValue="https://sync.aushadhalayam.local/api/ingest" />
          </div>
          <div className="mt-8 form-group" style={{ maxWidth: 180 }}>
            <label>Sync interval (seconds)</label>
            <input type="number" defaultValue={30} min={5} />
          </div>
          <button className="btn btn-primary mt-12">Save Sync Settings</button>
        </div>
      )}

      {tab === "hardware" && (
        <div className="card">
          <div className="card-title" style={{ marginBottom: 14 }}>Hardware</div>
          <div className="form-group" style={{ maxWidth: 300 }}>
            <label>Thermal Printer Port</label>
            <select>
              <option>COM1</option><option>COM2</option><option>COM3</option>
              <option>USB (auto-detect)</option>
            </select>
          </div>
          <div className="mt-12 form-group" style={{ maxWidth: 300 }}>
            <label>Paper Width</label>
            <select>
              <option>58mm</option><option>80mm</option>
            </select>
          </div>
          <div className="toggle-row mt-12">
            <div className="toggle-info">
              <div className="toggle-name">Open cash drawer after payment</div>
              <div className="toggle-desc">Send pulse signal to cash drawer</div>
            </div>
            <Toggle checked={false} onChange={() => {}} />
          </div>
        </div>
      )}

      {tab === "gst" && (
        <div className="card">
          <div className="card-title" style={{ marginBottom: 14 }}>GST Configuration</div>
          <div className="input-row">
            <div className="form-group">
              <label>GSTIN</label>
              <input placeholder="29AABCT1332L1ZV" className="mono" />
            </div>
            <div className="form-group">
              <label>State Code</label>
              <input placeholder="29" style={{ maxWidth: 80 }} />
            </div>
          </div>
          <div className="mt-8 form-group">
            <label>Legal Business Name</label>
            <input placeholder="Aushadhalayam Pharmacy" />
          </div>
          <div className="mt-8 form-group">
            <label>Registered Address</label>
            <textarea rows={2} placeholder="Shop address as per GSTIN registration" />
          </div>
          <div className="toggle-row mt-12">
            <div className="toggle-info">
              <div className="toggle-name">Composition Scheme</div>
              <div className="toggle-desc">Tick if registered under GST Composition Scheme</div>
            </div>
            <Toggle checked={false} onChange={() => {}} />
          </div>
          <button className="btn btn-primary mt-12">Save GST Settings</button>
        </div>
      )}
    </>
  );
}
