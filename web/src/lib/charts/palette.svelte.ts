/**
 * Chart colors resolved from the CSS tokens of the current theme, so canvas
 * and computed fills follow light/dark mode exactly like the CSS does.
 */
import { color, interpolateRgbBasis } from "d3";
import { theme } from "../theme.svelte";

function css(name: string): string {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

export interface ChartPalette {
  series: string[];
  ink: string;
  ink2: string;
  muted: string;
  surface: string;
  hairline: string;
  axis: string;
  accent: string;
  snake: string;
  ladder: string;
  /** Sequential ramp: 0 -> near the surface, 1 -> strongest. */
  seq: (t: number) => string;
}

class Palette {
  value = $derived.by<ChartPalette>(() => {
    void theme.effective; // recompute when the theme changes
    const ramp = interpolateRgbBasis(["--seq-0", "--seq-1", "--seq-3", "--seq-5", "--seq-7", "--seq-9"].map(css));
    return {
      series: ["--s1", "--s2", "--s3", "--s4", "--s5", "--s6", "--s7", "--s8"].map(css),
      ink: css("--ink-strong"),
      ink2: css("--ink-2"),
      muted: css("--muted"),
      surface: css("--surface"),
      hairline: css("--hairline"),
      axis: css("--axis"),
      accent: css("--accent"),
      snake: css("--s2"),
      ladder: css("--s3"),
      seq: (t: number) => ramp(Math.max(0, Math.min(1, t))),
    };
  });
}

export const palette = new Palette();

/** Ink (dark or light) that stays readable on top of `fill`. */
export function inkOn(fill: string): string {
  const c = color(fill)?.rgb();
  if (!c) return "#0b0b0b";
  const [r, g, b] = [c.r, c.g, c.b].map((v) => {
    const x = v / 255;
    return x <= 0.03928 ? x / 12.92 : ((x + 0.055) / 1.055) ** 2.4;
  });
  const lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  // Pick whichever ink has the higher WCAG contrast ratio against the fill.
  const onWhite = 1.05 / (lum + 0.05);
  const onBlack = (lum + 0.05) / 0.05;
  return onBlack >= onWhite ? "#0b0b0b" : "#ffffff";
}

export const fmtP = (p: number, digits = 2): string =>
  p === 0 ? "0" : p === 1 ? "1" : p < 0.005 && digits <= 2 ? p.toExponential(0) : p.toFixed(digits);
