# The image tools/helga.sh builds in on helga: emsdk at the version N64Recomp-browser's CI uses, ninja, ccache, clang
# with lld (N64Recomp and RSPRecomp natively, and the patches library for MIPS), naga 30.0.1 (WGSL from SPIR-V; dxc
# comes with rt64), cmake and python3 from the emsdk image, and SDL2 and GTK 3 for a native Linux build. helga.sh tags
# it with a hash of this file, so a change here makes the next run build a new one.
FROM emscripten/emsdk:6.0.12
RUN apt-get update && apt-get install -y --no-install-recommends ninja-build ccache clang lld git curl ca-certificates \
    libsdl2-dev libgtk-3-dev xz-utils unzip p7zip-full && rm -rf /var/lib/apt/lists/*
ENV CARGO_HOME=/opt/cargo RUSTUP_HOME=/opt/rustup
RUN curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal && /opt/cargo/bin/cargo install naga-cli --version 30.0.1 --locked \
    && cp /opt/cargo/bin/naga /usr/local/bin/ && rm -rf /opt/cargo/registry
