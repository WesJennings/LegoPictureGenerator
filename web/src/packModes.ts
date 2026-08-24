/** Pack algorithms offered in the UI — matches backend PackMode.modeName. */
export interface PackModeOption {
  id: string;
  label: string;
  blurb: string;
}

export const PACK_MODES: PackModeOption[] = [
  {
    id: "greedy",
    label: "Greedy",
    blurb: "Fast largest-first packing — good default.",
  },
  {
    id: "ilp",
    label: "ILP (integer linear programming)",
    blurb: "Most accurate; slower, may fall back on large regions.",
  },
  {
    id: "rle",
    label: "RLE (run-length encoding)",
    blurb: "Row-run packing — simple and quick.",
  },
  {
    id: "component",
    label: "Component",
    blurb: "Greedy per connected same-color blob.",
  },
  {
    id: "dlx",
    label: "DLX (dancing links exact cover)",
    blurb: "Dancing Links exact cover; similar tradeoffs to ILP.",
  },
  {
    id: "anneal",
    label: "Anneal",
    blurb: "Simulated annealing from a greedy seed.",
  },
];

/** Special UI choice: run every algorithm and show a side-by-side page. */
export const COMPARE_ALL_ID = "compare";

export const COMPARE_ALL_OPTION: PackModeOption = {
  id: COMPARE_ALL_ID,
  label: "Compare all",
  blurb: "Run every algorithm and compare images + stats side by side.",
};

/** Dropdown order: Compare all first, then individual algorithms. */
export const PACK_MODE_OPTIONS: PackModeOption[] = [
  COMPARE_ALL_OPTION,
  ...PACK_MODES,
];

export const DEFAULT_PACK_MODE = "greedy";

export const ALL_PACK_MODE_IDS = PACK_MODES.map((m) => m.id);

/** Form value for the API `modes` field. */
export function modesForRequest(selection: string): string {
  if (selection === COMPARE_ALL_ID) {
    return ALL_PACK_MODE_IDS.join(",");
  }
  return selection;
}

/** Artifact map keys use camelCase with a capitalized mode name (legoGreedy). */
export function artifactKey(prefix: string, mode: string): string {
  return prefix + mode.charAt(0).toUpperCase() + mode.slice(1);
}

export function packModeLabel(mode: string): string {
  if (mode === COMPARE_ALL_ID) {
    return COMPARE_ALL_OPTION.label;
  }
  return PACK_MODES.find((m) => m.id === mode)?.label ?? mode;
}

export function isCompareJob(modes: string[]): boolean {
  return modes.length > 1;
}
