/**
 * @file   dpkginfo.c
 * @brief  dpkginfo probe
 * @author "Pierre Chifflier" <chifflier@wzdftpd.net>
 */

/*
 * Copyright 2009 Red Hat Inc., Durham, North Carolina.
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
 *      "Pierre Chifflier <chifflier@wzdftpd.net>"
 */


/*
 * dpkginfo probe:
 *
 *  dpkginfo_object(string name)
 *
 *  dpkginfo_state(string name,
 *                string arch,
 *                string epoch,
 *                string release,
 *                string version,
 *                string evr,
 *                string signature_keyid)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/* SEAP */
#include <seap.h>
#include <probe-api.h>
#include <alloc.h>
#include <common/assume.h>
#include "common/debug_priv.h"


#include "dpkginfo-helper.h"


struct dpkginfo_global {
        pthread_mutex_t mutex;
};

static struct dpkginfo_global g_dpkg;


void *probe_init(void)
{
        pthread_mutex_init (&(g_dpkg.mutex), NULL);
        dpkginfo_init();

        return ((void *)&g_dpkg);
}

void probe_fini (void *ptr)
{
        struct dpkginfo_global *d = (struct dpkginfo_global *)ptr;

        dpkginfo_fini();
        pthread_mutex_destroy (&(d->mutex));

        return;
}

int probe_main (probe_ctx *ctx, void *arg)
{
        SEXP_t *val, *item, *ent;
        char *request_st = NULL;
        struct dpkginfo_reply_t *dpkginfo_reply = NULL;
        int errflag;

        ent = probe_obj_getent(probe_ctx_getobject(ctx), "name", 1);

        if (ent == NULL) {
                return (PROBE_ENOENT);
        }

        val = probe_ent_getval (ent);

        if (val == NULL) {
                dI("%s: no value\n", "name");
                SEXP_free (ent);
                return (PROBE_ENOVAL);
        }

        request_st = SEXP_string_cstr (val);
        SEXP_free (val);

        if (request_st == NULL) {
                switch (errno) {
                case EINVAL:
                        dI("%s: invalid value type\n", "name");
			return PROBE_EINVAL;
                        break;
                case EFAULT:
                        dI("%s: element not found\n", "name");
			return PROBE_ENOELM;
                        break;
		default:
			return PROBE_EUNKNOWN;
                }
        }

        /* get info from debian apt cache */
        pthread_mutex_lock (&(g_dpkg.mutex));
        dpkginfo_reply = dpkginfo_get_by_name(request_st, &errflag);
        pthread_mutex_unlock (&(g_dpkg.mutex));

        if (dpkginfo_reply == NULL) {
                switch (errflag) {
		case 0: /* Not found */
		{
			dI("Package \"%s\" not found.\n", request_st);
			break;
		}
		case -1: /* Error */
		{
			dI("dpkginfo_get_by_name failed.\n");
			item = probe_item_create(OVAL_LINUX_DPKG_INFO, NULL,
					"name", OVAL_DATATYPE_STRING, request_st,
					NULL);
			probe_item_setstatus (item, SYSCHAR_STATUS_ERROR);
			probe_item_collect(ctx, item);
			break;
		}
                }
        } else { /* Ok */
                int i;
                int num_items = 1; /* FIXME */

                for (i = 0; i < num_items; ++i) {
                        dI("%s: element found version %s\n", dpkginfo_reply->name, dpkginfo_reply->evr);
                        item = probe_item_create (OVAL_LINUX_DPKG_INFO, NULL,
                                        "name", OVAL_DATATYPE_STRING, dpkginfo_reply->name,
                                        "arch", OVAL_DATATYPE_STRING, dpkginfo_reply->arch,
                                        "epoch", OVAL_DATATYPE_STRING, dpkginfo_reply->epoch,
                                        "release", OVAL_DATATYPE_STRING, dpkginfo_reply->release,
                                        "version", OVAL_DATATYPE_STRING, dpkginfo_reply->version,
                                        "evr", OVAL_DATATYPE_EVR_STRING, dpkginfo_reply->evr,
                                        NULL);

			probe_item_collect(ctx, item);

                        dpkginfo_free_reply(dpkginfo_reply);
                }
        }

        SEXP_vfree(ent, NULL);
        oscap_free(request_st);

        return (0);
}
