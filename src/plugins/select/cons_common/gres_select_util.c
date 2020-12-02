/*****************************************************************************\
 *  gres_select_util.c - filters used in the select plugin
 *****************************************************************************
 *  Copyright (C) 2020 SchedMD LLC.
 *  Derived in large part from code previously in common/gres.h
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

#include "src/common/slurm_xlator.h"

#include "gres_select_util.h"

#include "src/common/xstring.h"

/*
 * Set job default parameters in a given element of a list
 * IN job_gres_list - job's gres_list built by gres_plugin_job_state_validate()
 * IN gres_name - name of gres, apply defaults to all elements (e.g. updates to
 *		  gres_name="gpu" would apply to "gpu:tesla", "gpu:volta", etc.)
 * IN cpu_per_gpu - value to set as default
 * IN mem_per_gpu - value to set as default
 * OUT *cpus_per_tres - CpusPerTres string displayed by scontrol show job
 * OUT *mem_per_tres - MemPerTres string displayed by scontrol show job
 */
extern void gres_select_util_job_set_defs(List job_gres_list,
					  char *gres_name,
					  uint64_t cpu_per_gpu,
					  uint64_t mem_per_gpu,
					  char **cpus_per_tres,
					  char **mem_per_tres)
{
	uint32_t plugin_id;
	ListIterator gres_iter;
	gres_state_t *gres_ptr = NULL;
	gres_job_state_t *job_gres_data;

	/*
	 * Currently only GPU supported, check how cpus_per_tres/mem_per_tres
	 * is handled in _fill_job_desc_from_sbatch_opts and
	 * _job_desc_msg_create_from_opts.
	 */
	xassert(!xstrcmp(gres_name, "gpu"));

	if (!job_gres_list)
		return;

	plugin_id = gres_plugin_build_id(gres_name);
	gres_iter = list_iterator_create(job_gres_list);
	while ((gres_ptr = (gres_state_t *) list_next(gres_iter))) {
		if (gres_ptr->plugin_id != plugin_id)
			continue;
		job_gres_data = (gres_job_state_t *) gres_ptr->gres_data;
		if (!job_gres_data)
			continue;
		job_gres_data->def_cpus_per_gres = cpu_per_gpu;
		job_gres_data->def_mem_per_gres = mem_per_gpu;
		if (!job_gres_data->cpus_per_gres) {
			xfree(*cpus_per_tres);
			if (cpu_per_gpu)
				xstrfmtcat(*cpus_per_tres, "gpu:%"PRIu64,
					   cpu_per_gpu);
		}
		if (!job_gres_data->mem_per_gres) {
			xfree(*mem_per_tres);
			if (mem_per_gpu)
				xstrfmtcat(*mem_per_tres, "gpu:%"PRIu64,
					   mem_per_gpu);
		}
	}
	list_iterator_destroy(gres_iter);
}
