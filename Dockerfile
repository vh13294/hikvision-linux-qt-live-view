FROM debian:bookworm-slim

ARG UID=1000
ARG GID=1000

ENV DEBIAN_FRONTEND=noninteractive

# ── Build tools + Qt5 + all SDK runtime/compile-time dependencies ─────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Core build toolchain
    build-essential \
    g++ \
    make \
    pkg-config \
    git \
    ca-certificates \
    curl \
    wget \
    sudo \
    file \
    lsb-release \
    # Qt5 – modules used by QtClientDemo.pro: core gui opengl widgets
    qt5-qmake \
    qtbase5-dev \
    qttools5-dev \
    qttools5-dev-tools \
    libqt5opengl5-dev \
    # OpenGL / Mesa (required by libqt5opengl5-dev at link time)
    libgl-dev \
    libglu1-mesa-dev \
    libglx-dev \
    # SDL2 – used by libHCDisplay / libSuperRender for video surface rendering
    libsdl2-dev \
    # OpenAL – audio rendering (SDK ships libopenal.so.1 but dev headers needed)
    libopenal-dev \
    # OpenSSL headers – SDK ships its own libssl.so.1.1 / libcrypto.so.1.1
    # headers still needed for #include <openssl/...> in SDK headers
    libssl-dev \
    # zlib + iconv (SDK ships libz.so / libiconv2.so, but dev headers useful)
    zlib1g-dev \
    libc6-dev \
    # Virtual framebuffer – lets the compiled Qt binary run headless inside the container
    xvfb \
    x11-utils \
    && rm -rf /var/lib/apt/lists/*

# ── code-server (VS Code in browser) ──────────────────────────────────────────
RUN curl -fsSL https://code-server.dev/install.sh | sh \
    && rm -rf /tmp/code-server* /root/.cache \
    && CS_BIN=$(command -v code-server 2>/dev/null \
        || find /root/.local/bin /usr/lib/code-server/bin -name code-server -type f 2>/dev/null | head -1) \
    && ln -sf "$CS_BIN" /usr/local/bin/code-server \
    && /usr/local/bin/code-server --version

# ── Non-root developer user ────────────────────────────────────────────────────
RUN groupadd -g ${GID} developer \
    && useradd -m -u ${UID} -g ${GID} -s /bin/bash developer \
    && echo "developer ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER developer
WORKDIR /home/developer

EXPOSE 8080

# Start code-server pointing at the mounted SDK workspace
CMD ["code-server", "--bind-addr", "0.0.0.0:8080", "--auth", "none", "/workspace/sdk"]
