/*****************************************************************************\
 *  cgroup_common.c - Cgroup plugin common functions
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

#include "cgroup_common.h"

/*
 * Returns the path to the cgroup.procs file over which we have permissions
 * defined by check_mode. This path is where we'll be able to read or write
 * pids. If there are no paths available with these permisisons, return NULL,
 * which means the cgroup doesn't exist or we do not have permissions to modify
 * the cg.
 */
static char *_cgroup_procs_check (xcgroup_t *cg, int check_mode)
{
	struct stat st;
	char *path = xstrdup_printf("%s/%s", cg->path, "cgroup.procs");

	if (!((stat (path, &st) >= 0) && (st.st_mode & check_mode)))
		xfree(path);

	return path;
}

static char *_cgroup_procs_readable_path (xcgroup_t *cg)
{
	return _cgroup_procs_check(cg, S_IRUSR);
}

static char *_cgroup_procs_writable_path (xcgroup_t *cg)
{
	return _cgroup_procs_check(cg, S_IWUSR);
}

static int _set_uint32_param(xcgroup_t* cg, char* param, uint32_t value)
{
	int fstatus = SLURM_ERROR;
	char file_path[PATH_MAX];
	char* cpath = cg->path;

	if (snprintf(file_path, PATH_MAX, "%s/%s", cpath, param) >= PATH_MAX) {
		debug2("unable to build filepath for '%s' and"
		       " parameter '%s' : %m", cpath, param);
		return fstatus;
	}

	fstatus = common_file_write_uint32s(file_path, &value, 1);
	if (fstatus != SLURM_SUCCESS)
		debug2("%s: unable to set parameter '%s' to '%u' for '%s'",
			__func__, param, value, cpath);
	else
		debug3("%s: parameter '%s' set to '%u' for '%s'",
			__func__, param, value, cpath);

	return fstatus;
}

extern size_t common_file_getsize(int fd)
{
	int rc;
	size_t fsize;
	off_t offset;
	char c;

	/* store current position and rewind */
	offset = lseek(fd, 0, SEEK_CUR);
	if (offset < 0)
		return -1;
	if (lseek(fd, 0, SEEK_SET) < 0)
		error("%s: lseek(0): %m", __func__);

	/* get file size */
	fsize = 0;
	do {
		rc = read(fd, (void*)&c, 1);
		if (rc > 0)
			fsize++;
	} while ((rc < 0 && errno == EINTR) || rc > 0);

	/* restore position */
	if (lseek(fd, offset, SEEK_SET) < 0)
		error("%s: lseek(): %m", __func__);

	if (rc < 0)
		return -1;
	else
		return fsize;
}

extern int common_file_write_uint64s(char* file_path, uint64_t* values, int nb)
{
	int fstatus;
	int rc;
	int fd;
	char tstr[256];
	uint64_t value;
	int i;

	/* open file for writing */
	fd = open(file_path, O_WRONLY, 0700);
	if (fd < 0) {
		debug2("%s: unable to open '%s' for writing : %m",
			__func__, file_path);
		return SLURM_ERROR;
	}

	/* add one value per line */
	fstatus = SLURM_SUCCESS;
	for (i = 0; i < nb ; i++) {

		value = values[i];

		rc = snprintf(tstr, sizeof(tstr), "%"PRIu64"", value);
		if (rc < 0) {
			debug2("unable to build %"PRIu64" string value, "
			       "skipping", value);
			fstatus = SLURM_ERROR;
			continue;
		}

		do {
			rc = write(fd, tstr, strlen(tstr)+1);
		}
		while (rc < 0 && errno == EINTR);
		if (rc < 1) {
			debug2("%s: unable to add value '%s' to file '%s' : %m",
				__func__, tstr, file_path);
			if (errno != ESRCH)
				fstatus = SLURM_ERROR;
		}

	}

	/* close file */
	close(fd);

	return fstatus;
}

