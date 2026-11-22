/** @file
 * @brief Test Suite prologue
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_TEST_NAME    "prologue"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"

#include "tapi_masque.h"
#include "tsapi_masque.h"

int
main(int argc, char **argv)
{
    tsapi_masque_session sess = {0};
    tapi_masque_backends be = TAPI_MASQUE_BACKENDS_INIT;

    TEST_START;

    TEST_STEP("Open a session and record which QUIC backends the agent has");
    CHECK_RC(tsapi_masque_session_init(&sess, "pco_prologue"));
    CHECK_RC(tapi_masque_available(sess.pco, &be));
    RING("MASQUE backends on %s: %s", sess.ta,
         be.versions != NULL ? be.versions : "(none)");

    if (!be.quiche && !be.msquic)
        RING("no QUIC backend compiled into the agent - data-plane tests "
             "will skip");

    free(be.versions);

    TEST_SUCCESS;

cleanup:
    tsapi_masque_session_fini(&sess);
    TEST_END;
}
