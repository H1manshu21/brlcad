/*                         W H I C H _ S H A D E R . C
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
/** @file libged/which_shader.c
 *
 * The which_shader commands.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_which_core_shader(struct ged *gedp, int argc, const char *argv[])
{
    int j;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int sflag;
    int myArgc;
    char **myArgv;
    static const char *usage = "[-s] args";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    myArgc = argc;
    myArgv = (char **)argv;
    sflag = 0;

    if (myArgc > 1 && BU_STR_EQUAL(myArgv[1], "-s")) {
	--myArgc;
	++myArgv;
	sflag = 1;
    }

    if (myArgc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (j = 1; j < myArgc; j++) {

	if (!sflag)
	    bu_vls_printf(gedp->ged_result_str, "Combination[s] with shader %s:\n", myArgv[j]);

	/* Examine all COMB nodes */
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	    if (!(dp->d_flags & RT_DIR_COMB))
		continue;

	    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting.\n");
		return BRLCAD_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;

	    if (!strstr(bu_vls_addr(&comb->shader), myArgv[j]))
		continue;

	    if (sflag)
		bu_vls_printf(gedp->ged_result_str, " %s", dp->d_namep);
	    else
		bu_vls_printf(gedp->ged_result_str, "   %s\n", dp->d_namep);
	    intern.idb_meth->ft_ifree(&intern);
	} FOR_ALL_DIRECTORY_END;
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl which_shader_cmd_impl = {
    "which_shader",
    ged_which_core_shader,
    GED_CMD_DEFAULT
};

const struct ged_cmd which_shader_cmd = { &which_shader_cmd_impl };
const struct ged_cmd *which_shader_cmds[] = { &which_shader_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  which_shader_cmds, 1 };

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
