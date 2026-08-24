package com.legopicturegenerator.api;

import com.legopicturegenerator.application.JobService;
import com.legopicturegenerator.domain.JobConfig;
import com.legopicturegenerator.domain.JobManifest;
import com.legopicturegenerator.domain.PackMode;
import com.legopicturegenerator.infrastructure.FileJobRepository;
import io.javalin.Javalin;
import io.javalin.http.BadRequestResponse;
import io.javalin.http.ConflictResponse;
import io.javalin.http.Context;
import io.javalin.http.NotFoundResponse;
import io.javalin.http.UploadedFile;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.EnumSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

/** HTTP surface for jobs: create, poll, fetch artifacts, delete. */
public final class JobRoutes {
  private final JobService jobs;
  private final FileJobRepository repo;

  public JobRoutes(JobService jobs, FileJobRepository repo) {
    this.jobs = jobs;
    this.repo = repo;
  }

  public void register(Javalin app) {
    app.post("/api/v1/jobs", this::createJob);
    app.get("/api/v1/jobs/{jobId}", this::getJob);
    app.get("/api/v1/jobs/{jobId}/artifacts/{artifactName}", this::getArtifact);
    app.delete("/api/v1/jobs/{jobId}", this::deleteJob);
  }

  private void createJob(Context ctx) throws IOException {
    UploadedFile upload = ctx.uploadedFile("image");
    if (upload == null) {
      throw new BadRequestResponse("Multipart field 'image' is required");
    }

    String sizingMode = parseSizingMode(ctx.formParam("sizing"));
    Set<PackMode> modes = JobConfig.SIZING_PIECES.equals(sizingMode)
        ? EnumSet.of(PackMode.DLX)
        : parseModes(ctx.formParam("modes"));

    int targetStudCount = JobConfig.SIZING_AUTO.equals(sizingMode)
        ? parseTargetStudCount(ctx.formParam("targetStudCount"))
        : 0;
    int targetPieceCount = JobConfig.SIZING_PIECES.equals(sizingMode)
        ? parseTargetPieceCount(ctx.formParam("targetPieceCount"))
        : 0;
    boolean includeStudBom = parseIncludeStudBom(ctx.formParam("includeStudBom"));

    JobManifest manifest = jobs.submit(
        modes, sizingMode, targetStudCount, targetPieceCount, includeStudBom);
    Path input = repo.inputFile(manifest.id);
    try (InputStream in = upload.content();
         OutputStream out = Files.newOutputStream(input)) {
      long copied = 0;
      byte[] buf = new byte[64 * 1024];
      int n;
      while ((n = in.read(buf)) > 0) {
        copied += n;
        if (copied > UploadValidator.MAX_UPLOAD_BYTES) {
          throw new UploadValidator.InvalidUploadException("Upload exceeds 25 MB limit");
        }
        out.write(buf, 0, n);
      }
    } catch (RuntimeException e) {
      repo.deleteJob(manifest.id);
      throw e;
    }

    try {
      UploadValidator.validate(input);
    } catch (RuntimeException e) {
      repo.deleteJob(manifest.id);
      throw e;
    }

    jobs.enqueue(manifest, modes);
    ctx.status(202).json(Map.of(
        "jobId", manifest.id,
        "statusUrl", "/api/v1/jobs/" + manifest.id));
  }

  private void getJob(Context ctx) {
    JobManifest m = requireJob(ctx);
    ctx.json(toResponse(m));
  }

  private void getArtifact(Context ctx) throws IOException {
    JobManifest m = requireJob(ctx);
    String name = ctx.pathParam("artifactName");
    if (!m.artifacts.containsValue(name)) {
      throw new NotFoundResponse("Unknown artifact");
    }
    Path file = repo.jobDir(m.id).resolve(name).normalize();
    if (!file.startsWith(repo.jobDir(m.id)) || !Files.isRegularFile(file)) {
      throw new NotFoundResponse("Unknown artifact");
    }

    String contentType = name.endsWith(".png") ? "image/png"
        : name.endsWith(".json") ? "application/json"
        : "text/plain; charset=utf-8";
    ctx.contentType(contentType);
    ctx.header("X-Content-Type-Options", "nosniff");
    if (!name.endsWith(".png")) {
      ctx.header("Content-Disposition", "attachment; filename=\"" + name + "\"");
    }
    ctx.result(Files.newInputStream(file));
  }

  private void deleteJob(Context ctx) throws IOException {
    JobManifest m = requireJob(ctx);
    if (!jobs.deleteJob(m.id)) {
      throw new ConflictResponse("Job is still queued or running");
    }
    ctx.status(204);
  }

