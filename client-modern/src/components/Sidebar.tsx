import {
  IconLayoutDashboard, IconReceipt2, IconPackage, IconPill,
  IconShoppingCart, IconChartBar, IconUsers, IconSettings,
} from "@tabler/icons-react";
import { useApp, type Screen } from "../App";

interface NavEntry {
  id: Screen;
  label: string;
  icon: JSX.Element;
  section?: string;
}

const NAV: NavEntry[] = [
  { id: "dashboard",  label: "Dashboard",  icon: <IconLayoutDashboard size={17} />, section: "Main" },
  { id: "billing",    label: "Billing",    icon: <IconReceipt2 size={17} /> },
  { id: "inventory",  label: "Inventory",  icon: <IconPackage size={17} />, section: "Store" },
  { id: "medicines",  label: "Medicines",  icon: <IconPill size={17} /> },
  { id: "purchase",   label: "Purchase",   icon: <IconShoppingCart size={17} /> },
  { id: "sales",      label: "Sales",      icon: <IconChartBar size={17} />, section: "Reports" },
  { id: "customers",  label: "Customers",  icon: <IconUsers size={17} /> },
  { id: "settings",   label: "Settings",   icon: <IconSettings size={17} />, section: "System" },
];

export default function Sidebar() {
  const { screen, setScreen } = useApp();
  let lastSection = "";

  return (
    <aside className="sidebar">
      <div className="sidebar-logo">
        <div className="sidebar-logo-mark">A</div>
        <div>
          <div className="sidebar-logo-text">Aushadhalayam</div>
          <div className="sidebar-logo-sub">Pharmacy POS</div>
        </div>
      </div>
      <div className="sidebar-divider" />

      <ul className="sidebar-nav">
        {NAV.map(item => {
          const showSection = item.section && item.section !== lastSection;
          if (item.section) lastSection = item.section;
          return (
            <li key={item.id}>
              {showSection && (
                <div className="sidebar-section-label">{item.section}</div>
              )}
              <button
                className={`nav-item${screen === item.id ? " active" : ""}`}
                onClick={() => setScreen(item.id)}
              >
                {item.icon}
                {item.label}
              </button>
            </li>
          );
        })}
      </ul>

      <div className="sidebar-bottom">
        <button
          className={`nav-item${screen === "settings" ? " active" : ""}`}
          onClick={() => setScreen("settings")}
          style={{ fontSize: 12, color: "var(--sidebar-text)" }}
        >
          <IconSettings size={15} />
          v0.1.0
        </button>
      </div>
    </aside>
  );
}
