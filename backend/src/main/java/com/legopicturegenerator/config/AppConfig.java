package com.legopicturegenerator.config;

import java.nio.file.Path;

/** Runtime configuration from environment variables with repo-local defaults. */
public record AppConfig(Path dbPath, Path jobsPath, int port, int workerCount) {

  public static AppConfig fromEnvironment() {
    return new AppConfig(
        Path.of(env("LEGO_DB_PATH", "data/bricks.db")),
        Path.of(env("LEGO_JOBS_PATH", "runtime/jobs")),
        Integer.parseInt(env("LEGO_PORT", "8080")),
        Integer.parseInt(env("LEGO_WORKER_COUNT", "1")));
  }

  private static String env(String name, String fallback) {
    String value = System.getenv(name);
    return (value == null || value.isBlank()) ? fallback : value;
  }
}
