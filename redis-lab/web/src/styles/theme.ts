import { theme } from 'antd';

// ---- Brand Colors ----
const blue = {
  primary: '#3b82f6',
  hover: '#2563eb',
  active: '#1d4ed8',
  bg: 'rgba(59,130,246,0.12)',
  border: 'rgba(59,130,246,0.25)',
};
const green = { primary: '#22c55e', bg: 'rgba(34,197,94,0.10)' };
const red = { primary: '#ef4444', bg: 'rgba(239,68,68,0.10)' };
const orange = { primary: '#f59e0b', bg: 'rgba(245,158,11,0.10)' };
const purple = { primary: '#a855f7', bg: 'rgba(168,85,247,0.10)' };

// ---- Dark Theme Tokens ----
export const darkTokens = {
  algorithm: theme.darkAlgorithm,
  token: {
    colorPrimary: blue.primary,
    colorSuccess: green.primary,
    colorWarning: orange.primary,
    colorError: red.primary,
    colorInfo: blue.primary,
    colorBgBase: '#0c0d14',
    colorBgContainer: '#11131a',
    colorBgElevated: '#161922',
    colorBgLayout: '#0a0b10',
    colorBgSpotlight: '#1a1d28',
    colorBorder: '#252836',
    colorBorderSecondary: '#1c1f2d',
    colorText: '#e1e4ed',
    colorTextSecondary: '#8b8fa8',
    colorTextTertiary: '#595e73',
    colorTextQuaternary: '#3d4055',
    borderRadius: 6,
    borderRadiusLG: 8,
    borderRadiusSM: 4,
    fontFamily: `-apple-system, BlinkMacSystemFont, 'Inter', 'Segoe UI', Roboto, sans-serif`,
    fontFamilyCode: `'JetBrains Mono', 'Fira Code', 'Cascadia Code', Consolas, monospace`,
    fontSize: 13,
    fontSizeSM: 12,
    fontSizeLG: 14,
    fontSizeHeading1: 28,
    fontSizeHeading2: 22,
    fontSizeHeading3: 18,
    fontSizeHeading4: 16,
    fontSizeHeading5: 14,
    lineHeight: 1.5714,
    controlHeight: 32,
    controlHeightSM: 26,
    controlHeightLG: 40,
    paddingXS: 4,
    paddingSM: 8,
    padding: 12,
    paddingMD: 16,
    paddingLG: 20,
    paddingXL: 24,
    marginXS: 4,
    marginSM: 8,
    margin: 12,
    marginMD: 16,
    marginLG: 20,
    marginXL: 24,
  },
  components: {
    Layout: {
      headerBg: '#11131a',
      headerHeight: 44,
      siderBg: '#0c0d14',
      bodyBg: '#0a0b10',
      triggerBg: '#1a1d28',
      triggerColor: '#8b8fa8',
    },
    Menu: {
      darkItemBg: 'transparent',
      darkItemSelectedBg: blue.bg,
      darkSubMenuItemBg: 'transparent',
      itemBorderRadius: 4,
      itemMarginInline: 4,
      itemHeight: 32,
      iconSize: 16,
      collapsedIconSize: 18,
    },
    Button: {
      primaryShadow: 'none',
      defaultShadow: 'none',
      dangerShadow: 'none',
      contentFontSize: 13,
      contentFontSizeSM: 12,
      onlyIconSizeSM: 14,
      paddingInline: 14,
      paddingInlineSM: 10,
      controlHeight: 32,
      controlHeightSM: 26,
    },
    Input: {
      activeShadow: `0 0 0 2px ${blue.border}`,
      paddingInline: 12,
      controlHeight: 32,
      controlHeightSM: 26,
    },
    Select: {
      controlHeight: 32,
    },
    Table: {
      headerBg: '#11131a',
      headerColor: '#8b8fa8',
      rowHoverBg: 'rgba(255,255,255,0.02)',
      borderColor: '#252836',
      cellPaddingBlock: 8,
      cellPaddingInline: 14,
      fontSize: 12,
    },
    Tree: {
      indentSize: 16,
      titleHeight: 28,
      directoryNodeSelectedBg: blue.bg,
      directoryNodeSelectedColor: blue.primary,
    },
    Tabs: {
      cardBg: '#11131a',
      cardGutter: 2,
      horizontalItemPadding: '8px 14px',
      horizontalMargin: '0',
      inkBarColor: blue.primary,
      itemColor: '#8b8fa8',
      itemHoverColor: '#e1e4ed',
      itemSelectedColor: blue.primary,
    },
    Card: {
      paddingLG: 20,
    },
    Modal: {
      contentBg: '#11131a',
      headerBg: '#11131a',
    },
    Tooltip: {
      colorBgSpotlight: '#1a1d28',
      colorTextLightSolid: '#e1e4ed',
    },
    Dropdown: {
      controlPaddingHorizontal: 8,
    },
  },
};

// ---- Light Theme ----
export const lightTokens = {
  algorithm: theme.defaultAlgorithm,
  token: {
    colorPrimary: blue.primary,
    colorSuccess: green.primary,
    colorWarning: orange.primary,
    colorError: red.primary,
    borderRadius: 6,
    fontFamily: `-apple-system, BlinkMacSystemFont, 'Inter', 'Segoe UI', Roboto, sans-serif`,
    fontSize: 13,
    controlHeight: 32,
  },
  components: {
    Layout: { headerHeight: 44 },
    Menu: { itemBorderRadius: 4, itemMarginInline: 4, itemHeight: 32 },
    Button: { primaryShadow: 'none' },
    Input: { activeShadow: `0 0 0 2px ${blue.border}` },
  },
};

// ---- Shared Brand Colors ----
export const brand = { blue, green, red, orange, purple };
