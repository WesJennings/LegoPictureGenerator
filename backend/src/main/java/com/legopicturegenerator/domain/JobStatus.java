package com.legopicturegenerator.domain;

/** Lifecycle of one mosaic job. Terminal states: COMPLETE, FAILED. */
public enum JobStatus {
  QUEUED,
  /** Piece-target search: repeated DLX probes before the final pipeline run. */
  SEARCHING,
  PREPROCESSING,
  MATCHING,
  PACKING,
  RENDERING,
  COMPLETE,
  FAILED;

  public boolean isTerminal() {
    return this == COMPLETE || this == FAILED;
  }
}
