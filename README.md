# tsf-masque-ts

A Test Environment (TE) suite for
[tsf-masque](https://github.com/interpretica-io/tsf-masque). It exercises
MASQUE proxying from a Test Agent against a real MASQUE proxy, over both
QUIC backends (quiche and msquic), on the data plane and, with TAD, on the
wire.

## What it builds and runs

The suite's Docker build image carries everything the agent needs, built
from source (see `scripts/docker/build/Dockerfile`):

- Cloudflare quiche: `libquiche.so` and `quiche.h` (the C API).
- Microsoft msquic: `libmsquic.so` and headers.
- masque-proxy: a small CONNECT-UDP proxy built on quic-go's `masque-go`
  (`scripts/docker/build/masque-proxy/`).
- udp-echo and tcp-echo: trivial echo targets.
- masque-topo: brings the proxy up in a network namespace joined to the
  root namespace by a veth pair, with the echo targets in the root
  namespace.

tsf-masque itself is pulled in as a TE external repository
(`conf/external.yml`, pinned in `conf/builder.conf.lock`); its `ta_masque`
and `rpcs_masque` link into the agent's RPC server and its `tapi_masque`
into the engine.

## Groups

- connect: `echo` brings a CONNECT-UDP tunnel up over each backend and
  echoes a payload through it; `negative` covers a dead proxy and a closed
  TCP target (must not establish) and the h3-only control (must).
  connect-tcp skips cleanly because this proxy serves CONNECT-UDP only.
- onwire: `datagram` uses two TAD `udp.ip4.eth` CSAPs on the root-side
  veth end to prove, on the wire, that the agent's QUIC packets reach the
  proxy and that the exact probe sent through the tunnel emerges as a
  plaintext UDP datagram to the target. Each test emits an MI line-graph.

## Why a network namespace

Plain localhost rides `lo`, which has no L2 framing, so a TAD eth/ip4/udp
CSAP sees nothing. Putting the proxy in a namespace and the client and
targets in the root namespace forces both the QUIC flow and the proxy's
decrypted egress across the `mqv0` veth end, where TAD can capture them.

## Run

```bash
git clone https://github.com/interpretica-io/test-environment ../test-environment
./scripts/run.sh docker guess --cfg=localhost
```

The first run builds the image (quiche and msquic from source, several
minutes). Read results with `cat log.txt`, or `./scripts/html-log.sh` for
the HTML report with the MI charts.

## Status

Green on Docker Desktop (macOS arm64, amd64 emulated): 12 passed as
expected, 0 failed, 0 aborted, 2 skipped as expected. Both backends do
CONNECT-UDP end to end (quiche/udp and msquic/udp pass). connect-tcp skips
as expected because masque-go serves CONNECT-UDP only. The onwire TAD test
captures real packets.

Two things bite anyone building from scratch:

- The container needs `--cap-add=NET_ADMIN,SYS_ADMIN,NET_RAW` and
  `--user=0:0`; netns setup needs SYS_ADMIN, and a non-root user cannot use
  added caps without ambient caps.
- msquic pins to tag `v2.5.13` and CMake `-DQUIC_TLS_LIB=quictls` (the TLS
  flag name and provider value drift between msquic releases).

After committing to tsf-masque, advance the `tsf_masque` commit hash in
`conf/builder.conf.lock` (or run with `--update-external`); the Builder
reuses the pinned commit and will not pick up new commits otherwise.
