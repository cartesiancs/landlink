# Multi-stage build → small, non-root runtime image.
FROM rust:1.89-bookworm AS builder
WORKDIR /build
# Cache dependencies first.
COPY Cargo.toml Cargo.lock ./
RUN mkdir src && echo "fn main() {}" > src/main.rs && \
    echo "" > src/lib.rs && \
    cargo build --release --locked || true
# Real sources.
COPY src ./src
COPY examples ./examples
RUN touch src/main.rs src/lib.rs && cargo build --release --locked --bin landlink-relay

# Distroless runtime: glibc + libgcc, no shell, runs as nonroot (uid 65532).
FROM gcr.io/distroless/cc-debian12:nonroot
COPY --from=builder /build/target/release/landlink-relay /usr/local/bin/landlink-relay
# Data dir for the SQLite db + challenge secret (mount a volume here).
WORKDIR /data
ENV RELAY_BIND=0.0.0.0:8080 \
    RELAY_DB_PATH=/data/relay.db
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/landlink-relay"]
