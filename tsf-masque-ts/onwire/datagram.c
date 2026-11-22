/** @file
 * @brief onwire/datagram
 *
 * Prove, on the wire with TAD, that a CONNECT-UDP tunnel actually carries
 * a payload end to end:
 *
 *  - the agent's QUIC packets to the proxy are visible (UDP to the proxy
 *    port), so the tunnel really used QUIC;
 *  - the exact bytes sent through the tunnel emerge from the proxy as a
 *    plaintext UDP datagram to the target (UDP to the echo port with the
 *    probe as payload), so the proxy decrypted and forwarded them.
 *
 * Both flows cross the root-side veth end (@ref TSAPI_MASQUE_IFACE), where
 * two udp.ip4.eth CSAPs capture them. On a kernel without namespaces the
 * group prologue has already skipped; here the backend is fixed to quiche
 * for determinism.
 *
 * The captures are armed BEFORE the tunnel is opened, so the whole QUIC
 * handshake (many packets to the proxy port) is on the wire inside the
 * capture window - not just the lone datagram a late-armed CSAP might
 * miss. The probe is sent several times for the same reason.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "onwire/datagram"

#include "te_config.h"

#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tapi_test.h"
#include "te_mi_log.h"
#include "rcf_api.h"
#include "asn_usr.h"
#include "tad_common.h"
#include "tapi_tad.h"
#include "tapi_udp.h"
#include "tapi_ip4.h"
#include "tapi_eth.h"

#include "tapi_masque.h"
#include "tsapi_masque.h"

#define SESSION "w1"
/* A distinctive payload so the egress match cannot be a coincidence. */
#define PROBE   "TSF-MASQUE-ONWIRE-7b1c9a42-PROBE"
/* How many times the probe is pushed through the tunnel. */
#define PROBES  5

/* Build a receive pattern matching UDP to @p dst_port, optionally with an
 * exact payload. Returns a newly allocated asn_value (caller frees). */
static asn_value *
udp_pattern(int dst_port, const char *payload, size_t payload_len)
{
    asn_value *pat = NULL;
    te_errno rc;

    /*
     * tapi_udp_add_pdu() takes ports in network byte order (it applies
     * ntohs() before storing them in the pattern), so the host-order
     * constant has to be converted - otherwise the pattern looks for a
     * byte-swapped port and matches nothing.
     */
    rc = tapi_udp_add_pdu(&pat, NULL, true, -1, htons(dst_port));
    if (rc == 0)
        rc = tapi_ip4_add_pdu(&pat, NULL, true, 0, 0, -1, -1, -1);
    if (rc == 0)
        rc = tapi_eth_add_pdu(&pat, NULL, true, NULL, NULL, NULL,
                              TE_BOOL3_ANY, TE_BOOL3_ANY);
    if (rc == 0 && payload != NULL)
        rc = asn_write_value_field(pat, payload, payload_len,
                                   "0.payload.#bytes");
    if (rc != 0)
    {
        ERROR("building the UDP pattern failed: %r", rc);
        asn_free_value(pat);
        return NULL;
    }
    return pat;
}

