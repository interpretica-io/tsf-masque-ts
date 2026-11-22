/** @file
 * @brief connect group prologue
 *
 * Bring the MASQUE proxy topology up on the agent. If the kernel has no
 * network namespaces or the proxy is missing, skip the whole group.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "connect/prologue"

#include "te_config.h"
#include "tapi_test.h"

#include "tsapi_masque.h"

int
main(int argc, char **argv)
{

    TEST_START;

    TEST_STEP("Bring the MASQUE proxy topology up");
    rc = tsapi_masque_topo_up(TSAPI_MASQUE_TA);
    if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
        TEST_SKIP("the agent cannot host the MASQUE proxy (no netns/proxy)");
    if (rc != 0)
        TEST_VERDICT("failed to bring the proxy topology up: %r", rc);

    TEST_SUCCESS;

cleanup:
    TEST_END;
}
