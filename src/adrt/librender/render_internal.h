/*               R E N D E R _ I N T E R N A L . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2025 United States Government as represented by
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
/** @file librender/render_internal.h
 *
 */

#ifndef ADRT_LIBRENDER_RENDER_INTERNAL_H
#define ADRT_LIBRENDER_RENDER_INTERNAL_H

#include "common.h"
#include "rt/tie.h"

#ifndef RENDER_EXPORT
#  if defined(RENDER_DLL_EXPORTS) && defined(RENDER_DLL_IMPORTS)
#    error "Only RENDER_DLL_EXPORTS or RENDER_DLL_IMPORTS can be defined, not both."
#  elif defined(STATIC_BUILD)
#    define RENDER_EXPORT
#  elif defined(RENDER_DLL_EXPORTS)
#    define RENDER_EXPORT COMPILER_DLLEXPORT
#  elif defined(RENDER_DLL_IMPORTS)
#    define RENDER_EXPORT COMPILER_DLLIMPORT
#  else
#    define RENDER_EXPORT
#  endif
#endif

#define RENDER_METHOD_COMPONENT	0x01
#define RENDER_METHOD_CUT	0x02
#define RENDER_METHOD_DEPTH	0x03
#define RENDER_METHOD_FLAT	0x04
#define RENDER_METHOD_FLOS	0x05
#define RENDER_METHOD_GRID	0x06
#define RENDER_METHOD_NORMAL	0x07
#define RENDER_METHOD_PHONG	0x08
#define RENDER_METHOD_PATH	0x09
#define RENDER_METHOD_SPALL	0x0A
#define RENDER_METHOD_SURFEL	0x0B

#define RENDER_METHOD_PLANE	RENDER_METHOD_CUT


#define RENDER_MAX_DEPTH	24


struct render_s;
typedef void render_work_t(struct render_s *render, struct tie_s *tie, struct tie_ray_s *ray, vect_t *pixel);
typedef void render_free_t(struct render_s *render);

typedef struct render_s {
    char name[256];
    struct tie_s *tie;
    render_work_t *work;
    render_free_t *free;
    void *data;
    struct render_s *next;
    const char *shader;
} render_t;

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