int
main(int argc, char **argv)
{
    tsapi_masque_session sess = {0};
    te_mi_logger *logger = NULL;
    tapi_masque_data echo = TAPI_MASQUE_DATA_INIT;
    csap_handle_t csap_quic = CSAP_INVALID_HANDLE;
    csap_handle_t csap_egress = CSAP_INVALID_HANDLE;
    asn_value *pat_quic = NULL;
    asn_value *pat_egress = NULL;
    unsigned int quic_pkts = 0;
    unsigned int egress_pkts = 0;
    unsigned int echoes = 0;
    int i;
    int sid;

    TEST_START;

    TEST_STEP("Open a session; require the quiche backend");
    CHECK_RC(tsapi_masque_session_init(&sess, "pco_onwire"));
    if (!tsapi_masque_backend_available(sess.pco, "quiche"))
        TEST_SKIP("the agent has no quiche MASQUE backend");

    TEST_STEP("Create two capturing CSAPs on %s", TSAPI_MASQUE_IFACE);
    CHECK_RC(rcf_ta_create_session(sess.ta, &sid));
    CHECK_RC(tapi_udp_ip4_eth_csap_create(sess.ta, sid, TSAPI_MASQUE_IFACE,
                                          TAD_ETH_RECV_ALL, NULL, NULL,
                                          0, 0, -1, -1, &csap_quic));
    CHECK_RC(tapi_udp_ip4_eth_csap_create(sess.ta, sid, TSAPI_MASQUE_IFACE,
                                          TAD_ETH_RECV_ALL, NULL, NULL,
                                          0, 0, -1, -1, &csap_egress));

    pat_quic = udp_pattern(TSAPI_MASQUE_PROXY_PORT, NULL, 0);
    pat_egress = udp_pattern(TSAPI_MASQUE_UDP_ECHO_PORT, PROBE, strlen(PROBE));
    if (pat_quic == NULL || pat_egress == NULL)
        TEST_FAIL("could not build the capture patterns");

    TEST_STEP("Arm both captures before opening the tunnel");
    /*
     * Arm first, open second. The QUIC handshake is dozens of packets to
     * the proxy port; catching it makes the QUIC-on-the-wire assertion
     * robust instead of depending on the single datagram below racing a
     * CSAP that may not have finished arming yet.
     */
    CHECK_RC(tapi_tad_trrecv_start(sess.ta, sid, csap_quic, pat_quic,
                                   15000, 0, RCF_TRRECV_COUNT));
    CHECK_RC(tapi_tad_trrecv_start(sess.ta, sid, csap_egress, pat_egress,
                                   15000, 0, RCF_TRRECV_COUNT));

    TEST_STEP("Bring a CONNECT-UDP tunnel up through the proxy");
    {
        char *err = NULL;
        te_errno crc = tapi_masque_connect(sess.pco, SESSION,
                           TAPI_MASQUE_KIND_UDP, "quiche",
                           TSAPI_MASQUE_UDP_URI,
                           TSAPI_MASQUE_ROOT_IP ":7777", false, NULL, &err);

        if (crc != 0)
        {
            char *msg = err;

            err = NULL;
            TEST_VERDICT("the CONNECT-UDP tunnel did not establish: %s",
                         msg != NULL ? msg : "(no detail)");
        }
        free(err);
    }

    TEST_STEP("Push the probe through the tunnel %d times", PROBES);
    for (i = 0; i < PROBES; i++)
    {
        tapi_masque_data_free(&echo);
        CHECK_RC(tapi_masque_udp_send(sess.pco, SESSION,
                                      (const uint8_t *)PROBE, strlen(PROBE)));
        if (tapi_masque_udp_recv(sess.pco, SESSION, 3000, &echo) == 0 &&
            echo.len == strlen(PROBE) &&
            memcmp(echo.data, PROBE, echo.len) == 0)
        {
            echoes++;
        }
    }
    if (echoes == 0)
        TEST_VERDICT("not one probe echoed back through the tunnel");

    TEST_STEP("Stop the captures and read the counts");
    CHECK_RC(tapi_tad_trrecv_stop(sess.ta, sid, csap_quic, NULL, &quic_pkts));
    CHECK_RC(tapi_tad_trrecv_stop(sess.ta, sid, csap_egress, NULL,
                                  &egress_pkts));
    RING("on the wire: %u QUIC packet(s) to the proxy, %u egress "
         "datagram(s) carrying the probe (%u/%d echoes round-tripped)",
         quic_pkts, egress_pkts, echoes, PROBES);

    TEST_STEP("The wire must corroborate the tunnel");
    /*
     * The data-plane round trip succeeded, so the packets certainly
     * crossed mqv0. If TAD saw none of them the fault is in the capture,
     * not the tunnel - and that is a real defect to fix, not to skip.
     */
    if (quic_pkts == 0)
        TEST_VERDICT("no QUIC packets to the proxy were captured - the "
                     "tunnel did not use QUIC on the wire, or the capture "
                     "missed it");
    if (egress_pkts == 0)
        TEST_VERDICT("the probe never emerged as a plaintext UDP datagram "
                     "to the target - the proxy did not forward it, or the "
                     "capture missed it");

    TEST_STEP("Chart the on-wire counts");
    CHECK_RC(te_mi_logger_meas_create("tsf-masque", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "quic_pkts", TE_MI_MEAS_AGGR_SINGLE, quic_pkts,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "egress_pkts", TE_MI_MEAS_AGGR_SINGLE, egress_pkts,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "onwire", "CONNECT-UDP packets on the wire");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                                          TE_MI_MEAS_VIEW_LINE_GRAPH,
                                          "onwire", TE_MI_GRAPH_AXIS_X,
                                          TE_MI_GRAPH_AUTO_SEQNO);
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tapi_masque_data_free(&echo);
    if (csap_quic != CSAP_INVALID_HANDLE)
        tapi_tad_csap_destroy(sess.ta, sid, csap_quic);
    if (csap_egress != CSAP_INVALID_HANDLE)
        tapi_tad_csap_destroy(sess.ta, sid, csap_egress);
    asn_free_value(pat_quic);
    asn_free_value(pat_egress);
    if (sess.pco != NULL)
        tapi_masque_close(sess.pco, SESSION);
    tsapi_masque_session_fini(&sess);
    TEST_END;
}
