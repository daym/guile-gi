// Copyright (C) 2019 Michael L. Gran

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef GIG_AMAP_H
#define GIG_AMAP_H

#include <glib.h>
#include <girepository.h>
#include "gig_data_type.h"

// *INDENT-OFF*
G_BEGIN_DECLS
// *INDENT-ON*

// This module gathers information to help convert between 3 paradigms
//  - arguments as they appear in GIArgInfo lists
//  - arguments as they appear in SCM gsubr calls
//  - arguments as they appear in g_function_info_invoke calls
typedef enum
{
    GIG_ARG_DIRECTION_VOID,
    GIG_ARG_DIRECTION_INPUT,
    GIG_ARG_DIRECTION_INOUT,
    GIG_ARG_DIRECTION_PREALLOCATED_OUTPUT,
    GIG_ARG_DIRECTION_OUTPUT,
    GIG_ARG_DIRECTION_COUNT
} GigArgDirection;

typedef enum
{
    GIG_ARG_TUPLE_SINGLETON,
    GIG_ARG_TUPLE_ARRAY,
    GIG_ARG_TUPLE_ARRAY_SIZE,
    GIG_ARG_TUPLE_COUNT
} GigArgTuple;

typedef enum
{
    GIG_ARG_PRESENCE_REQUIRED,
    GIG_ARG_PRESENCE_OPTIONAL,
    GIG_ARG_PRESENCE_IMPLICIT,
    GIG_ARG_PRESENCE_COUNT
} GigArgPresence;

// This structure contains the information necessary for choosing how
// to convert between scheme and C arguments of a function or method
// call.
typedef struct _GigArgMapEntry GigArgMapEntry;
struct _GigArgMapEntry
{
    gchar *name;
    GigTypeMeta meta;

    ////////////////////////////////////////////////////////////////
    // This block is derived information about how to map
    // Scheme to C and back again

    GigArgDirection s_direction;
    // If this argument can be mapped standalone, or requires another
    // argument's information to map between C and Scheme
    GigArgTuple tuple;
    // If this arg is optional in the Scheme GSubr.
    GigArgPresence presence;
    // This arg's index in g_callable_info_get_arg()

    guint8 is_c_input:1;
    guint8 is_c_output:1;
    guint8 is_s_input:1;
    guint8 is_s_output:1;
    guint8 padding:4;

    gint i;
    // This arg's position in input args of g_function_info_invoke
    gint c_input_pos;
    // This arg's position in the output args of g_function_info_invoke
    gint c_output_pos;
    // This arg's position in the Scheme GSubr
    gint s_input_pos;
    // This arg's position in the return values list
    gint s_output_pos;
    // When non-NULL, this is the entry of the array length argument
    // for this array argument.

    GigArgMapEntry *child;
    GigArgMapEntry *parent;
};

typedef struct _GigArgMap GigArgMap;
struct _GigArgMap
{
    gchar *name;

    // S arguments.
    gint s_input_req;
    gint s_input_opt;

    // S return values
    gint s_output_len;

    // For g_function_invoke call
    gint c_input_len;
    gint c_output_len;

    // If this arg map has invalid entries
    gboolean is_invalid;

    // An array of arg_map_entry
    GigArgMapEntry *pdata;
    gint len;

    GigArgMapEntry return_val;
};

GigArgMap *gig_amap_new(const gchar *name, GICallableInfo *function_info);
void gig_amap_free(GigArgMap *am);
void gig_amap_dump(const gchar *name, const GigArgMap *am);

void gig_amap_s_input_count(const GigArgMap *amap, gint *required, gint *optional);
GigArgMapEntry *gig_amap_get_input_entry_by_s(GigArgMap *am, gint spos);
GigArgMapEntry *gig_amap_get_output_entry_by_c(GigArgMap *am, gint cpos);
gboolean gig_amap_output_child_c(GigArgMap *am, gint c_output_pos,
                                 gint *cinvoke_output_array_size_index);
void gig_amap_c_count(const GigArgMap *am, gint *c_input_pos, gint *c_output_pos);
gboolean gig_amap_input_s_2_input_c(const GigArgMap *amap, gint s_input, gint *c_input);
gboolean gig_amap_input_s_2_output_c(const GigArgMap *amap, gint s_input, gint *c_output);
gboolean gig_amap_input_s_2_child_input_c(const GigArgMap *amap, gint s_input, gint *c_input);
gboolean gig_amap_input_s_2_child_output_c(const GigArgMap *amap, gint s_input, gint *c_output);
gboolean gig_amap_get_cinvoke_array_length_indices(const GigArgMap *am, gint s_input_pos,
                                                   gint *c_input_pos, gint *c_output_pos);
gboolean gig_amap_input_i2c(const GigArgMap *amap, gint i, gint *c);
gboolean gig_amap_input_i2s(const GigArgMap *amap, gint i, gint *s);
gboolean gig_amap_input_c2i(const GigArgMap *amap, gint c, gint *i);
gboolean gig_amap_input_s2i(const GigArgMap *amap, gint s, gint *i);
gboolean gig_amap_input_c2s(const GigArgMap *amap, gint c, gint *s);
gboolean gig_amap_input_s2c(const GigArgMap *amap, gint s, gint *c);

gboolean gig_amap_output_i2c(const GigArgMap *amap, gint i, gint *c);
gboolean gig_amap_output_i2s(const GigArgMap *amap, gint i, gint *s);
gboolean gig_amap_output_c2i(const GigArgMap *amap, gint c, gint *i);
gboolean gig_amap_output_s2i(const GigArgMap *amap, gint s, gint *i);
gboolean gig_amap_output_c2s(const GigArgMap *amap, gint c, gint *s);
gboolean gig_amap_output_s2c(const GigArgMap *amap, gint s, gint *c);

gboolean gig_amap_child_i(const GigArgMap *amap, gint i, gint *ichild);
gboolean gig_amap_return_child_i(const GigArgMap *amap, gint *ichild);

G_END_DECLS
#endif
