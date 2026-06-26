import { useEffect, useState } from "react";
import { useApp, type Screen } from "../App";
import { getPendingSyncCount } from "../data/api";

const TITLES: Record<Screen, string> = {
  dashboard: "Dashboard",
  billing:   "Billing",
  inventory: "Inventory",
  medicines: "Medicine Catalogue",
  purchase:  "Purchase Orders",
  sales:     "Sales Reports",
  customers: "Customers",
  settings:  "Settings",
};

export default function Topbar() {
  const { screen } = useApp();
  const [pending, setPending] = useState(0);

  useEffect(() => {
    let active = true;
    const poll = async () => {
      const n = await getPendingSyncCount();
      if (active) setPending(n);
    };
    poll();
    const id = setInterval(poll, 15_000);
    return () => { active = false; clearInterval(id); };
  }, []);

  const now = new Date();
  const dateStr = now.toLocaleDateString("en-IN", { day: "2-digit", month: "short", year: "numeric" });

  return (
    <div className="topbar">
      <span className="topbar-title">{TITLES[screen]}</span>
      <span className="topbar-badge">
        <span className={`sync-dot${pending > 0 ? " pending" : ""}`} />
        {pending > 0 ? `${pending} pending sync` : "Synced"}
      </span>
      <span className="topbar-badge">{dateStr}</span>
    </div>
  );
}
