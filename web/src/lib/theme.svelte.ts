/**
 * Light/dark theme, shared with the portfolio (next-themes) and Pixel Lab:
 * the same `theme` localStorage key and `.dark` class on <html>, default
 * light. index.html applies it before first paint.
 */
type Mode = "light" | "dark";

function stored(): Mode {
  try {
    return localStorage.getItem("theme") === "dark" ? "dark" : "light";
  } catch {
    return "light";
  }
}

function apply(mode: Mode): void {
  const root = document.documentElement;
  root.classList.toggle("dark", mode === "dark");
  root.classList.toggle("light", mode === "light");
  root.style.colorScheme = mode;
  document.querySelector('meta[name="theme-color"]')?.setAttribute("content", mode === "dark" ? "#000000" : "#f5f5f7");
}

class Theme {
  effective = $state<Mode>(stored());

  toggle(): void {
    const next: Mode = this.effective === "dark" ? "light" : "dark";
    const commit = () => {
      this.effective = next;
      apply(next);
      try {
        localStorage.setItem("theme", next);
      } catch {
        /* private mode: the choice just isn't remembered */
      }
    };
    // Cross-fade the whole page as one snapshot, like the portfolio's toggle.
    const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    const doc = document as Document & { startViewTransition?: (cb: () => void) => unknown };
    if (doc.startViewTransition && !reduce) doc.startViewTransition(commit);
    else commit();
  }
}

export const theme = new Theme();
