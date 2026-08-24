package com.legopicturegenerator.infrastructure;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.legopicturegenerator.domain.JobConfig;
import com.legopicturegenerator.domain.JobManifest;
import com.legopicturegenerator.domain.JobStatus;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.util.stream.Stream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Filesystem-backed job store: one directory per job under the jobs root,
 * with an atomically written manifest.json as the source of truth.
 */
public final class FileJobRepository {
  private static final Logger log = LoggerFactory.getLogger(FileJobRepository.class);
  private static final Duration MAX_AGE = Duration.ofDays(7);
  private static final int MAX_JOBS = 50;
  private static final long MAX_TOTAL_BYTES = 2L * 1024 * 1024 * 1024;

  private final Path root;
  private final ObjectMapper json = new ObjectMapper();

  public FileJobRepository(Path root) throws IOException {
    this.root = root;
    Files.createDirectories(root);
  }

  public Path root() {
    return root;
  }

  /** Creates the job directory and initial manifest; enforces retention first. */
  public JobManifest createJob(
      int blockSize,
      String sizingMode,
      int targetStudCount,
      int targetPieceCount,
      boolean includeStudBom,
      int renderStudSizePx,
      List<String> modes)
      throws IOException {
    enforceRetention();
    JobManifest m = new JobManifest();
    m.id = UUID.randomUUID().toString();
    m.createdAtEpochMs = System.currentTimeMillis();
    m.updatedAtEpochMs = m.createdAtEpochMs;
    m.blockSize = blockSize;
    m.sizingMode = sizingMode != null ? sizingMode : JobConfig.SIZING_FIXED;
    m.autoSizing = JobConfig.SIZING_AUTO.equals(m.sizingMode);
    // Classic: aspect-preserving ~54-wide grid (not raw B=80, which collapses small photos).
    m.targetStudWidth = JobConfig.SIZING_FIXED.equals(m.sizingMode)
        ? JobConfig.CLASSIC_STUD_WIDTH
        : 0;
    m.targetStudCount = targetStudCount;
    m.targetPieceCount = targetPieceCount;
    m.includeStudBom = includeStudBom;
    m.renderStudSizePx = renderStudSizePx;
    m.modes = modes;
    Files.createDirectories(jobDir(m.id));
    saveManifest(m);
    return m;
  }

  public Path jobDir(String jobId) {
    return root.resolve(jobId);
  }

  public Path inputFile(String jobId) {
    return jobDir(jobId).resolve("input.png");
  }

  public synchronized void saveManifest(JobManifest m) throws IOException {
    m.updatedAtEpochMs = System.currentTimeMillis();
    Path dir = jobDir(m.id);
    Path tmp = dir.resolve("manifest.json.tmp");
    json.writerWithDefaultPrettyPrinter().writeValue(tmp.toFile(), m);
    Files.move(tmp, dir.resolve("manifest.json"),
        StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
  }

  public Optional<JobManifest> findJob(String jobId) {
    Path manifest = jobDir(jobId).resolve("manifest.json");
    if (!Files.isRegularFile(manifest)) {
      return Optional.empty();
    }
    try {
      return Optional.of(json.readValue(manifest.toFile(), JobManifest.class));
    } catch (IOException e) {
      log.warn("Unreadable manifest for job {}: {}", jobId, e.getMessage());
      return Optional.empty();
    }
  }

  public List<JobManifest> listJobs() {
    List<JobManifest> out = new ArrayList<>();
    try (Stream<Path> dirs = Files.list(root)) {
      dirs.filter(Files::isDirectory).forEach(dir ->
          findJob(dir.getFileName().toString()).ifPresent(out::add));
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
    out.sort(Comparator.comparingLong((JobManifest m) -> m.createdAtEpochMs).reversed());
    return out;
  }

  public void deleteJob(String jobId) throws IOException {
    Path dir = jobDir(jobId);
    if (!Files.isDirectory(dir)) {
      return;
    }
    try (Stream<Path> files = Files.walk(dir)) {
      for (Path p : files.sorted(Comparator.reverseOrder()).toList()) {
        Files.deleteIfExists(p);
      }
    }
  }

  /**
   * Startup recovery: any job persisted in a non-terminal state was interrupted
   * by a crash or restart — mark it FAILED, never resume half-finished work.
   */
  public void recoverInterruptedJobs() {
    for (JobManifest m : listJobs()) {
      if (!m.statusEnum().isTerminal()) {
        m.status = JobStatus.FAILED.name();
        m.error = "Interrupted by backend restart";
        try {
          saveManifest(m);
          log.info("Marked interrupted job {} as FAILED", m.id);
        } catch (IOException e) {
          log.warn("Could not recover job {}: {}", m.id, e.getMessage());
        }
      }
    }
  }

  /** TTL plus count/size caps, oldest completed jobs evicted first. */
  public void enforceRetention() {
    List<JobManifest> jobs = listJobs();
    Instant cutoff = Instant.now().minus(MAX_AGE);

    for (JobManifest m : jobs) {
      if (Instant.ofEpochMilli(m.createdAtEpochMs).isBefore(cutoff)
          && m.statusEnum().isTerminal()) {
        quietDelete(m.id, "older than " + MAX_AGE.toDays() + " days");
      }
    }

    List<JobManifest> remaining = listJobs();
    List<JobManifest> oldestFirst = new ArrayList<>(remaining);
    oldestFirst.sort(Comparator.comparingLong(m -> m.createdAtEpochMs));
    int count = remaining.size();
    long totalBytes = directoryBytes(root);
    for (JobManifest m : oldestFirst) {
      if (count < MAX_JOBS && totalBytes < MAX_TOTAL_BYTES) {
        break;
      }
      if (!m.statusEnum().isTerminal()) {
        continue;
      }
      long freed = directoryBytes(jobDir(m.id));
      quietDelete(m.id, "retention cap");
      count--;
      totalBytes -= freed;
    }
  }

  private void quietDelete(String jobId, String reason) {
    try {
      deleteJob(jobId);
      log.info("Evicted job {} ({})", jobId, reason);
    } catch (IOException e) {
      log.warn("Could not evict job {}: {}", jobId, e.getMessage());
    }
  }

  private static long directoryBytes(Path dir) {
    try (Stream<Path> files = Files.walk(dir)) {
      return files.filter(Files::isRegularFile).mapToLong(p -> {
        try {
          return Files.size(p);
        } catch (IOException e) {
          return 0L;
        }
      }).sum();
    } catch (IOException e) {
      return 0L;
    }
  }
}
