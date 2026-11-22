/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers for tsf-masque
 *
 * Every test needs the same two things:
 *
 *  - an RPC server on the agent, because the data plane (tapi_masque)
 *    drives the agent's QUIC backend over RPCs;
 *  - a reachable MASQUE proxy with a target behind it.
 *
 * The proxy is brought up by the @c masque-topo helper shipped in the
 * build image: it puts the proxy in a network namespace joined to the
 * root namespace by a veth pair, with a UDP and a TCP echo target in the
 * root namespace. The point of the namespace is the wire: QUIC packets
 * (agent to proxy) and the proxy's decrypted egress (proxy to target)
 * both cross the root-side veth end @ref TSAPI_MASQUE_IFACE, where TAD
 * can see them - which plain localhost, riding @c lo with no L2 framing,
 * does not allow.
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#ifndef __TSAPI_MASQUE_H__
#define __TSAPI_MASQUE_H__

#include "rcf_rpc.h"
#include "te_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The agent this suite works on. */
#define TSAPI_MASQUE_TA         "Agt_A"

/* The topology the masque-topo helper builds (must match the script). */

/** Root-side veth end: where TAD captures. */
#define TSAPI_MASQUE_IFACE      "mqv0"
/** Root-side veth IP; the echo targets live here. */
#define TSAPI_MASQUE_ROOT_IP    "10.78.0.1"
/** Namespace-side veth IP; the proxy listens here. */
#define TSAPI_MASQUE_PROXY_IP   "10.78.0.2"
/** The proxy's UDP/QUIC port. */
#define TSAPI_MASQUE_PROXY_PORT 4433
/** UDP echo target port (root namespace). */
#define TSAPI_MASQUE_UDP_ECHO_PORT 7777
/** TCP echo target port (root namespace). */
#define TSAPI_MASQUE_TCP_ECHO_PORT 7778

/** The expanded CONNECT-UDP proxy URI template for the UDP echo target. */
#define TSAPI_MASQUE_UDP_URI \
    "https://" TSAPI_MASQUE_PROXY_IP ":4433/.well-known/masque/udp/" \
    TSAPI_MASQUE_ROOT_IP "/7777/"

/** The connect-tcp proxy URI template for the TCP echo target. */
#define TSAPI_MASQUE_TCP_URI \
    "https://" TSAPI_MASQUE_PROXY_IP ":4433/.well-known/masque/tcp/" \
    TSAPI_MASQUE_ROOT_IP "/7778/"

/** What a test needs to talk to the agent. */
typedef struct tsapi_masque_session {
    const char     *ta;     /**< Agent name. */
    rcf_rpc_server *pco;    /**< RPC server on it - what tapi_masque takes. */
} tsapi_masque_session;

/**
 * Open a session: an RPC server on the agent.
 *
 * @param[out] session  Session.
 * @param[in]  name     RPC server name, unique within the test.
 *
 * @return Status code.
 */
extern te_errno tsapi_masque_session_init(tsapi_masque_session *session,
                                          const char *name);

/** Close a session. */
extern void tsapi_masque_session_fini(tsapi_masque_session *session);

/**
 * Bring the proxy topology up on the agent (@c masque-topo up): the
 * namespace, the veth pair, the proxy and the echo targets. Idempotent
 * enough to call from a group prologue; tear it down with
 * tsapi_masque_topo_down() from the epilogue.
 *
 * @param ta    Agent name.
 *
 * @return Status code; TE_ESKIP-worthy failures (no namespaces, no
 *         proxy binary) are returned so the caller can TEST_SKIP.
 */
extern te_errno tsapi_masque_topo_up(const char *ta);

/** Tear the proxy topology down (@c masque-topo down). Never fails hard. */
extern void tsapi_masque_topo_down(const char *ta);

/**
 * Is a given backend usable on the agent? Reads masque_available().
 *
 * @param pco           RPC server.
 * @param backend       "quiche" or "msquic".
 *
 * @return @c true when that backend is compiled in.
 */
extern bool tsapi_masque_backend_available(rcf_rpc_server *pco,
                                           const char *backend);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSAPI_MASQUE_H__ */
