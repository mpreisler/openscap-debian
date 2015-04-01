/*
 * Copyright 2010 Red Hat Inc., Durham, North Carolina.
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
#pragma once
#ifndef CRAPI_SHA2_H
#define CRAPI_SHA2_H

#include <stddef.h>

void *crapi_sha224_init (void *dst, void *size);
int   crapi_sha224_update (void *ctxp, void *bptr, size_t blen);
int   crapi_sha224_fini (void *ctxp);
void  crapi_sha224_free (void *ctxp);

int crapi_sha224_fd (int fd, void *dst, size_t *size);

void *crapi_sha256_init (void *dst, void *size);
int   crapi_sha256_update (void *ctxp, void *bptr, size_t blen);
int   crapi_sha256_fini (void *ctxp);
void  crapi_sha256_free (void *ctxp);

int crapi_sha256_fd (int fd, void *dst, size_t *size);

void *crapi_sha384_init (void *dst, void *size);
int   crapi_sha384_update (void *ctxp, void *bptr, size_t blen);
int   crapi_sha384_fini (void *ctxp);
void  crapi_sha384_free (void *ctxp);

int crapi_sha384_fd (int fd, void *dst, size_t *size);

void *crapi_sha512_init (void *dst, void *size);
int   crapi_sha512_update (void *ctxp, void *bptr, size_t blen);
int   crapi_sha512_fini (void *ctxp);
void  crapi_sha512_free (void *ctxp);

int crapi_sha512_fd (int fd, void *dst, size_t *size);

#endif /* CRAPI_SHA2_H */
