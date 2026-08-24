package com.legopicturegenerator.application;

import com.legopicturegenerator.core.image.ImageSampler;
import com.legopicturegenerator.domain.JobConfig;
import com.legopicturegenerator.domain.JobManifest;
import com.legopicturegenerator.domain.JobStatus;
import com.legopicturegenerator.domain.PackMode;
import com.legopicturegenerator.domain.PipelineResult;
import com.legopicturegenerator.infrastructure.FileJobRepository;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.time.Duration;
import java.util.EnumSet;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;
import javax.imageio.ImageIO;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Owns the job lifecycle: bounded queue, background execution,
 * per-job timeout watchdog, and manifest updates.
 */
public final class JobService {
  private static final Logger log = LoggerFactory.getLogger(JobService.class);
  private static final int QUEUE_CAPACITY = 8;
  /** Single-mode default; multi-mode (compare all) gets more time per algorithm. */
  private static final Duration JOB_TIMEOUT = Duration.ofSeconds(120);
  private static final Duration PER_MODE_BUDGET = Duration.ofSeconds(90);
  private static final Duration MAX_JOB_TIMEOUT = Duration.ofMinutes(10);

  private final FileJobRepository repo;
  private final PipelineService pipeline;
  private final ThreadPoolExecutor worker;
  private final ExecutorService pipelineRunner;

  /** Thrown when the queue is full; maps to HTTP 429. */
  public static final class QueueFullException extends RuntimeException {
    public QueueFullException() {
      super("Job queue is full; try again shortly");
    }
  }

  public JobService(FileJobRepository repo, PipelineService pipeline, int workerCount) {
    this.repo = repo;
    this.pipeline = pipeline;
    this.worker = new ThreadPoolExecutor(
        workerCount, workerCount, 0L, TimeUnit.MILLISECONDS,
        new LinkedBlockingQueue<>(QUEUE_CAPACITY),
        r -> {
          Thread t = new Thread(r, "job-worker");
          t.setDaemon(true);
          return t;
        },
        new ThreadPoolExecutor.AbortPolicy());
    this.pipelineRunner = Executors.newCachedThreadPool(r -> {
      Thread t = new Thread(r, "pipeline");
      t.setDaemon(true);
      return t;
    });
  }

