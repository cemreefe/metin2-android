# Metin2 server: db + auth + channel1 + ws->tcp bridge, one container.
# Builds the server from source inside the image, so the same Dockerfile
# produces amd64 on Intel and arm64 on Apple Silicon.
FROM debian:bookworm-slim AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends cmake ninja-build g++ ca-certificates \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /build
COPY src/ /build/src/
COPY srvcmake/ /build/srvcmake/
RUN cmake -S srvcmake -B b -G Ninja \
        -DM2_SERVER_SRC=/build/src -DCMAKE_BUILD_TYPE=Release \
 && cmake --build b --target db game qc

FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends sqlite3 python3 ca-certificates \
 && rm -rf /var/lib/apt/lists/*
COPY --from=build /build/b/bin/db /build/b/bin/game /build/b/bin/qc /app/bin/
COPY share/ /app/share/
COPY sqlite-seed/ /app/seed/
COPY run-m2.sh wsbridge.py gm.sh adduser.sh /app/
RUN chmod +x /app/run-m2.sh /app/gm.sh /app/adduser.sh && mkdir -p /data /srv
ENV M2_DATA=/data \
    M2_SQLITE_DIR=/data/sqlite \
    M2_ALLOWED_TARGETS=127.0.0.1:11000,127.0.0.1:11011
EXPOSE 8000
ENTRYPOINT ["/app/run-m2.sh"]
