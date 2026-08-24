import { Link, Outlet } from "react-router-dom";

export default function App() {
  return (
    <div className="shell">
      <header className="topbar">
        <Link to="/" className="brand">
          Lego Picture Generator
        </Link>
        <span className="tagline">photo → buildable LEGO mosaic + parts list</span>
      </header>
      <main>
        <Outlet />
      </main>
    </div>
  );
}