extern int common_file_read_uint64s(char* file_path, uint64_t** pvalues,
				    int* pnb)
{
	int rc;
	int fd;

	size_t fsize;
	char* buf;
	char* p;

	uint64_t* pa=NULL;
	int i;

	/* check input pointers */
	if (pvalues == NULL || pnb == NULL)
		return SLURM_ERROR;

	/* open file for reading */
	fd = open(file_path, O_RDONLY, 0700);
	if (fd < 0) {
		debug2("%s: unable to open '%s' for reading : %m",
			__func__, file_path);
		return SLURM_ERROR;
	}

	/* get file size */
	fsize = common_file_getsize(fd);
	if (fsize == -1) {
		close(fd);
		return SLURM_ERROR;
	}

	/* read file contents */
	buf = xmalloc(fsize + 1);
	do {
		rc = read(fd, buf, fsize);
	} while (rc < 0 && errno == EINTR);
	close(fd);
	buf[fsize]='\0';

	/* count values (splitted by \n) */
	i=0;
	if (rc > 0) {
		p = buf;
		while (xstrchr(p, '\n') != NULL) {
			i++;
			p = xstrchr(p, '\n') + 1;
		}
	}

	/* build uint64_t list */
	if (i > 0) {
		pa = (uint64_t*) xmalloc(sizeof(uint64_t) * i);
		p = buf;
		i = 0;
		while (xstrchr(p, '\n') != NULL) {
			long long unsigned int ll_tmp;
			sscanf(p, "%llu", &ll_tmp);
			pa[i++] = ll_tmp;
			p = xstrchr(p, '\n') + 1;
		}
	}

	/* free buffer */
	xfree(buf);

	/* set output values */
	*pvalues = pa;
	*pnb = i;

	return SLURM_SUCCESS;
}

extern int common_file_write_uint32s(char* file_path, uint32_t* values, int nb)
{
	int rc;
	int fd;
	char tstr[256];

	/* open file for writing */
	if ((fd = open(file_path, O_WRONLY, 0700)) < 0) {
		error("%s: unable to open '%s' for writing: %m",
			__func__, file_path);
		return SLURM_ERROR;
	}

	/* add one value per line */
	for (int i = 0; i < nb; i++) {
		uint32_t value = values[i];

		if (snprintf(tstr, sizeof(tstr), "%u", value) < 0)
			fatal("%s: unable to build %u string value",
			      __func__, value);

		/* write terminating NUL byte */
		safe_write(fd, tstr, strlen(tstr) + 1);
	}

	/* close file */
	close(fd);
	return SLURM_SUCCESS;

rwfail:
	rc = errno;
	error("%s: write pid %s to %s failed: %m",
	      __func__, tstr, file_path);
	close(fd);
	return rc;;
}

extern int common_file_read_uint32s(char *file_path, uint32_t **pvalues,
				    int *pnb)
{
	int rc;
	int fd;

	size_t fsize;
	char* buf;
	char* p;

	uint32_t* pa=NULL;
	int i;

	/* check input pointers */
	if (pvalues == NULL || pnb == NULL)
		return SLURM_ERROR;

	/* open file for reading */
	fd = open(file_path, O_RDONLY, 0700);
	if (fd < 0) {
		debug2("%s: unable to open '%s' for reading : %m",
			__func__, file_path);
		return SLURM_ERROR;
	}

	/* get file size */
	fsize = common_file_getsize(fd);
	if (fsize == -1) {
		close(fd);
		return SLURM_ERROR;
	}

	/* read file contents */
	buf = xmalloc(fsize + 1);
	do {
		rc = read(fd, buf, fsize);
	} while (rc < 0 && errno == EINTR);
	close(fd);
	buf[fsize]='\0';

	/* count values (splitted by \n) */
	i=0;
	if (rc > 0) {
		p = buf;
		while (xstrchr(p, '\n') != NULL) {
			i++;
			p = xstrchr(p, '\n') + 1;
		}
	}

	/* build uint32_t list */
	if (i > 0) {
		pa = (uint32_t*) xmalloc(sizeof(uint32_t) * i);
		p = buf;
		i = 0;
		while (xstrchr(p, '\n') != NULL) {
			sscanf(p, "%u", pa+i);
			p = xstrchr(p, '\n') + 1;
			i++;
		}
	}

	/* free buffer */
	xfree(buf);

	/* set output values */
	*pvalues = pa;
	*pnb = i;

	return SLURM_SUCCESS;
}

extern int common_file_write_content(char *file_path, char *content,
				     size_t csize)
{
	int fd;

	/* open file for writing */
	if ((fd = open(file_path, O_WRONLY, 0700)) < 0) {
		error("%s: unable to open '%s' for writing: %m",
			__func__, file_path);
		return SLURM_ERROR;
	}

	safe_write(fd, content, csize);

	/* close file */
	close(fd);
	return SLURM_SUCCESS;

rwfail:
	error("%s: unable to write %zu bytes to cgroup %s: %m",
	      __func__, csize, file_path);
	close(fd);
	return SLURM_ERROR;
}

