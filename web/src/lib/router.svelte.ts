/** Minimal hash router: "#/learn/3" -> { page: "learn", param: "3" }. */
export const routes = ["home", "learn", "text", "snakes", "guess", "hood"] as const;
export type Page = (typeof routes)[number];

function parse(hash: string): { page: Page; param: string | null } {
  const [, page = "", param = null] = hash.replace(/^#/, "").split("/");
  return { page: (routes as readonly string[]).includes(page) ? (page as Page) : "home", param };
}

class Router {
  current = $state(parse(location.hash));

  constructor() {
    window.addEventListener("hashchange", () => {
      const prev = this.current.page;
      this.current = parse(location.hash);
      if (prev !== this.current.page) window.scrollTo({ top: 0 });
    });
  }
}

export const router = new Router();

export function href(page: Page, param?: string | number): string {
  return page === "home" ? "#/" : `#/${page}${param !== undefined ? `/${param}` : ""}`;
}
