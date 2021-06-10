/*****************************************************************************\
 *  cgroup_v1.c - Cgroup v1 plugin
 *****************************************************************************
 *  Copyright (C) 2021 SchedMD LLC
 *  Written by Felip Moll <felip.moll@schedmd.com>
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#define _GNU_SOURCE

#include "cgroup_v1.h"

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 *
 * plugin_name - a string giving a human-readable description of the
 * plugin.  There is no maximum length, but the symbol must refer to
 * a valid string.
 *
 * plugin_type - a string suggesting the type of the plugin or its
 * applicability to a particular form of data or method of data handling.
 * If the low-level plugin API is used, the contents of this string are
 * unimportant and may be anything.  Slurm uses the higher-level plugin
 * interface which requires this string to be of the form
 *
 *	<application>/<method>
 *
 * where <application> is a description of the intended application of
 * the plugin (e.g., "select" for Slurm node selection) and <method>
 * is a description of how this plugin satisfies that application.  Slurm will
 * only load select plugins if the plugin_type string has a
 * prefix of "select/".
 *
 * plugin_version - an unsigned 32-bit integer containing the Slurm version
 * (major.minor.micro combined into a single number).
 */
const char plugin_name[] = "Cgroup v1 plugin";
const char plugin_type[] = "cgroup/v1";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

static char g_user_cgpath[CG_CTL_CNT][PATH_MAX];
static char g_job_cgpath[CG_CTL_CNT][PATH_MAX];
static char g_step_cgpath[CG_CTL_CNT][PATH_MAX];

static xcgroup_ns_t g_cg_ns[CG_CTL_CNT];

static xcgroup_t g_root_cg[CG_CTL_CNT];
//static xcgroup_t g_user_cg[CG_CTL_CNT];
//static xcgroup_t g_job_cg[CG_CTL_CNT];
//static xcgroup_t g_step_cg[CG_CTL_CNT];

const char *g_cg_name[CG_CTL_CNT] = {
	"freezer",
	"cpuset",
	"memory",
	"devices",
	"cpuacct"
};

extern int init(void)
{
	int i;

	for (i = 0; i < CG_CTL_CNT; i++) {
		g_user_cgpath[i][0] = '\0';
		g_job_cgpath[i][0] = '\0';
		g_step_cgpath[i][0] = '\0';
	}

	debug("%s loaded", plugin_name);
	return SLURM_SUCCESS;
}

extern int fini(void)
{
	debug("unloading %s", plugin_name);
	return SLURM_SUCCESS;
}

extern int cgroup_p_initialize(cgroup_ctl_type_t sub)
{
	int rc = SLURM_SUCCESS;
	return rc;
}

/*
 * cgroup_p_step_create - Description Description Description
 * IN/OUT param1 - description description
 * IN param2 - description description
 * RET zero on success, EINVAL otherwise
 */
extern int cgroup_p_step_create(cgroup_ctl_type_t sub, stepd_step_rec_t *job)
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_step_addto(cgroup_ctl_type_t sub, pid_t *pids, int npids)
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_step_get_pids(pid_t **pids, int *npids)
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_step_suspend()
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_step_resume()
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_step_destroy(cgroup_ctl_type_t sub)
{
	return SLURM_SUCCESS;
}

/*
 * Is the specified pid in our cgroup g_cg_ns[CG_TRACK]?
 * In the future we may want to replace this with a get pids and a search.
 */
extern bool cgroup_p_has_pid(pid_t pid)
{
	return true;
}

extern void cgroup_p_free_conf(slurm_cgroup_conf_t *cg_conf)
{
	cgroup_free_conf(cg_conf);
}

extern slurm_cgroup_conf_t *cgroup_p_get_conf()
{
	return cgroup_get_conf();
}

extern cgroup_limits_t *cgroup_p_root_constrain_get(cgroup_ctl_type_t sub)
{
	return NULL;
}

extern int cgroup_p_root_constrain_set(cgroup_ctl_type_t sub,
				       cgroup_limits_t *limits)
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_user_constrain_set(cgroup_ctl_type_t sub,
				       stepd_step_rec_t *job,
				       cgroup_limits_t *limits)
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_job_constrain_set(cgroup_ctl_type_t sub,
				      stepd_step_rec_t *job,
				      cgroup_limits_t *limits)
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_step_constrain_set(cgroup_ctl_type_t sub,
				       stepd_step_rec_t *job,
				       cgroup_limits_t *limits)
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_step_start_oom_mgr()
{
	return SLURM_SUCCESS;
}

extern cgroup_oom_t *cgroup_p_step_stop_oom_mgr(stepd_step_rec_t *job)
{
	return NULL;
}

extern int cgroup_p_accounting_init()
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_accounting_fini()
{
	return SLURM_SUCCESS;
}

extern int cgroup_p_task_addto_accounting(pid_t pid, stepd_step_rec_t *job,
					  uint32_t task_id)
{
	return SLURM_SUCCESS;
}

extern cgroup_acct_t *cgroup_p_task_get_acct_data(uint32_t taskid)
{
	return NULL;
}
