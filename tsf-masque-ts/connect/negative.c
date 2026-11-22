/** @file
 * @brief connect/negative
 *
 *  - @c bad_proxy  - a proxy address with nothing listening: connect MUST
 *                    fail and report an error.
 *  - @c tcp_closed - connect-tcp to a closed target port: the proxy
 *                    answers non-2xx, so connect MUST fail.
 *  - @c h3_only    - demand HTTP/3 against the HTTP/3 proxy: connect MUST
 *                    succeed and report used_h3 (the positive control).
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "connect/negative"

#include "te_config.h"

#include <string.h>

#include "tapi_test.h"
#include "te_mi_log.h"

#include "tapi_masque.h"
#include "tsapi_masque.h"

#define SESSION "n1"

int
main(int argc, char **argv)
{
    tsapi_masque_session sess = {0};
    te_mi_logger *logger = NULL;
    const char *backend;
    const char *scenario;
    int kind = TAPI_MASQUE_KIND_UDP;
    const char *proxy_uri;
    const char *target;
    te_bool h3_only = false;
    te_bool used_h3 = false;
    char *error = NULL;
    bool established;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);
    TEST_GET_STRING_PARAM(scenario);

    TEST_STEP("Open a session to the agent");
    CHECK_RC(tsapi_masque_session_init(&sess, "pco_negative"));
    if (!tsapi_masque_backend_available(sess.pco, backend))
        TEST_SKIP("the agent has no '%s' MASQUE backend", backend);

    if (strcmp(scenario, "bad_proxy") == 0)
    {
        proxy_uri = "https://" TSAPI_MASQUE_PROXY_IP ":4444"
                    "/.well-known/masque/udp/" TSAPI_MASQUE_ROOT_IP "/7777/";
        target = TSAPI_MASQUE_ROOT_IP ":7777";
    }
    else if (strcmp(scenario, "tcp_closed") == 0)
    {
        kind = TAPI_MASQUE_KIND_TCP;
        proxy_uri = "https://" TSAPI_MASQUE_PROXY_IP ":4433"
                    "/.well-known/masque/tcp/" TSAPI_MASQUE_ROOT_IP "/9999/";
        target = TSAPI_MASQUE_ROOT_IP ":9999";
    }
    else if (strcmp(scenario, "h3_only") == 0)
    {
        proxy_uri = TSAPI_MASQUE_UDP_URI;
        target = TSAPI_MASQUE_ROOT_IP ":7777";
        h3_only = true;
    }
    else
    {
        TEST_FAIL("unknown scenario '%s'", scenario);
    }

    TEST_STEP("Connect the '%s' scenario and read the outcome", scenario);
    rc = tapi_masque_connect(sess.pco, SESSION, kind, backend, proxy_uri,
                             target, h3_only, &used_h3, &error);
    established = (rc == 0);
    RING("scenario '%s': established=%d used_h3=%d error='%s'", scenario,
         established, used_h3, error != NULL ? error : "");

    TEST_STEP("Check the scenario's expectation");
    if (strcmp(scenario, "h3_only") == 0)
    {
        if (!established || !used_h3)
            TEST_VERDICT("h3-only tunnel to the HTTP/3 proxy did not come "
                         "up over HTTP/3: %s",
                         error != NULL ? error : "(no detail)");
    }
    else
    {
        if (established)
            TEST_VERDICT("the '%s' scenario established a tunnel it should "
                         "not have", scenario);
        if (error == NULL || error[0] == '\0')
            TEST_VERDICT("the '%s' scenario failed without an error",
                         scenario);
    }

    TEST_STEP("Chart the outcome");
    CHECK_RC(te_mi_logger_meas_create("tsf-masque", &logger));
    te_mi_logger_add_meas_key(logger, NULL, "scenario", "%s", scenario);
    te_mi_logger_add_meas_key(logger, NULL, "backend", "%s", backend);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "established", TE_MI_MEAS_AGGR_SINGLE,
                          established ? 1 : 0, TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "neg", "MASQUE negative-case outcome");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                                          TE_MI_MEAS_VIEW_LINE_GRAPH, "neg",
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
