import { createContext, useCallback, useContext, useEffect, useState } from "react";
import Sidebar from "./components/Sidebar";
import Topbar from "./components/Topbar";
import Dashboard from "./screens/Dashboard";
import Billing from "./screens/Billing";
import Inventory from "./screens/Inventory";
import Medicines from "./screens/Medicines";
import Purchase from "./screens/Purchase";
import Sales from "./screens/Sales";
import Customers from "./screens/Customers";
import Settings from "./screens/Settings";

export type Screen =
  | "dashboard" | "billing" | "inventory" | "medicines"
  | "purchase" | "sales" | "customers" | "settings";

type Theme = "default" | "dark" | "emerald" | "rose" | "violet";

interface AppCtx {
  screen: Screen;
  setScreen: (s: Screen) => void;
  theme: Theme;
  setTheme: (t: Theme) => void;
  animEnabled: boolean;
  setAnimEnabled: (v: boolean) => void;
}

export const AppContext = createContext<AppCtx>({} as AppCtx);
export const useApp = () => useContext(AppContext);

const SCREENS: Record<Screen, JSX.Element> = {
  dashboard: <Dashboard />,
  billing:   <Billing />,
  inventory: <Inventory />,
  medicines: <Medicines />,
  purchase:  <Purchase />,
  sales:     <Sales />,
  customers: <Customers />,
  settings:  <Settings />,
};

export default function App() {
  const [screen, setScreen] = useState<Screen>("dashboard");
  const [theme, setThemeState] = useState<Theme>(() =>
    (localStorage.getItem("theme") as Theme) ?? "default"
  );
  const [animEnabled, setAnimState] = useState<boolean>(() =>
    localStorage.getItem("anim") !== "0"
  );

  const setTheme = useCallback((t: Theme) => {
    setThemeState(t);
    localStorage.setItem("theme", t);
  }, []);

  const setAnimEnabled = useCallback((v: boolean) => {
    setAnimState(v);
    localStorage.setItem("anim", v ? "1" : "0");
  }, []);

  useEffect(() => {
    document.documentElement.setAttribute("data-theme", theme);
  }, [theme]);

  useEffect(() => {
    document.documentElement.setAttribute("data-anim", animEnabled ? "1" : "0");
  }, [animEnabled]);

  return (
    <AppContext.Provider value={{ screen, setScreen, theme, setTheme, animEnabled, setAnimEnabled }}>
      <div className="app-shell">
        <Sidebar />
        <div className="main-area">
          <Topbar />
          <div className="screen">
            {SCREENS[screen]}
          </div>
        </div>
      </div>
    </AppContext.Provider>
  );
}
