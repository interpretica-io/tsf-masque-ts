/** @file
 * @brief connect group epilogue
 *
 * Tear the MASQUE proxy topology down.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "connect/epilogue"

#include "te_config.h"
#include "tapi_test.h"

#include "tsapi_masque.h"

int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("Tear the MASQUE proxy topology down");
    tsapi_masque_topo_down(TSAPI_MASQUE_TA);
    TEST_SUCCESS;

cleanup:
    TEST_END;
}
