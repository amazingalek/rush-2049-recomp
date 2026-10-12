# The image tools/helga.sh builds in on helga: emsdk at the version N64Recomp-browser's CI uses, ninja, ccache, clang
# with lld (N64Recomp and RSPRecomp natively, and the patches library for MIPS), naga 30.0.1 (WGSL from SPIR-V; dxc
# comes with rt64), cmake and python3 from the emsdk image, SDL2 and GTK 3 for a native Linux build, what the game
# forks need, and spirv-opt (below). helga.sh tags it with a hash of this file, so a change here makes the next run
# build a new one.
FROM emscripten/emsdk:6.0.12
RUN apt-get update && apt-get install -y --no-install-recommends ninja-build ccache clang lld git curl ca-certificates \
    libsdl2-dev libgtk-3-dev xz-utils unzip p7zip-full && rm -rf /var/lib/apt/lists/*
ENV CARGO_HOME=/opt/cargo RUSTUP_HOME=/opt/rustup
RUN curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal && /opt/cargo/bin/cargo install naga-cli --version 30.0.1 --locked \
    && cp /opt/cargo/bin/naga /usr/local/bin/ && rm -rf /opt/cargo/registry
# What the game forks' builds need besides: xdelta3 (GoldenEye's TLBFREE ROM), make (patches libraries), and what
# Conker's build.sh checks for before it recompiles. A layer of its own, so the ones above stay cached.
RUN apt-get update && apt-get install -y --no-install-recommends xdelta3 make pkg-config libdbus-1-dev libfreetype-dev \
    python3-venv && rm -rf /var/lib/apt/lists/*
# spirv-opt (SPIRV-Tools), which rt64 runs on a game's prebuilt shaders (RT64_PREBUILT_SHADER_LIST) before naga: their
# WGSL comes out about 45% smaller, and builds in about half the time in Chrome.
RUN apt-get update && apt-get install -y --no-install-recommends spirv-tools && rm -rf /var/lib/apt/lists/*