extern int common_file_read_content(char* file_path, char** content,
				    size_t *csize)
{
	int fstatus;
	int rc;
	int fd;
	size_t fsize;
	char* buf;

	fstatus = SLURM_ERROR;

	/* check input pointers */
	if (content == NULL || csize == NULL)
		return fstatus;

	/* open file for reading */
	fd = open(file_path, O_RDONLY, 0700);
	if (fd < 0) {
		debug2("%s: unable to open '%s' for reading : %m",
			__func__, file_path);
		return fstatus;
	}

	/* get file size */
	fsize=common_file_getsize(fd);
	if (fsize == -1) {
		close(fd);
		return fstatus;
	}

	/* read file contents */
	buf = xmalloc(fsize + 1);
	buf[fsize]='\0';
	do {
		rc = read(fd, buf, fsize);
	} while (rc < 0 && errno == EINTR);

	/* set output values */
	if (rc >= 0) {
		*content = buf;
		*csize = rc;
		fstatus = SLURM_SUCCESS;
	} else {
		xfree(buf);
	}

	/* close file */
	close(fd);

	return fstatus;
}

extern int common_cgroup_instantiate(xcgroup_t *cg)
{
	int fstatus = SLURM_ERROR;
	mode_t cmask;
	mode_t omask;

	char* file_path;
	uid_t uid;
	gid_t gid;

	/* init variables based on input cgroup */
	file_path = cg->path;
	uid = cg->uid;
	gid = cg->gid;

	/* save current mask and apply working one */
	cmask = S_IWGRP | S_IWOTH;
	omask = umask(cmask);

	/* build cgroup */
	if (mkdir(file_path, 0755)) {
		if (errno != EEXIST) {
			error("%s: unable to create cgroup '%s' : %m",
			      __func__, file_path);
			umask(omask);
			return fstatus;
		} else {
			debug3("%s: cgroup '%s' already exists",
			       __func__, file_path);
		}
	}
	umask(omask);

	/* change cgroup ownership as requested */
	if (chown(file_path, uid, gid)) {
		error("%s: unable to chown %d:%d cgroup '%s' : %m",
		      __func__, uid, gid, file_path);
		return fstatus;
	}

	/* following operations failure might not result in a general
	 * failure so set output status to success */
	fstatus = SLURM_SUCCESS;

	return fstatus;
}

extern int common_cgroup_create(xcgroup_ns_t* cgns, xcgroup_t* cg, char* uri,
				uid_t uid,  gid_t gid)
{
	int fstatus = SLURM_ERROR;
	char file_path[PATH_MAX];

	/* build cgroup absolute path*/
	if (snprintf(file_path, PATH_MAX, "%s%s", cgns->mnt_point,
		      uri) >= PATH_MAX) {
		debug2("unable to build cgroup '%s' absolute path in ns '%s' "
		       ": %m", uri, cgns->subsystems);
		return fstatus;
	}

	/* fill xcgroup structure */
	cg->ns = cgns;
	cg->name = xstrdup(uri);
	cg->path = xstrdup(file_path);
	cg->uid = uid;
	cg->gid = gid;

	return SLURM_SUCCESS;
}

extern int common_cgroup_move_process (xcgroup_t *cg, pid_t pid)
{
	char *path = NULL;

	/*
	 * First we check permissions to see if we will be able to move the pid.
	 * The path is a path to cgroup.procs and writting there will instruct
	 * the cgroup subsystem to move the process and all its threads there.
	 */
	path = _cgroup_procs_writable_path(cg);

	if (!path) {
		debug2("Cannot write to cgroup.procs for %s", cg->path);
		return SLURM_ERROR;
	}

	xfree(path);

	return _set_uint32_param(cg, "cgroup.procs", pid);
}

extern int common_cgroup_set_param(xcgroup_t* cg, char* param, char* content)
{
	int fstatus = SLURM_ERROR;
	char file_path[PATH_MAX];
	char* cpath = cg->path;

	if (!content) {
		debug2("%s: no content given, nothing to do.", __func__);
		return fstatus;
	}

	if (snprintf(file_path, PATH_MAX, "%s/%s", cpath, param) >= PATH_MAX) {
		debug2("unable to build filepath for '%s' and"
		       " parameter '%s' : %m", cpath, param);
		return fstatus;
	}

	fstatus = common_file_write_content(file_path, content,
					    strlen(content));
	if (fstatus != SLURM_SUCCESS)
		debug2("%s: unable to set parameter '%s' to '%s' for '%s'",
			__func__, param, content, cpath);
	else
		debug3("%s: parameter '%s' set to '%s' for '%s'",
			__func__, param, content, cpath);

	return fstatus;
}

