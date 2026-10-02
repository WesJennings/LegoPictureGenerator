# syntax=docker/dockerfile:1

# ---------- Stage 1: React UI ----------
FROM node:20-bookworm-slim AS web
WORKDIR /src/web
COPY web/package.json web/package-lock.json ./
RUN npm ci
COPY web/ ./
RUN npm run build

# ---------- Stage 2: C++ host + engine ----------
FROM debian:bookworm-slim AS native
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY native/ native/
RUN cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build native/build -j"$(nproc)" --target lego_server lego_cli

# ---------- Stage 3: runtime ----------
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
      libsqlite3-0 ca-certificates curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --home /app lego \
    && mkdir -p /app /jobs /data \
    && chown -R lego:lego /app /jobs /data
WORKDIR /app
COPY --from=native /src/native/build/lego_server /src/native/build/lego_cli /app/
COPY --from=web /src/web/dist /app/web/dist

USER lego
ENV LEGO_BIND=0.0.0.0 \
    LEGO_PORT=8080 \
    LEGO_DB_PATH=/data/bricks.db \
    LEGO_JOBS_PATH=/jobs \
    LEGO_WEB_DIST=/app/web/dist
EXPOSE 8080
VOLUME ["/jobs"]
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s \
  CMD curl -fsS -H "Host: localhost" http://127.0.0.1:8080/api/v1/health || exit 1
ENTRYPOINT ["/app/lego_server"]
