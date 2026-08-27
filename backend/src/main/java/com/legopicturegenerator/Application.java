package com.legopicturegenerator;

import com.legopicturegenerator.api.JobRoutes;
import com.legopicturegenerator.api.UploadValidator;
import com.legopicturegenerator.application.JobService;
import com.legopicturegenerator.application.PipelineService;
import com.legopicturegenerator.config.AppConfig;
import com.legopicturegenerator.core.nativeengine.NativeEngine;
import com.legopicturegenerator.infrastructure.CatalogProvider;
import com.legopicturegenerator.infrastructure.FileJobRepository;
import io.javalin.Javalin;
import io.javalin.http.ForbiddenResponse;
import io.javalin.http.HttpResponseException;
import io.javalin.http.staticfiles.Location;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/** Local web entrypoint: wires catalog, jobs, pipeline, and HTTP together. */
public final class Application {
  private static final Logger log = LoggerFactory.getLogger(Application.class);
  private static final Path WEB_DIST = Path.of("web/dist");

  public static void main(String[] args) throws Exception {
    NativeEngine.ensureLoaded();
    AppConfig config = AppConfig.fromEnvironment();

    CatalogProvider catalogs = new CatalogProvider(config.dbPath());
    FileJobRepository repo = new FileJobRepository(config.jobsPath());
    repo.recoverInterruptedJobs();
    repo.enforceRetention();

    PipelineService pipeline = new PipelineService(catalogs);
    JobService jobs = new JobService(repo, pipeline, config.workerCount());
    Runtime.getRuntime().addShutdownHook(new Thread(jobs::shutdown));

    Javalin app = Javalin.create(cfg -> {
      cfg.showJavalinBanner = false;
      cfg.jetty.multipartConfig.maxFileSize(
          UploadValidator.MAX_UPLOAD_BYTES, io.javalin.config.SizeUnit.BYTES);
      cfg.jetty.multipartConfig.maxTotalRequestSize(
          UploadValidator.MAX_UPLOAD_BYTES + 1024 * 1024, io.javalin.config.SizeUnit.BYTES);
      if (Files.isDirectory(WEB_DIST)) {
        cfg.staticFiles.add(staticFiles -> {
          staticFiles.directory = WEB_DIST.toString();
          staticFiles.location = Location.EXTERNAL;
        });
        cfg.spaRoot.addFile("/", WEB_DIST.resolve("index.html").toString(), Location.EXTERNAL);
      }
    });

    // Loopback binding alone does not stop DNS-rebinding pages in the user's
    // browser from reaching this API; requiring a local Host header does.
    app.before(ctx -> {
      String host = ctx.header("Host");
      String bare = host == null ? "" : host.split(":", 2)[0];
      if (!bare.equals("localhost") && !bare.equals("127.0.0.1")) {
        throw new ForbiddenResponse("Only local requests are allowed");
      }
    });

    app.get("/api/v1/health", ctx -> ctx.json(Map.of(
        "status", "ok",
        "dbPath", catalogs.dbPath(),
        "paletteColors", catalogs.palette().size())));

    new JobRoutes(jobs, repo).register(app);

    app.exception(UploadValidator.InvalidUploadException.class, (e, ctx) ->
        ctx.status(400).json(Map.of("error", e.getMessage())));
    // Jetty raises this when the multipart size cap aborts an upload mid-stream
    app.exception(IllegalStateException.class, (e, ctx) -> {
      String msg = e.getMessage();
      if (msg != null && msg.contains("max filesize")) {
        ctx.status(400).json(Map.of("error", "Upload exceeds 25 MB limit"));
      } else {
        log.error("Unhandled state error", e);
        ctx.status(500).json(Map.of("error", "Internal error"));
      }
    });
    app.exception(JobService.QueueFullException.class, (e, ctx) ->
        ctx.status(429).json(Map.of("error", e.getMessage())));
    app.exception(HttpResponseException.class, (e, ctx) ->
        ctx.status(e.getStatus()).json(Map.of("error", e.getMessage())));
    app.exception(Exception.class, (e, ctx) -> {
      log.error("Unhandled error", e);
      ctx.status(500).json(Map.of("error", "Internal error"));
    });

    app.start("127.0.0.1", config.port());
    log.info("Lego Picture Generator listening on http://127.0.0.1:{}", config.port());
  }
}
