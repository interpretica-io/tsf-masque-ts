/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers for tsf-masque
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_LGR_USER "TSAPI MASQUE"

#include "te_config.h"

#include <string.h>

#include "logger_api.h"
#include "te_alloc.h"
#include "tapi_rpc_stdio.h"

#include "tsapi_masque.h"
#include "tapi_masque.h"

/* See description in tsapi_masque.h */
te_errno
tsapi_masque_session_init(tsapi_masque_session *session, const char *name)
{
    te_errno rc;

    memset(session, 0, sizeof(*session));
    session->ta = TSAPI_MASQUE_TA;

    rc = rcf_rpc_server_create(session->ta, name, &session->pco);
    if (rc != 0)
    {
        ERROR("Cannot create the RPC server '%s' on %s: %r", name,
              session->ta, rc);
        return rc;
    }

    return 0;
}

/* See description in tsapi_masque.h */
void
tsapi_masque_session_fini(tsapi_masque_session *session)
{
    if (session->pco != NULL)
    {
        rcf_rpc_server_destroy(session->pco);
        session->pco = NULL;
    }
}

/* Run a shell command on the agent, return its exit code (or -1). */
static int
run_on_agent(const char *ta, const char *cmd)
{
    rcf_rpc_server *pco = NULL;
    rpc_wait_status ws;
    te_errno rc;

    rc = rcf_rpc_server_create(ta, "pco_topo", &pco);
    if (rc != 0)
    {
        ERROR("cannot create RPC server for '%s': %r", cmd, rc);
        return -1;
    }

    RPC_AWAIT_IUT_ERROR(pco);
    ws = rpc_system(pco, cmd);
    rcf_rpc_server_destroy(pco);

    if (ws.flag != RPC_WAIT_STATUS_EXITED)
        return -1;
    return (int)ws.value;
}

/* See description in tsapi_masque.h */
te_errno
tsapi_masque_topo_up(const char *ta)
{
    int code = run_on_agent(ta, "sudo masque-topo up");

    if (code == 0)
        return 0;

    /* The helper exits 2 when the kernel has no network namespaces or
     * the proxy binary is missing - a skip, not a failure. */
    if (code == 2)
    {
        WARN("masque-topo reports the proxy environment is unavailable");
        return TE_RC(TE_TAPI, TE_EOPNOTSUPP);
    }

    ERROR("masque-topo up failed (exit %d)", code);
    return TE_RC(TE_TAPI, TE_EFAIL);
}

/* See description in tsapi_masque.h */
void
tsapi_masque_topo_down(const char *ta)
{
    (void)run_on_agent(ta, "sudo masque-topo down");
}

/* See description in tsapi_masque.h */
bool
tsapi_masque_backend_available(rcf_rpc_server *pco, const char *backend)
{
    tapi_masque_backends b = TAPI_MASQUE_BACKENDS_INIT;
    bool ok = false;

    if (tapi_masque_available(pco, &b) != 0)
        return false;

    if (strcmp(backend, "quiche") == 0)
        ok = b.quiche;
    else if (strcmp(backend, "msquic") == 0)
        ok = b.msquic;

    RING("masque backends: %s (asked for %s)",
         b.versions != NULL ? b.versions : "(none)", backend);
    free(b.versions);
    return ok;
}
