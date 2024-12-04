FROM debian:trixie

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    clang-format \
    clang-tidy \
    clangd \
    build-essential \
    cmake \
    git \
    nano \
    python3 \
    pipx \
    ca-certificates \
    gcovr \
    python-is-python3 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd --create-home --shell /bin/bash builder
USER builder
WORKDIR /home/builder

# Install conan via pipx
RUN pipx install conan==2.25 && \
    pipx inject conan setuptools_scm && \
    pipx ensurepath
ENV PATH="/home/builder/.local/bin:${PATH}"

