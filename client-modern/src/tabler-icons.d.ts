// Ambient stub for @tabler/icons-react v3.44.0.
// The installed package's "types" field points to a file that doesn't exist in
// the published tarball. This file provides hand-written declarations for every
// icon actually used in this project. It must have NO top-level imports so that
// TypeScript treats it as a script (ambient declarations), not a module.

type _TablerIconProps = {
  size?: number | string;
  stroke?: number | string;
  color?: string;
  className?: string;
  style?: import("react").CSSProperties;
  title?: string;
};

type _TablerIcon = (props: _TablerIconProps) => import("react").ReactElement | null;

declare module "@tabler/icons-react" {
  export const IconAlertTriangle: _TablerIcon;
  export const IconChartBar: _TablerIcon;
  export const IconCurrencyRupee: _TablerIcon;
  export const IconDownload: _TablerIcon;
  export const IconLayoutDashboard: _TablerIcon;
  export const IconPackage: _TablerIcon;
  export const IconPill: _TablerIcon;
  export const IconPlus: _TablerIcon;
  export const IconReceipt2: _TablerIcon;
  export const IconSearch: _TablerIcon;
  export const IconSettings: _TablerIcon;
  export const IconShoppingCart: _TablerIcon;
  export const IconTrash: _TablerIcon;
  export const IconTrendingUp: _TablerIcon;
  export const IconUsers: _TablerIcon;
  export const IconUserPlus: _TablerIcon;
  export const IconFilter: _TablerIcon;
}
