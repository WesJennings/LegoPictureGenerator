import React from "react";
import ReactDOM from "react-dom/client";
import { BrowserRouter, Route, Routes } from "react-router-dom";
import App from "./App";
import CreateJobPage from "./pages/CreateJobPage";
import JobPage from "./pages/JobPage";
import "./styles/app.css";

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <BrowserRouter>
      <Routes>
        <Route element={<App />}>
          <Route path="/" element={<CreateJobPage />} />
          <Route path="/jobs/:jobId" element={<JobPage />} />
        </Route>
      </Routes>
    </BrowserRouter>
  </React.StrictMode>,
);
