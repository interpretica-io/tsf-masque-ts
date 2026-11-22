# Build environment
FROM debian:bookworm

ARG user
ARG uid
ARG gid
ARG docker_gid
ENV USER=${user}
ENV UID=${uid}
ENV GID=${gid}
ENV DOCKER_GID=${docker_gid}

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${DOCKER_GID} docker ; fi

RUN if [ ! -d /home/${USER} ] ; then mkdir -p /home/${USER} && chmod -R 777 /home/${USER} ; fi

# Install basic dependencies & useful tools. libcurl4-openssl-dev gives
# the libcurl (curl/curl.h, -lcurl) that ta_quic links; note the stock
# Debian/Ubuntu libcurl is usually built WITHOUT an HTTP/3 backend, so
# tapi_quic_available() returns false and the suite skips cleanly. A
# libcurl with HTTP/3 (ngtcp2/quiche) is needed to actually exercise h3.
RUN apt-get update && \
    apt-get install -y \
        autoconf \
        automake \
        bison \
        build-essential \
        ccache \
        cmake \
        curl \
        docker.io \
        file \
        flex \
        gawk \
        libbsd-dev \
        libjansson-dev \
        libexpat1 \
        libexpat1-dev \
        libcurl4-openssl-dev \
        libglib2.0-dev \
        libnetconf2-dev \
        libncurses5 \
        libncurses5-dev \
        libpam0g \
        libpam0g-dev \
        libpcap0.8-dev \
        libpopt-dev \
        libreadline-dev \
        libssh-dev \
        libssh2-1-dev \
        libssl-dev \
        libusb-1.0-0 \
        libusb-1.0-0-dev \
        libxml2 \
        libxml2-dev \
        libxslt1-dev \
        libxslt1.1 \
        libpcre3-dev \
        libyaml-dev \
        m4 \
        meson \
        openssh-server \
        pkg-config \
        perl \
        python3 \
        rsync \
        ssh \
        sudo \
        tshark \
        tcl-dev \
        unzip \
        wget

# --- MASQUE suite extras: QUIC backends, the proxy, the targets --------
# Network namespaces + veth carry the TAD topology; the rest are build
# deps for quiche (Rust), msquic (CMake + OpenSSL/quictls) and the
# masque-go proxy (Go).
RUN apt-get update && apt-get install -y \
        iproute2 \
        iptables \
        libnuma-dev \
        liblttng-ust-dev \
        nasm

ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    GOPATH=/tmp/go \
    PATH=/usr/local/cargo/bin:/usr/local/go/bin:/usr/local/bin:$PATH

# A recent Go (bookworm's is too old for current quic-go / masque-go).
RUN curl -sSL https://go.dev/dl/go1.22.5.linux-amd64.tar.gz \
        | tar -C /usr/local -xz

# Rust toolchain for Cloudflare quiche.
RUN curl -sSf https://sh.rustup.rs \
        | sh -s -- -y --default-toolchain stable --profile minimal

# Cloudflare quiche: build the C API, install libquiche.so + quiche.h.
RUN git clone --recursive --depth 1 --branch 0.22.0 \
        https://github.com/cloudflare/quiche /tmp/quiche && \
    cd /tmp/quiche && \
    cargo build --release --features ffi,pkg-config-meta && \
    install -D -m0644 target/release/libquiche.so \
            /usr/local/lib/libquiche.so && \
    install -D -m0644 quiche/include/quiche.h \
            /usr/local/include/quiche.h && \
    (install -D -m0644 target/release/quiche.pc \
            /usr/local/lib/pkgconfig/quiche.pc 2>/dev/null || true) && \
    ldconfig && rm -rf /tmp/quiche /usr/local/cargo/registry

# Microsoft msquic: build the shared library and install it. msquic 2.5
# selects the TLS provider with QUIC_TLS_LIB (openssl = the quictls fork);
# passing the older QUIC_TLS at all is a hard error there.
RUN git clone --recursive --depth 1 --branch v2.5.13 \
        https://github.com/microsoft/msquic /tmp/msquic && \
    cd /tmp/msquic && mkdir build && cd build && \
    cmake -G 'Unix Makefiles' -DQUIC_BUILD_SHARED=on \
          -DQUIC_ENABLE_LOGGING=off -DQUIC_TLS_LIB=quictls \
          -DCMAKE_INSTALL_PREFIX=/usr/local .. && \
    cmake --build . --parallel && cmake --install . && \
    ldconfig && rm -rf /tmp/msquic

# The masque-go CONNECT-UDP proxy.
COPY masque-proxy /tmp/masque-proxy
RUN cd /tmp/masque-proxy && go mod tidy && \
    go build -o /usr/local/bin/masque-proxy . && \
    rm -rf /tmp/masque-proxy /tmp/go

# The echo targets and the topology helper.
COPY udp-echo.c tcp-echo.c /tmp/echo/
RUN gcc -O2 -o /usr/local/bin/udp-echo /tmp/echo/udp-echo.c && \
    gcc -O2 -o /usr/local/bin/tcp-echo /tmp/echo/tcp-echo.c && \
    rm -rf /tmp/echo
COPY masque-topo /usr/local/bin/masque-topo
RUN chmod +x /usr/local/bin/masque-topo

# Clean pkg-config files for the agent app link (build.sh --extra-deps).
# quiche's own .pc from pkg-config-meta points at the deleted build dir,
# and msquic ships none; both are rewritten to /usr/local here.
RUN mkdir -p /usr/local/lib/pkgconfig && \
    printf 'prefix=/usr/local\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\n\nName: quiche\nDescription: quiche\nVersion: 0.22.0\nLibs: -L${libdir} -lquiche\nCflags: -I${includedir}\n' \
        > /usr/local/lib/pkgconfig/quiche.pc && \
    printf 'prefix=/usr/local\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\n\nName: libmsquic\nDescription: msquic\nVersion: 2.5.13\nLibs: -L${libdir} -lmsquic\nCflags: -I${includedir}\n' \
        > /usr/local/lib/pkgconfig/libmsquic.pc

# Make /usr/local/lib visible to the runtime loader (quiche/msquic).
RUN echo /usr/local/lib > /etc/ld.so.conf.d/masque.conf && ldconfig

RUN echo "${USER} ALL=NOPASSWD: ALL" >> /etc/sudoers

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${GID} ${USER} ; fi

RUN useradd --system \
            --gid ${GID} --gid ${DOCKER_GID} \
            --uid ${UID} \
            --shell /bin/bash \
            ${USER}
RUN usermod -a -G ${DOCKER_GID} ${USER}
RUN usermod -a -G docker ${USER}