  /**
   * Creates the manifest. Upload is already on disk.
   *
   * @param sizingMode {@code fixed}, {@code auto}, or {@code pieces}
   * @param targetStudCount used when sizingMode is auto
   * @param targetPieceCount used when sizingMode is pieces
   */
  public JobManifest submit(
      Set<PackMode> modes,
      String sizingMode,
      int targetStudCount,
      int targetPieceCount,
      boolean includeStudBom) {
    List<String> modeNames = modes.stream().map(m -> m.modeName).collect(Collectors.toList());
    try {
      return repo.createJob(
          JobConfig.DEFAULT_BLOCK_SIZE,
          sizingMode,
          JobConfig.SIZING_AUTO.equals(sizingMode) ? targetStudCount : 0,
          JobConfig.SIZING_PIECES.equals(sizingMode) ? targetPieceCount : 0,
          includeStudBom,
          JobConfig.DEFAULT_RENDER_STUD_PX,
          modeNames);
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  /** Queue the created job for execution; call after the input file is stored. */
  public void enqueue(JobManifest manifest, Set<PackMode> modes) {
    try {
      worker.execute(() -> execute(manifest.id, modes));
    } catch (RejectedExecutionException e) {
      try {
        repo.deleteJob(manifest.id);
      } catch (IOException cleanup) {
        log.warn("Could not clean up rejected job {}", manifest.id);
      }
      throw new QueueFullException();
    }
  }

  private void execute(String jobId, Set<PackMode> modes) {
    Optional<JobManifest> loaded = repo.findJob(jobId);
    if (loaded.isEmpty()) {
      return;
    }
    JobManifest manifest = loaded.get();

    try {
      resolveSizing(manifest);
    } catch (Exception e) {
      updateStatus(manifest, JobStatus.FAILED, userSafeMessage(e));
      return;
    }

    Set<PackMode> runModes = manifest.isPieceSizing()
        ? EnumSet.of(PackMode.DLX)
        : modes;

    int studWidth = manifest.targetStudWidth > 0
        ? manifest.targetStudWidth
        : (JobConfig.SIZING_FIXED.equals(manifest.effectiveSizingMode())
            ? JobConfig.CLASSIC_STUD_WIDTH
            : 0);
    JobConfig cfg = new JobConfig(
        repo.inputFile(jobId),
        repo.jobDir(jobId),
        manifest.blockSize,
        studWidth,
        manifest.renderStudSizePx,
        runModes,
        manifest.includeStudBom);

    PipelineService.ProgressListener progress = stage -> updateStatus(manifest, stage, null);

    Duration timeout = timeoutFor(runModes.size(), manifest.isPieceSizing());
    Future<PipelineResult> future = pipelineRunner.submit(() -> pipeline.run(cfg, progress));
    try {
      PipelineResult result = future.get(timeout.toMillis(), TimeUnit.MILLISECONDS);
      manifest.result = result;
      manifest.artifacts.putAll(result.artifacts());
      if (manifest.isPieceSizing() && result.modeResults() != null && !result.modeResults().isEmpty()) {
        manifest.achievedPieceCount = result.modeResults().get(0).pieceCount();
      }
      updateStatus(manifest, JobStatus.COMPLETE, null);
    } catch (TimeoutException e) {
      future.cancel(true);
      updateStatus(manifest, JobStatus.FAILED,
          "Timed out after " + timeout.toSeconds() + "s");
    } catch (Exception e) {
      Throwable cause = e.getCause() != null ? e.getCause() : e;
      log.error("Job {} failed", jobId, cause);
      updateStatus(manifest, JobStatus.FAILED, userSafeMessage(cause));
    }
  }

  private void updateStatus(JobManifest manifest, JobStatus status, String error) {
    manifest.status = status.name();
    manifest.error = error;
    try {
      repo.saveManifest(manifest);
    } catch (IOException e) {
      log.error("Could not persist manifest for job {}", manifest.id, e);
    }
  }

  private static String userSafeMessage(Throwable cause) {
    String msg = cause.getMessage();
    return (msg == null || msg.isBlank()) ? "Processing failed" : msg;
  }

  /** Compare-all and piece-search need more wall time. */
  static Duration timeoutFor(int modeCount, boolean pieceSearch) {
    if (pieceSearch) {
      return MAX_JOB_TIMEOUT;
    }
    if (modeCount <= 1) {
      return JOB_TIMEOUT;
    }
    Duration scaled = PER_MODE_BUDGET.multipliedBy(modeCount);
    return scaled.compareTo(MAX_JOB_TIMEOUT) > 0 ? MAX_JOB_TIMEOUT : scaled;
  }

  /**
   * Resolve {@code manifest.blockSize} for auto stud sizing or piece-target search.
   */
  private void resolveSizing(JobManifest manifest) throws Exception {
    if (manifest.isPieceSizing()) {
      resolvePieceTarget(manifest);
      return;
    }
    if (!manifest.isAutoStudSizing() || manifest.targetStudCount < 1) {
      return;
    }
    BufferedImage src = ImageIO.read(repo.inputFile(manifest.id).toFile());
    if (src == null) {
      throw new IOException("Could not decode input image");
    }
    manifest.blockSize = ImageSampler.blockSizeForTargetStuds(
        src.getWidth(), src.getHeight(), manifest.targetStudCount);
    repo.saveManifest(manifest);
  }

  private void resolvePieceTarget(JobManifest manifest) throws Exception {
    updateStatus(manifest, JobStatus.SEARCHING, null);
    BufferedImage src = ImageIO.read(repo.inputFile(manifest.id).toFile());
    if (src == null) {
      throw new IOException("Could not decode input image");
    }
    int target = manifest.targetPieceCount > 0
        ? manifest.targetPieceCount
        : JobConfig.DEFAULT_TARGET_PIECES;

    PieceTargetSolver.Result search = PieceTargetSolver.solve(
        src.getWidth(),
        src.getHeight(),
        target,
        blockSize -> pipeline.probePieceCount(src, blockSize, PackMode.DLX));

    manifest.blockSize = search.blockSize();
    manifest.targetStudCount = search.studTargetUsed();
    manifest.pieceSearchAttempts = search.attempts();
    manifest.achievedPieceCount = search.bestPieceCount();
    manifest.modes = List.of(PackMode.DLX.modeName);
    repo.saveManifest(manifest);
  }

  public Optional<JobManifest> findJob(String jobId) {
    return repo.findJob(jobId);
  }

  /** Deletes a terminal job; returns false if the job is still queued or running. */
  public boolean deleteJob(String jobId) throws IOException {
    Optional<JobManifest> m = repo.findJob(jobId);
    if (m.isEmpty()) {
      return true;
    }
    if (!m.get().statusEnum().isTerminal()) {
      return false;
    }
    repo.deleteJob(jobId);
    return true;
  }

  public void shutdown() {
    worker.shutdownNow();
    pipelineRunner.shutdownNow();
  }
}
