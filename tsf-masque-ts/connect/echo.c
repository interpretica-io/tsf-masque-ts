/** @file
 * @brief connect/echo
 *
 * Open a tunnel through the proxy and move a payload through it to an echo
 * target: a datagram for CONNECT-UDP, a byte stream for connect-tcp.
 * Parameterised by the QUIC backend and the kind, so the same test
 * exercises quiche and msquic, UDP and TCP.
 *
 * Skips when the backend is not compiled in, or (for tcp) when the proxy
 * offers no connect-tcp (it answers a non-2xx status).
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "connect/echo"

#include "te_config.h"

#include <string.h>
#include <time.h>

#include "tapi_test.h"
#include "te_mi_log.h"

#include "tapi_masque.h"
#include "tsapi_masque.h"

#define PROBE   "tsf-masque echo probe 0123456789"
#define SESSION "s1"

int
main(int argc, char **argv)
{
    tsapi_masque_session sess = {0};
    te_mi_logger *logger = NULL;
    const char *backend;
    const char *kind;
    bool is_udp = true;
    te_bool used_h3 = false;
    char *error = NULL;
    const char *proxy_uri;
    const char *target;
    size_t echoed = 0;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);
    TEST_GET_STRING_PARAM(kind);
    is_udp = (strcmp(kind, "udp") == 0);

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_masque_session_init(&sess, "pco_echo"));

    TEST_STEP("Skip unless the '%s' backend is available", backend);
    if (!tsapi_masque_backend_available(sess.pco, backend))
        TEST_SKIP("the agent has no '%s' MASQUE backend", backend);

    proxy_uri = is_udp ? TSAPI_MASQUE_UDP_URI : TSAPI_MASQUE_TCP_URI;
    target = is_udp ? TSAPI_MASQUE_ROOT_IP ":7777"
                    : TSAPI_MASQUE_ROOT_IP ":7778";

    TEST_STEP("Connect the %s tunnel through the proxy", kind);
    rc = tapi_masque_connect(sess.pco, SESSION,
                             is_udp ? TAPI_MASQUE_KIND_UDP
                                    : TAPI_MASQUE_KIND_TCP,
                             backend, proxy_uri, target, false, &used_h3,
                             &error);
    if (rc != 0)
    {
        if (!is_udp && error != NULL && strstr(error, ":status") != NULL)
        {
            RING("proxy refused connect-tcp: %s", error);
            TEST_SKIP("the proxy does not offer connect-tcp");
        }
        TEST_VERDICT("the %s tunnel did not establish: %s", kind,
                     error != NULL ? error : "(no detail)");
    }
    if (!used_h3)
        TEST_VERDICT("the tunnel came up but not over HTTP/3");

    TEST_STEP("Send the probe through the tunnel and read the echo back");
    if (is_udp)
    {
        tapi_masque_data r = TAPI_MASQUE_DATA_INIT;

        CHECK_RC(tapi_masque_udp_send(sess.pco, SESSION,
                                      (const uint8_t *)PROBE, strlen(PROBE)));
        CHECK_RC(tapi_masque_udp_recv(sess.pco, SESSION, 3000, &r));
        if (r.len != strlen(PROBE) || memcmp(r.data, PROBE, r.len) != 0)
        {
            tapi_masque_data_free(&r);
            TEST_VERDICT("the echoed datagram did not match the probe");
        }
        echoed = r.len;
        tapi_masque_data_free(&r);
    }
    else
    {
        size_t sent = 0;
        time_t deadline = time(NULL) + 5;

        CHECK_RC(tapi_masque_tcp_send(sess.pco, SESSION,
                                      (const uint8_t *)PROBE, strlen(PROBE),
                                      &sent));
        if (sent != strlen(PROBE))
            TEST_VERDICT("short write into the TCP tunnel: %zu/%zu", sent,
                         strlen(PROBE));
        while (echoed < strlen(PROBE) && time(NULL) < deadline)
        {
            tapi_masque_data r = TAPI_MASQUE_DATA_INIT;

            rc = tapi_masque_tcp_recv(sess.pco, SESSION, 2000, &r);
            if (rc != 0)
            {
                if (TE_RC_GET_ERROR(rc) == TE_ETIMEDOUT)
                    continue;
                TEST_VERDICT("reading the TCP echo failed: %r", rc);
            }
            echoed += r.len;
            tapi_masque_data_free(&r);
        }
        if (echoed != strlen(PROBE))
            TEST_VERDICT("the TCP echo returned %zu of %zu bytes", echoed,
                         strlen(PROBE));
    }

    TEST_STEP("Chart the outcome");
    CHECK_RC(te_mi_logger_meas_create("tsf-masque", &logger));
    te_mi_logger_add_meas_key(logger, NULL, "backend", "%s", backend);
    te_mi_logger_add_meas_key(logger, NULL, "kind", "%s", kind);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE, "used_h3",
                          TE_MI_MEAS_AGGR_SINGLE, used_h3 ? 1 : 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_MEMORY, "echoed_bytes",
                          TE_MI_MEAS_AGGR_SINGLE, echoed,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "echo", "MASQUE echo outcome");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                                          TE_MI_MEAS_VIEW_LINE_GRAPH, "echo",
                                          TE_MI_GRAPH_AXIS_X,
                                          TE_MI_GRAPH_AUTO_SEQNO);
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    free(error);
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (sess.pco != NULL)
        tapi_masque_close(sess.pco, SESSION);
    tsapi_masque_session_fini(&sess);
    TEST_END;
}