  private JobManifest requireJob(Context ctx) {
    String jobId = ctx.pathParam("jobId");
    try {
      UUID.fromString(jobId);
    } catch (IllegalArgumentException e) {
      throw new NotFoundResponse("No such job");
    }
    return jobs.findJob(jobId).orElseThrow(() -> new NotFoundResponse("No such job"));
  }

  private static Set<PackMode> parseModes(String raw) {
    if (raw == null || raw.isBlank()) {
      return EnumSet.of(PackMode.GREEDY);
    }
    Set<PackMode> modes = EnumSet.noneOf(PackMode.class);
    for (String part : raw.split(",")) {
      try {
        modes.add(PackMode.fromModeName(part.trim()));
      } catch (IllegalArgumentException e) {
        throw new BadRequestResponse("Unknown pack mode: " + part.trim());
      }
    }
    return modes;
  }

  /** {@code fixed} (default), {@code auto}, or {@code pieces}. */
  private static String parseSizingMode(String raw) {
    if (raw == null || raw.isBlank()) {
      return JobConfig.SIZING_FIXED;
    }
    String v = raw.trim().toLowerCase();
    return switch (v) {
      case "fixed", "auto", "pieces" -> v;
      default -> throw new BadRequestResponse(
          "sizing must be fixed, auto, or pieces");
    };
  }

  private static int parseTargetStudCount(String raw) {
    if (raw == null || raw.isBlank()) {
      return JobConfig.DEFAULT_TARGET_STUDS;
    }
    int value;
    try {
      value = Integer.parseInt(raw.trim());
    } catch (NumberFormatException e) {
      throw new BadRequestResponse("targetStudCount must be a number");
    }
    if (value < JobConfig.MIN_TARGET_STUDS || value > JobConfig.MAX_TARGET_STUDS) {
      throw new BadRequestResponse("targetStudCount must be between "
          + JobConfig.MIN_TARGET_STUDS + " and " + JobConfig.MAX_TARGET_STUDS);
    }
    return value;
  }

  /** Accepts {@code true}/{@code 1}/{@code on}/{@code yes}. */
  private static boolean parseIncludeStudBom(String raw) {
    if (raw == null || raw.isBlank()) {
      return false;
    }
    String v = raw.trim().toLowerCase();
    return v.equals("true") || v.equals("1") || v.equals("on") || v.equals("yes");
  }

  private static int parseTargetPieceCount(String raw) {
    if (raw == null || raw.isBlank()) {
      return JobConfig.DEFAULT_TARGET_PIECES;
    }
    int value;
    try {
      value = Integer.parseInt(raw.trim());
    } catch (NumberFormatException e) {
      throw new BadRequestResponse("targetPieceCount must be a number");
    }
    if (value < JobConfig.MIN_TARGET_PIECES || value > JobConfig.MAX_TARGET_PIECES) {
      throw new BadRequestResponse("targetPieceCount must be between "
          + JobConfig.MIN_TARGET_PIECES + " and " + JobConfig.MAX_TARGET_PIECES);
    }
    return value;
  }

  private Map<String, Object> toResponse(JobManifest m) {
    Map<String, String> artifactUrls = new LinkedHashMap<>();
    for (Map.Entry<String, String> e : m.artifacts.entrySet()) {
      artifactUrls.put(e.getKey(), "/api/v1/jobs/" + m.id + "/artifacts/" + e.getValue());
    }
    Map<String, Object> settings = new LinkedHashMap<>();
    settings.put("blockSize", m.blockSize);
    settings.put("targetStudWidth", m.targetStudWidth);
    settings.put("sizingMode", m.effectiveSizingMode());
    settings.put("autoSizing", m.isAutoStudSizing());
    settings.put("targetStudCount", m.targetStudCount);
    settings.put("targetPieceCount", m.targetPieceCount);
    settings.put("pieceSearchAttempts", m.pieceSearchAttempts);
    settings.put("achievedPieceCount", m.achievedPieceCount);
    settings.put("includeStudBom", m.includeStudBom);
    settings.put("renderStudSizePx", m.renderStudSizePx);
    settings.put("modes", m.modes);

    Map<String, Object> out = new LinkedHashMap<>();
    out.put("jobId", m.id);
    out.put("status", m.status);
    out.put("error", m.error);
    out.put("createdAtEpochMs", m.createdAtEpochMs);
    out.put("updatedAtEpochMs", m.updatedAtEpochMs);
    out.put("settings", settings);
    out.put("result", m.result);
    out.put("artifacts", artifactUrls);
    return out;
  }
}
