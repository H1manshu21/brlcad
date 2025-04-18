/*                         C O P Y M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/copymat.c
 *
 * The copymat command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_copymat_core(struct ged *gedp, int argc, const char *argv[])
{
    char *child = NULL;
    char *parent = NULL;
    struct bu_vls pvls = BU_VLS_INIT_ZERO;
    int i;
    int sep;
    int status;
    struct db_tree_state ts;
    struct directory *dp;
    struct rt_comb_internal *comb;
    struct rt_db_internal intern;
    struct animate anp;
    union tree *tp;
    static const char *usage = "a/b c/d";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /*
     * Ensure that each argument contains exactly one slash
     */
    for (i = 1; i <= 2; ++i) {
	if (((child = strchr(argv[i], '/')) == NULL)
	    || (strchr(++child, '/') != NULL))
	{
	    bu_vls_printf(gedp->ged_result_str, "%s: bad arc: '%s'\n", argv[0], argv[i]);
	    return BRLCAD_ERROR;
	}
    }

    memset(&anp, 0, sizeof(struct animate));
    anp.magic = ANIMATE_MAGIC;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    ts = wdbp->wdb_initial_tree_state;	/* struct copy */

    ts.ts_dbip = gedp->dbip;
    ts.ts_resp = &rt_uniresource;
    MAT_IDN(ts.ts_mat);
    db_full_path_init(&anp.an_path);
    if (child == NULL
	|| db_follow_path_for_state(&ts, &(anp.an_path), argv[1], LOOKUP_NOISY) < 0)
    {
	bu_vls_printf(gedp->ged_result_str, "%s: cannot follow path for arc: '%s'\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    bu_vls_strcat(&pvls, argv[2]);
    parent = bu_vls_addr(&pvls);
    sep = strchr(parent, '/') - parent;
    bu_vls_trunc(&pvls, sep);
    switch (rt_db_lookup_internal(gedp->dbip, parent, &dp, &intern, LOOKUP_NOISY, &rt_uniresource)) {
	case ID_COMBINATION:
	    if (dp->d_flags & RT_DIR_COMB)
		break;
	    else {
		bu_vls_printf(gedp->ged_result_str,
			      "%s: Non-combination directory <%p> '%s' for combination rt_db_internal <%p>\nThis should not happen\n",
			      argv[0], (void *)dp, dp->d_namep, (void *)&intern);
	    }
	    /* fall through */
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s: Object '%s' is not a combination\n", argv[0], parent);
	    /* fall through */
	case ID_NULL:
	    bu_vls_free(&pvls);
	    return BRLCAD_ERROR;
    }
    comb = (struct rt_comb_internal *) intern.idb_ptr;
    RT_CK_COMB(comb);

    tp = db_find_named_leaf(comb->tree, child);
    if (tp == TREE_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: unable to find instance of '%s' in combination '%s'\n",
		      argv[0], child, dp->d_namep);
	status = BRLCAD_ERROR;
	goto wrapup;
    }

    /*
     * Finally, copy the matrix
     */
    if (!bn_mat_is_identity(ts.ts_mat)) {
	if (tp->tr_l.tl_mat == NULL)
	    tp->tr_l.tl_mat = bn_mat_dup(ts.ts_mat);
	else
	    MAT_COPY(tp->tr_l.tl_mat, ts.ts_mat);
    } else if (tp->tr_l.tl_mat != NULL) {
	bu_free((void *) tp->tr_l.tl_mat, "tl_mat");
	tp->tr_l.tl_mat = (matp_t) 0;
    }

    if (rt_db_put_internal(dp, gedp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", argv[0]);
	status = BRLCAD_ERROR;
	goto wrapup;
    }

    status = BRLCAD_OK;

wrapup:

    bu_vls_free(&pvls);
    if (status & BRLCAD_ERROR)
	rt_db_free_internal(&intern);
    return status;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl copymat_cmd_impl = {
    "copymat",
    ged_copymat_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd copymat_cmd = { &copymat_cmd_impl };
const struct ged_cmd *copymat_cmds[] = { &copymat_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  copymat_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
