import { useEffect, useId, useRef, useState } from "react";
import {
  DEFAULT_PACK_MODE,
  PACK_MODE_OPTIONS,
} from "../packModes";

interface Props {
  mode: string;
  onMode: (mode: string) => void;
}

export default function PackModeSelect({ mode, onMode }: Props) {
  const [open, setOpen] = useState(false);
  const rootRef = useRef<HTMLDivElement>(null);
  const listId = useId();
  const selected =
    PACK_MODE_OPTIONS.find((m) => m.id === mode) ??
    PACK_MODE_OPTIONS.find((m) => m.id === DEFAULT_PACK_MODE)!;

  useEffect(() => {
    if (!open) {
      return;
    }
    function onPointerDown(e: MouseEvent) {
      if (rootRef.current && !rootRef.current.contains(e.target as Node)) {
        setOpen(false);
      }
    }
    function onKey(e: KeyboardEvent) {
      if (e.key === "Escape") {
        setOpen(false);
      }
    }
    document.addEventListener("mousedown", onPointerDown);
    document.addEventListener("keydown", onKey);
    return () => {
      document.removeEventListener("mousedown", onPointerDown);
      document.removeEventListener("keydown", onKey);
    };
  }, [open]);

  return (
    <div className="settings pack-mode" ref={rootRef}>
      <span className="settings-label" id={`${listId}-label`}>
        Packing algorithm
      </span>
      <div className="dropdown">
        <button
          type="button"
          className={`dropdown-trigger ${open ? "open" : ""}`}
          aria-haspopup="listbox"
          aria-expanded={open}
          aria-controls={listId}
          aria-labelledby={`${listId}-label`}
          onClick={() => setOpen((v) => !v)}
        >
          <span>{selected.label}</span>
          <span className="dropdown-caret" aria-hidden="true" />
        </button>
        {open && (
          <ul
            id={listId}
            className="dropdown-menu"
            role="listbox"
            aria-labelledby={`${listId}-label`}
          >
            {PACK_MODE_OPTIONS.map((m) => {
              const active = m.id === selected.id;
              return (
                <li key={m.id} role="option" aria-selected={active}>
                  <button
                    type="button"
                    className={`dropdown-option ${active ? "active" : ""}`}
                    onClick={() => {
                      onMode(m.id);
                      setOpen(false);
                    }}
                  >
                    <span className="dropdown-option-label">{m.label}</span>
                    <span className="dropdown-option-blurb">{m.blurb}</span>
                  </button>
                </li>
              );
            })}
          </ul>
        )}
      </div>
      <p className="muted">{selected.blurb}</p>
    </div>
  );
}
