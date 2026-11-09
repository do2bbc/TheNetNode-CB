FROM docker.io/library/debian:bookworm-slim
RUN dpkg --add-architecture i386 && apt-get update \
    && apt-get install -y --no-install-recommends \
    direwolf build-essential linux-libc-dev gcc-mingw-w64-i686 \
    g++-mingw-w64-i686 wine wine32:i386 xvfb xauth \
    && rm -rf /var/lib/apt/lists/*
COPY direwolf-dummy.conf /etc/direwolf.conf
CMD ["direwolf", "-t", "0", "-c", "/etc/direwolf.conf"]