extern void common_cgroup_ns_destroy(xcgroup_ns_t* cgns)
{
	xfree(cgns->mnt_point);
	xfree(cgns->mnt_args);
	xfree(cgns->subsystems);
}

extern void common_cgroup_destroy(xcgroup_t* cg)
{
	cg->ns = NULL;
	xfree(cg->name);
	xfree(cg->path);
	cg->uid = -1;
	cg->gid = -1;
}

extern int common_cgroup_delete(xcgroup_t* cg)
{
	uint16_t retries = 0;

	/*
	 *  Simply delete cgroup with rmdir(2). If cgroup doesn't
	 *   exist, do not propagate error back to caller.
	 *
	 * Do 5 retries if we receive an EBUSY, because we may be trying to
	 * remove the directory when the kernel hasn't yet drained the cgroup
	 * internal references (css_online), even if cgroup.procs is already
	 * empty.
	 */
retry:
	if (cg && cg->path && (rmdir(cg->path) < 0) && (errno != ENOENT)) {
		if ((errno == EBUSY) && retries < 5) {
			sleep(0.5);
			retries++;
			goto retry;
		}
		debug2("%s: did %d retries rmdir(%s): %m", __func__, retries,
		       cg->path);
		return SLURM_ERROR;
	}

	if (retries)
		debug2("%s: rmdir(%s): took %"PRIu16" retries, possible cgroup filesystem slowness",
		       __func__, cg->path, retries);

	return SLURM_SUCCESS;
}

extern int common_cgroup_add_pids(xcgroup_t* cg, pid_t* pids, int npids)
{
	int rc = SLURM_ERROR;
	char* path = _cgroup_procs_writable_path(cg);

	rc = common_file_write_uint32s(path, (uint32_t*)pids, npids);
	if (rc != SLURM_SUCCESS)
		debug2("%s: unable to add pids to '%s'", __func__, cg->path);

	xfree(path);
	return rc;
}

extern int common_cgroup_get_pids(xcgroup_t* cg, pid_t **pids, int *npids)
{
	int fstatus = SLURM_ERROR;
	char* path = NULL;

	if (pids == NULL || npids == NULL || !cg->path)
		return SLURM_ERROR;

	path = _cgroup_procs_readable_path(cg);
	if (!path) {
		debug2("%s: unable to read '%s/cgroup.procs'", __func__,
		       cg->path);
		return SLURM_ERROR;
	}

	fstatus = common_file_read_uint32s(path, (uint32_t**)pids, npids);
	if (fstatus != SLURM_SUCCESS)
		debug2("%s: unable to get pids of '%s', file disappeared?",
		       __func__, path);

	xfree(path);
	return fstatus;
}

extern int common_cgroup_get_param(xcgroup_t* cg, char* param, char **content,
				   size_t *csize)
{
	int fstatus = SLURM_ERROR;
	char file_path[PATH_MAX];
	char* cpath = cg->path;

	if (snprintf(file_path, PATH_MAX, "%s/%s", cpath, param) >= PATH_MAX) {
		debug2("unable to build filepath for '%s' and"
		       " parameter '%s' : %m", cpath, param);
	} else {
		fstatus = common_file_read_content(file_path, content, csize);
		if (fstatus != SLURM_SUCCESS)
			debug2("%s: unable to get parameter '%s' for '%s'",
			       __func__, param, cpath);
	}
	return fstatus;
}

extern int common_cgroup_set_uint64_param(xcgroup_t* cg, char* param,
					  uint64_t value)
{
	int fstatus = SLURM_ERROR;
	char file_path[PATH_MAX];
	char* cpath = cg->path;

	if (snprintf(file_path, PATH_MAX, "%s/%s", cpath, param) >= PATH_MAX) {
		debug2("unable to build filepath for '%s' and"
		       " parameter '%s' : %m", cpath, param);
		return fstatus;
	}

	fstatus = common_file_write_uint64s(file_path, &value, 1);
	if (fstatus != SLURM_SUCCESS)
		debug2("%s: unable to set parameter '%s' to '%"PRIu64"' for "
			"'%s'", __func__, param, value, cpath);
	else
		debug3("%s: parameter '%s' set to '%"PRIu64"' for '%s'",
			__func__, param, value, cpath);

	return fstatus;
}
