/**
 * @file   sysctl.c
 * @brief  implementation of the sysctl_object
 * @author "Daniel Kopecek" <dkopecek@redhat.com>
 */

/*
 * Copyright 2011 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "Daniel Kopecek" <dkopecek@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <probe-api.h>
#include "probe/entcmp.h"

#if defined(__linux__)

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "oval_fts.h"
#include "common/debug_priv.h"
#include "common/assume.h"

#define PROC_SYS_DIR "/proc/sys"
#define PROC_SYS_MAXDEPTH 7

int probe_main(probe_ctx *ctx, void *probe_arg)
{
        OVAL_FTS    *ofts;
        OVAL_FTSENT *ofts_ent;

        SEXP_t *name_entity, *probe_in;
        SEXP_t *r0, *r1, *r2, *r3;
        SEXP_t *ent_attrs, *bh_entity, *path_entity, *filename_entity;
        oval_version_t over;
        int over_cmp;

        probe_in    = probe_ctx_getobject(ctx);
        name_entity = probe_obj_getent(probe_in, "name", 1);
        over        = probe_obj_get_schema_version(probe_in);
        over_cmp    = oval_version_cmp(over, OVAL_VERSION(5.10));

        if (name_entity == NULL) {
                dE("Missing \"name\" entity in the input object\n");
                return (PROBE_ENOENT);
        }

        /*
         * prepare behaviors
         */
        ent_attrs = probe_attr_creat("max_depth",           r0 = SEXP_string_newf("%d", PROC_SYS_MAXDEPTH),
                                     "recurse_direction",   r1 = SEXP_string_new("down", 4),
                                     "recurse_file_system", r2 = SEXP_string_new("defined", 7),
                                     "recurse", r3 = SEXP_string_new("symlinks and directories", 24),
                                     NULL);
        bh_entity = probe_ent_creat1("behaviors", ent_attrs, NULL);
        SEXP_vfree(r0, r1, r2, r3, ent_attrs, NULL);

        /*
         * prepare path, filename
         */
        ent_attrs = probe_attr_creat("operation", r0 = SEXP_number_newi(OVAL_OPERATION_EQUALS),
                                     NULL);
        path_entity = probe_ent_creat1("path", ent_attrs, r1 = SEXP_string_new(PROC_SYS_DIR, strlen(PROC_SYS_DIR)));
        SEXP_vfree(r0, r1, NULL);

        ent_attrs = probe_attr_creat("operation", r0 = SEXP_number_newi(OVAL_OPERATION_PATTERN_MATCH),
                                     NULL);
        filename_entity = probe_ent_creat1("filename", ent_attrs, r1 = SEXP_string_new(".*", 2));
        SEXP_vfree(r0, r1, ent_attrs, NULL);

        /*
         * collect sysctls
         *  XXX: use direct access for the "equals" op
         */
        ofts = oval_fts_open(path_entity, filename_entity, NULL, bh_entity);

        if (ofts == NULL) {
                dE("oval_ftp_open(%s, %s) failed\n", PROC_SYS_DIR, ".\\+");
                SEXP_vfree(path_entity, filename_entity, bh_entity, name_entity, NULL);

                return (PROBE_EFATAL);
        }

        while ((ofts_ent = oval_fts_read(ofts)) != NULL) {
                SEXP_t *se_mib;
                char    mibpath[PATH_MAX], *mib;
                size_t  miblen;

                snprintf(mibpath, sizeof mibpath, "%s/%s", ofts_ent->path, ofts_ent->file);

                mib    = strdup(mibpath + strlen(PROC_SYS_DIR) + 1);
                miblen = strlen(mib);

                while (miblen > 0) {
                        if(mib[miblen - 1] == '/')
                                mib[miblen - 1] = '.';
                        --miblen;
                }

                dI("MIB: %s\n", mib);
                se_mib = SEXP_string_new(mib, strlen(mib));
                oscap_free(mib);

                if (probe_entobj_cmp(name_entity, se_mib) == OVAL_RESULT_TRUE) {
                        FILE   *fp;
                        SEXP_t *item;
                        char    sysval[8192];
                        char   *sysvals[512];
                        long i, l;
                        size_t s;

                        dI("MIB match\n");

                        /*
                         * read sysctl value
                         */
                        fp = fopen(mibpath, "r");

                        if (fp == NULL) {
                                dE("Can't read sysctl value from \"%s\": %u, %s\n",
                                   mibpath, errno, strerror(errno));
                                goto fail_item;
                        }

                        l = fread(sysval, 1, sizeof sysval - 1, fp);

                        if (ferror(fp)) {
                                dE("An error ocured when reading from \"%s\" (fp=%p): l=%ld, %u, %s\n",
                                    mibpath, fp, l, errno, strerror(errno));
                                goto fail_item;
                        }

                        fclose(fp);

                        /*
                         * sanitize the value
                         *  - only printable and whitespace chars allowed
                         *  - remove the last '\n'
                         */
                        sysvals[0] = sysval;

                        for(s = 0, i = 0; i < l && s < sizeof sysvals/sizeof(char *) - 1; ++i) {
	                        if ((!isprint(sysval[i]) && !isspace(sysval[i]))
	                            || (over_cmp >= 0 && sysval[i] == '\n' /* OVAL 5.10 and above */))
                                {
                                        sysval[i] = '\0';
                                        sysvals[++s] = sysval + i + 1;
                                }
                        }

                        if (sysval[l - 1] == '\n')
                                sysval[l - 1] = '\0';
                        else
                                sysval[l] = '\0';

                        if (strlen(sysvals[s]) == 0)
                                sysvals[s] = NULL;
                        else
                                sysvals[++s] = NULL;

                        if (over_cmp >= 0) {
	                        /* Only in OVAL 5.10 and above */
	                        item = probe_item_create(OVAL_UNIX_SYSCTL, NULL,
	                                                 "name",  OVAL_DATATYPE_SEXP,   se_mib,
	                                                 "value", OVAL_DATATYPE_STRING_M, sysvals,
	                                                 NULL);
                        } else {
	                        item = probe_item_create(OVAL_UNIX_SYSCTL, NULL,
	                                                 "name",  OVAL_DATATYPE_SEXP,   se_mib,
	                                                 "value", OVAL_DATATYPE_STRING, sysval,
	                                                 NULL);
                        }

                        goto add_item;
                fail_item:
                        if (fp != NULL)
                                fclose(fp);

                        item = probe_item_creat("sysctl_item", NULL, NULL);
                        probe_item_setstatus(item, SYSCHAR_STATUS_ERROR);
                add_item:
                        probe_item_collect(ctx, item);
                }

                oval_ftsent_free(ofts_ent);
                SEXP_free(se_mib);
        }

        oval_fts_close(ofts);
	SEXP_vfree(path_entity, filename_entity, bh_entity, name_entity);

        return (0);
}
#else
int probe_main(probe_ctx *ctx, void *probe_arg)
{
        return(PROBE_EOPNOTSUPP);
}
#endif
