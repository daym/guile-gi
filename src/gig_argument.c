// Copyright (C) 2018, 2019 Michael L. Gran

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

#include <girepository.h>
#include <glib.h>
#include <libguile.h>
#include <math.h>
#include <stdint.h>
#include "gig_argument.h"
#include "gig_object.h"
#include "gi_type_tag.h"
#include "gig_callback.h"
#include "gig_type.h"
#include "gig_typelib.h"

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F
#endif


static void scm_to_c_immediate(S2C_ARG_DECL);
static void scm_to_c_immediate_pointer(S2C_ARG_DECL);
static void scm_to_c_interface(S2C_ARG_DECL);
static void scm_to_c_string(S2C_ARG_DECL);
static void scm_to_c_void_pointer(S2C_ARG_DECL);
static void scm_to_c_interface_pointer(S2C_ARG_DECL);
static void scm_to_c_array(S2C_ARG_DECL);
static void scm_to_c_native_array(S2C_ARG_DECL);
static void scm_to_c_native_immediate_array(S2C_ARG_DECL);
static void scm_to_c_native_string_array(S2C_ARG_DECL);
static void scm_to_c_native_interface_array(S2C_ARG_DECL);
static void scm_to_c_ptr_array(S2C_ARG_DECL);
static void scm_to_c_garray(S2C_ARG_DECL);
static void scm_to_c_byte_array(S2C_ARG_DECL);
static void c_immediate_to_scm(C2S_ARG_DECL);
static void c_interface_to_scm(C2S_ARG_DECL);
static void c_interface_pointer_to_scm(C2S_ARG_DECL);
static void c_string_to_scm(C2S_ARG_DECL);
static void c_void_pointer_to_scm(C2S_ARG_DECL);
static void c_array_to_scm(C2S_ARG_DECL);
static void c_byte_array_to_scm(C2S_ARG_DECL);
static void c_native_array_to_scm(C2S_ARG_DECL);
static void c_garray_to_scm(C2S_ARG_DECL);
static void c_list_to_scm(C2S_ARG_DECL);

static void describe_non_pointer_type(GString *desc, GITypeInfo *type_info);

static gsize
array_length(GigArgMapEntry *entry, GIArgument *arg)
{
    if (entry->array_is_zero_terminated) {
        gpointer array = arg->v_pointer;
        if (array == NULL)
            return 0;

        gsize length = 0;

        if (gi_type_tag_is_string(entry->item_type_tag)) {
            gchar **ptr = array;
            while (ptr[length] != NULL)
                length++;
            return length;
        }

        switch (entry->item_size) {
        case 0:
            g_assert_not_reached();
        case 1:
            return strlen(array);
        case 2:
        {
            gint16 *ptr = array;
            while (ptr++ != 0)
                length++;
            return length;
        }
        case 4:
        {
            gint32 *ptr = array;
            while (ptr++ != 0)
                length++;
            return length;
        }
        case 8:
        {
            gint64 *ptr = array;
            while (ptr++ != 0)
                length++;
            return length;
        }
        default:
        {
            gchar *ptr = array;
            gboolean non_null;
            length = -1;
            do {
                length++;
                non_null = FALSE;
                for (gsize i = 0; i <= entry->item_size; i++)
                    if (ptr + i != 0) {
                        non_null = TRUE;
                        break;
                    }
                ptr += entry->item_size;
            } while (non_null);

            return length;
        }
        }
    }
    else if (entry->array_fixed_size != GIG_ARRAY_SIZE_UNKNOWN)
        return entry->array_fixed_size;

    return GIG_ARRAY_SIZE_UNKNOWN;
}

//////////////////////////////////////////////////////////
// CONVERTING SCM OBJECTS TO GIARGUMENTS
//////////////////////////////////////////////////////////

// This is the main entry point of the conversion of SCM objects to
// GIArguments.
void
gig_argument_scm_to_c(S2C_ARG_DECL)
{
    g_assert_nonnull(must_free);
    *must_free = GIG_FREE_NONE;

    if (size)
        *size = 0;

    // SCM #f means either NULL or FALSE.  Here we handle NULL.
    if (entry->may_be_null && scm_is_false(object)) {
        arg->v_pointer = NULL;
        return;
    }

    if (!entry->is_ptr) {
        switch (entry->type_tag) {
        case GI_TYPE_TAG_BOOLEAN:
        case GI_TYPE_TAG_DOUBLE:
        case GI_TYPE_TAG_FLOAT:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
        case GI_TYPE_TAG_UINT8:
        case GI_TYPE_TAG_UNICHAR:
        case GI_TYPE_TAG_GTYPE:
            scm_to_c_immediate(S2C_ARGS);
            break;

        case GI_TYPE_TAG_INTERFACE:
            // The non-pointer interfaces are usually FLAGS, ENUM, and
            // CALLBACK only.  STRUCT and OBJECT interfaces are seldom
            // passed directly.
            scm_to_c_interface(S2C_ARGS);
            break;

        case GI_TYPE_TAG_VOID:
        case GI_TYPE_TAG_ARRAY:
        case GI_TYPE_TAG_UTF8:
        case GI_TYPE_TAG_FILENAME:
        case GI_TYPE_TAG_GHASH:
        case GI_TYPE_TAG_GLIST:
        case GI_TYPE_TAG_GSLIST:
        case GI_TYPE_TAG_ERROR:
        default:
            // These are C pointer types, so they should never occur in
            // the non-pointer form.
            g_assert_not_reached();
            break;
        }
    }
    else {
        switch (entry->type_tag) {
        case GI_TYPE_TAG_BOOLEAN:
        case GI_TYPE_TAG_DOUBLE:
        case GI_TYPE_TAG_FLOAT:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
        case GI_TYPE_TAG_UINT8:
        case GI_TYPE_TAG_UNICHAR:
            scm_to_c_immediate_pointer(S2C_ARGS);
            break;

        case GI_TYPE_TAG_UTF8:
        case GI_TYPE_TAG_FILENAME:
            scm_to_c_string(S2C_ARGS);
            break;

        case GI_TYPE_TAG_VOID:
            scm_to_c_void_pointer(S2C_ARGS);
            break;

        case GI_TYPE_TAG_GHASH:
            scm_misc_error(subr, "marshalling to GHash is not implemented: ~S",
                           scm_list_1(object));
            break;
        case GI_TYPE_TAG_GLIST:
            scm_misc_error(subr, "marshalling to GList is not implemented: ~S",
                           scm_list_1(object));
            break;
        case GI_TYPE_TAG_GSLIST:
            scm_misc_error(subr, "marshalling to GSList is not implemented: ~S",
                           scm_list_1(object));
            break;

        case GI_TYPE_TAG_INTERFACE:
            scm_to_c_interface_pointer(S2C_ARGS);
            break;

        case GI_TYPE_TAG_GTYPE:
            // No GType pointer inputs as far as I can tell.
            scm_misc_error(subr,
                           "marshalling to GType pointer is not implemented: ~S",
                           scm_list_1(object));
            break;

        case GI_TYPE_TAG_ERROR:
            scm_misc_error(subr,
                           "marshalling to GError pointer is not implemented: ~S",
                           scm_list_1(object));
            break;

        case GI_TYPE_TAG_ARRAY:
        {
            scm_to_c_array(S2C_ARGS);
            break;
        }

        default:
            scm_misc_error(subr,
                           "marshalling to unknown C type is not implemented: ~S",
                           scm_list_1(object));
        }
    }
}

static void
describe_non_pointer_type(GString *desc, GITypeInfo *type_info)
{
    GITypeTag type_tag = g_type_info_get_tag(type_info);
    switch (type_tag) {
    case GI_TYPE_TAG_BOOLEAN:
        g_string_append(desc, "boolean");
        break;
    case GI_TYPE_TAG_DOUBLE:
    case GI_TYPE_TAG_FLOAT:
        g_string_append_printf(desc, "real number of size %s",
                               g_type_tag_to_string(g_type_info_get_tag(type_info)));
        break;
    case GI_TYPE_TAG_INT16:
    case GI_TYPE_TAG_INT32:
    case GI_TYPE_TAG_INT64:
    case GI_TYPE_TAG_INT8:
    case GI_TYPE_TAG_UINT16:
    case GI_TYPE_TAG_UINT32:
    case GI_TYPE_TAG_UINT64:
    case GI_TYPE_TAG_UINT8:
        g_string_append_printf(desc, "exact integer of size %s",
                               g_type_tag_to_string(g_type_info_get_tag(type_info)));
        break;
    case GI_TYPE_TAG_UNICHAR:
        g_string_append_printf(desc, "character");
        break;
    case GI_TYPE_TAG_GTYPE:
        g_string_append_printf(desc, "<GType>");
        break;

    case GI_TYPE_TAG_VOID:
    case GI_TYPE_TAG_ARRAY:
    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
    case GI_TYPE_TAG_GHASH:
    case GI_TYPE_TAG_GLIST:
    case GI_TYPE_TAG_GSLIST:
    case GI_TYPE_TAG_ERROR:
        //g_assert_not_reached();
        g_string_append_printf(desc, "Unhandled argument type tag %d in %s:%d", type_tag, __FILE__,
                               __LINE__);
        break;

    case GI_TYPE_TAG_INTERFACE:
    {
        GIBaseInfo *referenced_base_info = g_type_info_get_interface(type_info);
        GIInfoType referenced_base_type = g_base_info_get_type(referenced_base_info);
        if (referenced_base_type == GI_INFO_TYPE_ENUM)
            g_string_printf(desc, "exact integer of enum type %s",
                            g_base_info_get_name(referenced_base_info));
        else if (referenced_base_type == GI_INFO_TYPE_FLAGS)
            g_string_printf(desc, "exact integer of flags type %s",
                            g_base_info_get_name(referenced_base_info));
        else if (referenced_base_type == GI_INFO_TYPE_CALLBACK)
            g_string_printf(desc, "procedure of type %s",
                            g_base_info_get_name(referenced_base_info));
        else if (referenced_base_type == GI_INFO_TYPE_INTERFACE
                 || referenced_base_type == GI_INFO_TYPE_OBJECT
                 || referenced_base_type == GI_INFO_TYPE_STRUCT
                 || referenced_base_type == GI_INFO_TYPE_UNION) {
            GType type = g_registered_type_info_get_g_type(referenced_base_info);
            gchar *class_name = gig_type_class_name_from_gtype(type);
            g_string_append(desc, class_name);
            g_free(class_name);
        }
        else
            g_string_append_printf(desc, "Unhandled argument type tag %d in %s:%d", type_tag,
                                   __FILE__, __LINE__);
        g_base_info_unref(referenced_base_info);
        break;
    }
    default:
        g_assert_not_reached();
        break;
    }
}

gchar *
gig_argument_describe_arg(GIArgInfo *arg_info)
{
    GString *desc = g_string_new(NULL);
    GITypeInfo *type_info = g_arg_info_get_type(arg_info);
    GITypeTag type_tag = g_type_info_get_tag(type_info);
    gboolean is_ptr = g_type_info_is_pointer(type_info);

    if (g_arg_info_may_be_null(arg_info))
        g_string_append(desc, "#f for NULL or ");

    if (!is_ptr) {
        describe_non_pointer_type(desc, type_info);
    }
    else {
        switch (type_tag) {
        case GI_TYPE_TAG_BOOLEAN:
        case GI_TYPE_TAG_UNICHAR:
            g_string_append_printf(desc, "Unhandled argument type tag %d in %s:%d", type_tag,
                                   __FILE__, __LINE__);

            break;
        case GI_TYPE_TAG_DOUBLE:
        case GI_TYPE_TAG_FLOAT:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
        case GI_TYPE_TAG_UINT8:
            g_string_append_printf(desc, "bytevector containing elements %s",
                                   g_type_tag_to_string(type_tag));
            break;

        case GI_TYPE_TAG_UTF8:
            g_string_append(desc, "string");
            break;
        case GI_TYPE_TAG_FILENAME:
            g_string_append(desc, "locale string");
            break;

        case GI_TYPE_TAG_VOID:
            g_string_append(desc, "pointer");
            break;

        case GI_TYPE_TAG_GHASH:
            g_string_append(desc, "<GHash>");
            break;
        case GI_TYPE_TAG_GLIST:
            g_string_append(desc, "<GList>");
            break;
        case GI_TYPE_TAG_GSLIST:
            g_string_append(desc, "<GSList>");
            break;

        case GI_TYPE_TAG_INTERFACE:
        {
            GIBaseInfo *referenced_base_info = g_type_info_get_interface(type_info);
            GIInfoType referenced_base_type = g_base_info_get_type(referenced_base_info);
            if (referenced_base_type == GI_INFO_TYPE_ENUM ||
                referenced_base_type == GI_INFO_TYPE_FLAGS)
                g_string_printf(desc, "bytevector containing %s",
                                g_base_info_get_name(referenced_base_info));
            else if (referenced_base_type == GI_INFO_TYPE_CALLBACK)
                g_string_printf(desc, "list of procedures of type %s",
                                g_base_info_get_name(referenced_base_info));
            else if (referenced_base_type == GI_INFO_TYPE_STRUCT
                     || referenced_base_type == GI_INFO_TYPE_UNION
                     || referenced_base_type == GI_INFO_TYPE_OBJECT
                     || referenced_base_type == GI_INFO_TYPE_INTERFACE) {
                GType type = g_registered_type_info_get_g_type(referenced_base_info);
                gchar *class_name = gig_type_class_name_from_gtype(type);
                g_string_append(desc, class_name);
                g_free(class_name);
            }
            else
                g_string_append_printf(desc, "Unhandled argument type tag %d in %s:%d", type_tag,
                                       __FILE__, __LINE__);
            g_base_info_unref(referenced_base_info);
            break;
        }

        case GI_TYPE_TAG_GTYPE:
            g_string_append(desc, "GType");
            break;

        case GI_TYPE_TAG_ERROR:
            g_string_append(desc, "<GError>");
            break;

        case GI_TYPE_TAG_ARRAY:
        {
            GIArrayType array_type = g_type_info_get_array_type(type_info);
            if (array_type == GI_ARRAY_TYPE_BYTE_ARRAY)
                g_string_append_printf(desc, "A bytevector containing unspecified binary data");
            else if (array_type == GI_ARRAY_TYPE_C) {
                GITypeInfo *item_type_info = g_type_info_get_param_type(type_info, 0);
                GITypeTag item_type_tag = g_type_info_get_tag(item_type_info);
                if (item_type_tag == GI_TYPE_TAG_INTERFACE) {
                    GIBaseInfo *referenced_base_info = g_type_info_get_interface(item_type_info);
                    GIInfoType referenced_base_type = g_base_info_get_type(referenced_base_info);
                    referenced_base_info = g_type_info_get_interface(item_type_info);
                    referenced_base_type = g_base_info_get_type(referenced_base_info);
                    if (referenced_base_type == GI_INFO_TYPE_ENUM)
                        g_string_append_printf(desc,
                                               "A list of enum values of type %s as integers",
                                               g_base_info_get_name(referenced_base_info));
                    else if (referenced_base_type == GI_INFO_TYPE_FLAGS)
                        g_string_append_printf(desc,
                                               "A list of flag values of type %s as integers",
                                               g_base_info_get_name(referenced_base_info));
                    else if (referenced_base_type == GI_INFO_TYPE_STRUCT
                             || referenced_base_type == GI_INFO_TYPE_UNION
                             || referenced_base_type == GI_INFO_TYPE_OBJECT) {
                        GType type = g_registered_type_info_get_g_type(referenced_base_info);
                        gchar *class_name = gig_type_class_name_from_gtype(type);
                        g_string_append_printf(desc, "A list of %s", class_name);
                        g_free(class_name);
                    }
                }
                else if (item_type_tag == GI_TYPE_TAG_BOOLEAN)
                    g_string_append_printf(desc, "A list of booleans");
                else if (item_type_tag == GI_TYPE_TAG_INT8)
                    g_string_append_printf(desc, "A bytevector containing int8");
                else if (item_type_tag == GI_TYPE_TAG_UINT8)
                    g_string_append_printf(desc, "A bytevector containing uint8");
                else if (item_type_tag == GI_TYPE_TAG_INT16)
                    g_string_append_printf(desc, "A bytevector containing int16");
                else if (item_type_tag == GI_TYPE_TAG_UINT16)
                    g_string_append_printf(desc, "A bytevector containing uint16");
                else if (item_type_tag == GI_TYPE_TAG_INT32)
                    g_string_append_printf(desc, "A bytevector containing int32");
                else if (item_type_tag == GI_TYPE_TAG_UINT32)
                    g_string_append_printf(desc, "A bytevector containing uint32");
                else if (item_type_tag == GI_TYPE_TAG_INT64)
                    g_string_append_printf(desc, "A bytevector containing int64");
                else if (item_type_tag == GI_TYPE_TAG_UINT64)
                    g_string_append_printf(desc, "A bytevector containing uint64");
                else if (item_type_tag == GI_TYPE_TAG_FLOAT)
                    g_string_append_printf(desc,
                                           "A bytevector containing %d-byte floating-point numbers",
                                           (int)sizeof(float));
                else if (item_type_tag == GI_TYPE_TAG_DOUBLE)
                    g_string_append_printf(desc,
                                           "A bytevector containing %d-byte floating-point numbers",
                                           (int)sizeof(double));
                else if (gi_type_tag_is_string(item_type_tag))
                    g_string_append_printf(desc, "A list of strings");
                else if (item_type_tag == GI_TYPE_TAG_GTYPE)
                    g_string_append_printf(desc, "A list of <GType>");
                else {
                    g_string_append_printf(desc,
                                           "A bytevector containing some unknown type tag %d",
                                           item_type_tag);
                    g_critical("Unhandled array entry type in %s:%d", __FILE__, __LINE__);
                }
            }
            break;
        }

        default:
            g_assert_not_reached();
        }
    }
    g_base_info_unref(type_info);

    return g_string_free(desc, FALSE);
}


static void
scm_to_c_immediate(S2C_ARG_DECL)
{
    switch (entry->type_tag) {
    case GI_TYPE_TAG_INT8:
        if (!scm_is_signed_integer(object, INT8_MIN, INT8_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "int8");
        arg->v_int8 = scm_to_int8(object);
        break;
    case GI_TYPE_TAG_UINT8:
        if (!scm_is_unsigned_integer(object, 0, UINT8_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "uint8");
        arg->v_uint8 = scm_to_uint8(object);
        break;
    case GI_TYPE_TAG_INT16:
        if (!scm_is_signed_integer(object, INT16_MIN, INT16_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "int16");
        arg->v_int16 = scm_to_int16(object);
        break;
    case GI_TYPE_TAG_UINT16:
        if (!scm_is_unsigned_integer(object, 0, UINT16_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "uint16");
        arg->v_uint16 = scm_to_uint16(object);
        break;
    case GI_TYPE_TAG_INT32:
        if (!scm_is_signed_integer(object, INT32_MIN, INT32_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "int32");
        arg->v_int32 = scm_to_int32(object);
        break;
    case GI_TYPE_TAG_UINT32:
        if (!scm_is_unsigned_integer(object, 0, UINT32_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "uint32");
        arg->v_uint32 = scm_to_uint32(object);
        break;
    case GI_TYPE_TAG_INT64:
        if (!scm_is_signed_integer(object, INT64_MIN, INT64_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "int64");
        arg->v_int64 = scm_to_int64(object);
        break;
    case GI_TYPE_TAG_UINT64:
        if (!scm_is_unsigned_integer(object, 0, UINT64_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "uint64");
        arg->v_uint64 = scm_to_uint64(object);
        break;
    case GI_TYPE_TAG_BOOLEAN:
        arg->v_boolean = scm_is_true(object);
        break;
    case GI_TYPE_TAG_FLOAT:
        if (!scm_is_real(object))
            scm_wrong_type_arg_msg(subr, argpos, object, "float32");
        gdouble dtmp = scm_to_double(object);
        if (dtmp > FLT_MAX || dtmp < -FLT_MAX)
            scm_out_of_range(subr, object);
        arg->v_float = (float)dtmp;
        break;
    case GI_TYPE_TAG_DOUBLE:
        if (!scm_is_real(object))
            scm_wrong_type_arg_msg(subr, argpos, object, "float64");
        arg->v_double = scm_to_double(object);
        break;
    case GI_TYPE_TAG_GTYPE:
        if (!scm_is_unsigned_integer(object, 0, UINT64_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "GType integer");
        arg->v_size = scm_to_size_t(object);
        break;
    case GI_TYPE_TAG_UNICHAR:
        if (SCM_CHARP(object))
            arg->v_uint32 = SCM_CHAR(object);
        else if (scm_is_unsigned_integer(object, 0, SCM_CODEPOINT_MAX))
            arg->v_uint32 = scm_to_uint32(object);
        else {
            scm_wrong_type_arg_msg(subr, argpos, object, "char");
        }

        break;
    default:
        // Should never get here.
        g_assert_not_reached();
    }
}

// Handle SCM conversion to non-pointer objects that aren't simple C
// types.
static void
scm_to_c_interface(S2C_ARG_DECL)
{
    g_assert(entry->type_tag == GI_TYPE_TAG_INTERFACE);

    GIBaseInfo *referenced_base_info = g_type_info_get_interface(entry->type_info);
    GIInfoType referenced_base_type = g_base_info_get_type(referenced_base_info);

    if (referenced_base_type == GI_INFO_TYPE_ENUM)
        arg->v_int = scm_to_int(object);
    else if (referenced_base_type == GI_INFO_TYPE_FLAGS)
        arg->v_uint = scm_to_uint(object);
    else if (referenced_base_type == GI_INFO_TYPE_CALLBACK) {
        GICallbackInfo *callback_info = referenced_base_info;
        if (scm_is_true(scm_procedure_p(object))) {
            gint arity = scm_to_int(scm_car(scm_procedure_minimum_arity(object)));
            gint n_args = g_callable_info_get_n_args(callback_info);
            if (arity == n_args) {
                arg->v_pointer = gig_callback_get_ptr(callback_info, object);
                g_assert(arg->v_pointer != NULL);
            }
            else {
                const gchar *msg = "a procedure requiring %d arguments";
                gchar str[strlen(msg) + 20];
                snprintf(str, sizeof(str), msg, n_args);
                scm_wrong_type_arg_msg(subr, argpos, object, str);
            }
        }
    }
    else if ((referenced_base_type == GI_INFO_TYPE_STRUCT)
             || (referenced_base_type == GI_INFO_TYPE_UNION)
             || (referenced_base_type == GI_INFO_TYPE_OBJECT)) {
        // This is uncommon case where a struct is used directly,
        // and not as a pointer, such as in gtk_text_buffer_get_bounds.

        arg->v_pointer = gig_type_peek_object(object);
    }
    else
        g_assert_not_reached();
}

static void
scm_to_c_immediate_pointer(S2C_ARG_DECL)
{
    // Here we handle the uncommon case of converting an SCM to a
    // pointer to a simple C type like 'int *', but not objects or
    // arrays, which are handled elsewhere.

    // We'll require an input of bytevectors, since they can apply to
    // most cases, and this case is underspecified anyway.
    if (!scm_is_bytevector(object)) {
        scm_wrong_type_arg_msg(subr, argpos, object, "a bytevector");
    }
    else {
        // FIXME: add bytevector minimum length checks.
        if (entry->transfer == GI_TRANSFER_EVERYTHING)
            arg->v_pointer =
                g_memdup(SCM_BYTEVECTOR_CONTENTS(object), SCM_BYTEVECTOR_LENGTH(object));
        else
            arg->v_pointer = SCM_BYTEVECTOR_CONTENTS(object);
    }
}

static void
scm_to_c_string(S2C_ARG_DECL)
{
    // Here we convert an input string into an argument.  The input
    // string is either UTF8 or locale encoded.
    if (scm_is_bytevector(object)) {
        // Some methods expect passed-in strings to be overwriteable,
        // like g_date_strftime(char *s, ...) expects 's' to be a
        // place to store an output.  Since Glib strings and Guile
        // strings have no encoding in common, we can use
        // bytevectors...
        if (entry->transfer == GI_TRANSFER_NOTHING) {
            // But when we're using bytevectors as a possibly writable
            // location, they do need to be null terminated.
            gboolean terminated = FALSE;
            for (gsize i = 0; i < SCM_BYTEVECTOR_LENGTH(object); i++)
                if (SCM_BYTEVECTOR_CONTENTS(object)[i] == 0)
                    terminated = TRUE;
            if (!terminated)
                scm_wrong_type_arg_msg(subr, argpos, object, "null-terminated bytevector");
            arg->v_string = (char *)SCM_BYTEVECTOR_CONTENTS(object);
        }
        else
            // But when we're copying the contents of the string, the
            // null termination can be enforced here.
            arg->v_string = g_strndup((const gchar *)SCM_BYTEVECTOR_CONTENTS(object),
                                      SCM_BYTEVECTOR_LENGTH(object));
    }
    else if (scm_is_string(object)) {
        if (entry->type_tag == GI_TYPE_TAG_FILENAME)
            arg->v_string = scm_to_locale_string(object);
        else
            arg->v_string = scm_to_utf8_string(object);

        // The scm_to_..._string always makes a new copy, so if
        // transfer isn't EVERYTHING, we'll have to free the string
        // later.
        if (entry->transfer == GI_TRANSFER_NOTHING)
            *must_free = GIG_FREE_SIMPLE;
    }
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "string or bytevector");
}

static void
scm_to_c_void_pointer(S2C_ARG_DECL)
{
    if (SCM_POINTER_P(object))
        arg->v_pointer = scm_to_pointer(object);
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "pointer");
}

static void
scm_to_c_interface_pointer(S2C_ARG_DECL)
{
    // Usually STRUCT, UNION, INTERFACE, OBJECT.  Handle NULL_OK
    g_assert_cmpint(entry->type_tag, ==, GI_TYPE_TAG_INTERFACE);
    GIBaseInfo *referenced_base_info = g_type_info_get_interface(entry->type_info);
    GIInfoType referenced_base_type = g_base_info_get_type(referenced_base_info);

    GType obj_type = gig_type_get_gtype_from_obj(object);
    if (obj_type == G_TYPE_NONE || obj_type == G_TYPE_INVALID)
        scm_wrong_type_arg_msg(subr, argpos, object,
                               "a GObject struct, union, interface, or object");

    GType arg_type = g_registered_type_info_get_g_type(referenced_base_info);
    if (!g_type_is_a(obj_type, arg_type)) {
        gchar msg[80];
        snprintf(msg, 80, "a GObject that is a type of %s", g_type_name(arg_type));
        scm_wrong_type_arg_msg(subr, argpos, object, msg);
    }
    else if ((referenced_base_type == GI_INFO_TYPE_STRUCT)
             || (referenced_base_type == GI_INFO_TYPE_UNION)
             || (referenced_base_type == GI_INFO_TYPE_OBJECT)) {
        arg->v_pointer = gig_type_peek_object(object);
    }
    else if (referenced_base_type == GI_INFO_TYPE_CALLBACK) {
        scm_misc_error(subr,
                       "Marshalling to C callback pointer args is unimplemented: ~S",
                       scm_list_1(object));
    }
    else if (referenced_base_type == GI_INFO_TYPE_INTERFACE) {
        scm_misc_error(subr,
                       "Marshalling to C interface pointer args is unimplemented: ~S",
                       scm_list_1(object));
    }
    g_base_info_unref(referenced_base_info);
}


static void
scm_to_c_array(S2C_ARG_DECL)
{
    switch (entry->array_type) {
    case GI_ARRAY_TYPE_C:
        scm_to_c_native_array(S2C_ARGS);
        break;
    case GI_ARRAY_TYPE_ARRAY:
        scm_to_c_garray(S2C_ARGS);
        break;
    case GI_ARRAY_TYPE_BYTE_ARRAY:
        scm_to_c_byte_array(S2C_ARGS);
        break;
    case GI_ARRAY_TYPE_PTR_ARRAY:
        scm_to_c_ptr_array(S2C_ARGS);
        break;
    default:
        g_assert_not_reached();
        break;
    }
}

static void
scm_to_c_native_array(S2C_ARG_DECL)
{
    if (gi_type_tag_is_integer(entry->item_type_tag)
        || gi_type_tag_is_real_number(entry->item_type_tag))
        scm_to_c_native_immediate_array(S2C_ARGS);
    else if (gi_type_tag_is_string(entry->item_type_tag))
        scm_to_c_native_string_array(S2C_ARGS);
    else if (entry->item_type_tag == GI_TYPE_TAG_INTERFACE) {
        scm_to_c_native_interface_array(S2C_ARGS);
    }
    else {
        scm_misc_error(subr, "Unhandled array type", SCM_EOL);
    }
}

static void
scm_to_c_native_immediate_array(S2C_ARG_DECL)
{
#define FUNC_NAME "%object->c-native-immediate-array-arg"
    // IMMEDIATE TYPES.  It seems only boolean, double, and 8 and
    // 32-bit integer arrays are ever used. Sometimes deep copy.
    // Sometimes zero terminated.  For SCM bytevectors and
    // GI_TRANSFER_NOTHING and not zero-terminated, we can use the
    // contents of a bytevector directly.  For SCM bytevectors and
    // GI_TRANSFER_EVERYTHING, we need to make a deep copy.  If the
    // argument is NULL_OK and the SCM is #f, we pass NULL.

    g_assert_cmpint(entry->item_size, !=, 0);

    if (scm_is_bytevector(object)) {
        *size = SCM_BYTEVECTOR_LENGTH(object);
        if (entry->item_transfer == GI_TRANSFER_NOTHING) {
            if (!entry->array_is_zero_terminated) {
                // The fast path
                arg->v_pointer = SCM_BYTEVECTOR_CONTENTS(object);
            }
            else {
                gsize len = SCM_BYTEVECTOR_LENGTH(object);
                // Adding null terminator element.
                arg->v_pointer = g_malloc0(len + entry->item_size);
                memcpy(arg->v_pointer, SCM_BYTEVECTOR_CONTENTS(object), len);
                *must_free = GIG_FREE_SIMPLE;
            }
        }
        else if (entry->item_transfer == GI_TRANSFER_EVERYTHING) {
            if (!entry->array_is_zero_terminated) {
                arg->v_pointer = g_memdup(SCM_BYTEVECTOR_CONTENTS(object),
                                          SCM_BYTEVECTOR_LENGTH(object));
            }
            else {
                gsize len = SCM_BYTEVECTOR_LENGTH(object);
                // Note, null terminated here.
                arg->v_pointer = g_malloc0(len + entry->item_size);
                memcpy(arg->v_pointer, SCM_BYTEVECTOR_CONTENTS(object), len);
            }
        }
    }
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "bytevector");
#undef FUNC_NAME
}

static void
scm_to_c_byte_array(S2C_ARG_DECL)
{
    if (scm_is_bytevector(object)) {
        gpointer contents = SCM_BYTEVECTOR_CONTENTS(object);
        gsize len = SCM_BYTEVECTOR_LENGTH(object);
        *size = len;
        if (entry->transfer == GI_TRANSFER_EVERYTHING)
            arg->v_pointer = g_byte_array_new_take(contents, len);
        else
            arg->v_pointer = g_byte_array_new_take(g_memdup(contents, len), len);
    }
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "bytevector");
}

static void
scm_to_c_garray(S2C_ARG_DECL)
{
#define FUNC_NAME "%object->c-garray-array-arg"
    scm_misc_error(FUNC_NAME,
                   "Marshalling to C GArray pointer args is unimplemented: ~S",
                   scm_list_1(object));
#undef FUNC_NAME
}

static void
scm_to_c_ptr_array(S2C_ARG_DECL)
{
#define FUNC_NAME "%object->c-ptr-array-arg"
    scm_misc_error(FUNC_NAME,
                   "Marshalling to C ptr array pointer args is unimplemented: ~S",
                   scm_list_1(object));
#undef FUNC_NAME
}

static void
scm_to_c_native_direct_struct_array(S2C_ARG_DECL)
{
    // This is the uncommon case of the argument containing
    // an array of structs themselves, rather than an array
    // of pointers to structs.  These may be null terminated.
    // For example, gtk_tree_view_enable_model_drag_dest
    gsize len = scm_to_size_t(scm_length(object));
    gpointer ptr;
    if (entry->array_is_zero_terminated)
        ptr = g_malloc0_n(entry->item_size, len + 1);
    else
        ptr = g_malloc0_n(entry->item_size, len);
    gpointer entry_ptr = gig_type_peek_object(object);
    for (gsize i = 0; i < len; i++)
        memcpy((char *)ptr + i * entry->item_size, entry_ptr, entry->item_size);
    if (entry->item_transfer == GI_TRANSFER_NOTHING)
        *must_free = GIG_FREE_SIMPLE;
}

static void
scm_to_c_native_indirect_object_array(S2C_ARG_DECL)
{
    // Arrays of pointers to OBJECTS.  The only example I could find
    // is g_socket_send_message.
    if ((entry->item_type_tag == GI_TYPE_TAG_INTERFACE)
        && (entry->referenced_base_type == G_TYPE_OBJECT)
        && entry->item_is_ptr) {
        // On the Scheme side, an array of pointers to objects will be
        // a list of GObjects.
        gsize len = scm_to_size_t(scm_length(object));
        gpointer *ptr;
        if (entry->array_is_zero_terminated)
            ptr = g_malloc0_n(sizeof(gpointer), len + 1);
        else
            ptr = g_malloc0_n(sizeof(gpointer), len);
        for (gsize i = 0; i < len; i++) {
            SCM elt = scm_list_ref(object, scm_from_size_t(i));
            // Entry should be a GObject.  I guess we're not
            // increasing refcnt?  At least that is the case for
            // g_socket_send_message.
            ptr[i] = gig_type_peek_object(elt);
        }
        if (entry->item_transfer == GI_TRANSFER_NOTHING) {
            if (entry->array_is_zero_terminated)
                *must_free = GIG_FREE_STRV;
            else
                *must_free = GIG_FREE_PTR_ARRAY | len;
        }
    }
}

static void
scm_to_c_native_interface_array(S2C_ARG_DECL)
{
#define FUNC_NAME "%object->c-native-interface-array-arg"
    if ((entry->referenced_base_type == GI_INFO_TYPE_ENUM)
        || (entry->referenced_base_type == GI_INFO_TYPE_FLAGS)) {
        // We haven't bothered to make a special flag or enum
        // class on the Scheme side of things.  On the scheme
        // side, enums and flags are just variables holding
        // integers.
        scm_to_c_native_immediate_array(S2C_ARGS);
    }
    else if ((entry->referenced_base_type == GI_INFO_TYPE_STRUCT)
             || (entry->referenced_base_type == GI_INFO_TYPE_UNION)
             || (entry->referenced_base_type == GI_INFO_TYPE_OBJECT)) {
        // If we are a Struct or Object, we need to look up
        // our actual GType.
        g_assert(entry->referenced_object_type != G_TYPE_NONE);

#if 1
        g_assert_not_reached();
#else
        if (!entry->item_is_ptr && entry->referenced_base_type == GI_INFO_TYPE_STRUCT)
            object_to_c_native_direct_struct_array_arg(subr, argpos, object, ai, arg);
        else if (entry->item_is_ptr && entry->referenced_base_type == GI_INFO_TYPE_STRUCT)
            object_to_c_native_indirect_struct_array_arg(subr, argpos, object, ai, arg);
        else if (!entry->item_is_ptr && entry->referenced_base_type == GI_INFO_TYPE_UNION)
            object_to_c_native_direct_union_array_arg(subr, argpos, object, ai, arg);
        else if (entry->item_is_ptr && entry->referenced_base_type == GI_INFO_TYPE_UNION)
            object_to_c_native_indirect_union_array_arg(subr, argpos, object, ai, arg);
        else if (!entry->item_is_ptr && entry->referenced_base_type == G_TYPE_OBJECT)
            // Arrays of OBJECTS. Direct object arrays, holding the
            // complete structures themselves.  The only example is
            // the 'additions' argument of g_list_store_splice.
            object_to_c_native_direct_object_array_arg(subr, argpos, object, ai, arg);
        else if (entry->item_is_ptr && entry->referenced_base_type == G_TYPE_OBJECT)
            object_to_c_native_indirect_object_array_arg(subr, argpos, object, ai, arg);
#endif
    }
    else {
        // Everything else is unhandled.
        g_critical("Unhandled argument type, %s: %d", __FILE__, __LINE__);
    }
#undef FUNC_NAME
}

static void
scm_to_c_native_string_array(S2C_ARG_DECL)
{
    // UTF8 or FILENAME pointers.  It seems that arrays of type UTF8
    // can mean two things.
    // 1. If zero-terminated, it means a NULL-pointer-terminated list
    //    of gchar pointers to UTF8 strings.
    // 2. If not zero-terminated, it could mean a non-zero-terminated
    //    UTF8 string, but, only for GLib regex functions, which seems
    //    like a mistake.
    //    Also, if not zero-terminated, it could be an argv list of
    //    strings. In that case, it seems OK to do the same thing as
    //    case #1.

    // We're adding a zero termination despite the value of
    // array_is_zero_terminated, because it does no harm.
    if (scm_is_true(scm_list_p(object))) {
        gsize len = scm_to_size_t(scm_length(object));
        *size = len;
        gchar **strv = g_new0(gchar *, len + 1);
        SCM iter = object;
        for (gsize i = 0; i < len; i++) {
            SCM elt = scm_car(iter);
            if (entry->item_type_tag == GI_TYPE_TAG_FILENAME)
                strv[i] = scm_to_locale_string(elt);
            else
                strv[i] = scm_to_utf8_string(elt);
            iter = scm_cdr(iter);
        }
        strv[len] = NULL;
        arg->v_pointer = strv;
        if (entry->item_transfer == GI_TRANSFER_NOTHING)
            *must_free = GIG_FREE_STRV;
    }
    else if (scm_is_vector(object)) {
        scm_t_array_handle handle;
        gsize len;
        gssize inc;
        const SCM *elt;

        elt = scm_vector_elements(object, &handle, &len, &inc);
        *size = len;
        gchar **strv = g_new0(gchar *, len + 1);

        for (gsize i = 0; i < len; i++, elt += inc) {
            if (entry->item_type_tag == GI_TYPE_TAG_FILENAME)
                strv[i] = scm_to_locale_string(*elt);
            else
                strv[i] = scm_to_utf8_string(*elt);
        }
        strv[len] = NULL;
        arg->v_pointer = strv;

        if (entry->item_transfer == GI_TRANSFER_NOTHING)
            *must_free = GIG_FREE_STRV;

        scm_array_handle_release(&handle);
    }
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "list or vector of strings");
}

void
gig_argument_free_args(gint n, guint *must_free, GIArgument *args)
{
    for (gint i = 0; i < n; i++) {
        if (must_free[i] == GIG_FREE_SIMPLE)
            g_free(args[i].v_pointer);
        else if (must_free[i] == GIG_FREE_STRV) {
            gint j = 0;
            while (((char **)(args[i].v_pointer))[j] != NULL) {
                g_free(((char **)(args[i].v_pointer))[j]);
                j++;
            }
            g_free(args[i].v_pointer);
        }
        else if (must_free[i] & GIG_FREE_PTR_ARRAY) {
            gint count = GIG_FREE_PTR_COUNT(must_free[i]);
            for (gint j = 0; j < count; j++)
                g_free(((char **)(args[i].v_pointer))[j]);
            g_free(args[i].v_pointer);
        }
    }
}

//////////////////////////////////////////////////////////
// CONVERTING GIARGUMENTS TO SCM OBJECTS
//////////////////////////////////////////////////////////

void
gig_argument_preallocate_output_arg_and_object(GIArgInfo *arg_info, GIArgument *arg, SCM *object)
{
    if (!g_arg_info_is_caller_allocates(arg_info))
        return;

    GITypeInfo *type_info = g_arg_info_get_type(arg_info);
    GITypeTag type_tag = g_type_info_get_tag(type_info);
    // gboolean alloc = g_arg_info_is_caller_allocates(arg_info);
    gboolean is_ptr = g_type_info_is_pointer(type_info);

    g_base_info_unref(type_info);
    if (!is_ptr) {
        switch (type_tag) {
        case GI_TYPE_TAG_BOOLEAN:
        case GI_TYPE_TAG_DOUBLE:
        case GI_TYPE_TAG_FLOAT:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
        case GI_TYPE_TAG_UINT8:
        case GI_TYPE_TAG_UNICHAR:
        case GI_TYPE_TAG_GTYPE:
            // Uniquely, the GLib unicode procedures have the
            // ALLOC flag set on some 'items_read' and
            // 'items-written' outputs.  I think this is an error.
            // So what to do about that?  Try ignoring it?
            g_critical("Ignoring request to allocate output argument for immediate type");
            break;

        case GI_TYPE_TAG_VOID:
        case GI_TYPE_TAG_ARRAY:
        case GI_TYPE_TAG_UTF8:
        case GI_TYPE_TAG_FILENAME:
        case GI_TYPE_TAG_GHASH:
        case GI_TYPE_TAG_GLIST:
        case GI_TYPE_TAG_GSLIST:
        case GI_TYPE_TAG_ERROR:
            g_assert_not_reached();
            break;

        case GI_TYPE_TAG_INTERFACE:
        {
            GIBaseInfo *referenced_base_info = g_type_info_get_interface(type_info);
            GIInfoType referenced_base_type = g_base_info_get_type(referenced_base_info);
            // GType referenced_object_type = g_registered_type_info_get_g_type(referenced_base_info);

            if (referenced_base_type == GI_INFO_TYPE_STRUCT) {
                // If OBJ is already set, we typecheck that it is
                // a box holding a pointer for a struct of the
                // right type.  If it isn't set, we allocate a new
                // box.
                if (!scm_is_eq(*object, SCM_BOOL_F)) {
                    gsize item_size = g_struct_info_get_size(referenced_base_info);
                    arg->v_pointer = g_malloc0(item_size);
                    g_critical("unhandled allocation");
                    g_assert_not_reached();
                    //*obj = gig_new_struct_gbox(referenced_object_type, arg->v_pointer, TRUE);
                }
                else
                    g_assert_not_reached();
                //arg->v_pointer = gi_gbox_ref_pointer(*obj);
            }
            else
                g_assert_not_reached();
        }
            break;
        default:
            g_assert_not_reached();
            break;
        }
    }
    else {
        switch (type_tag) {
        case GI_TYPE_TAG_BOOLEAN:
        case GI_TYPE_TAG_DOUBLE:
        case GI_TYPE_TAG_FLOAT:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
        case GI_TYPE_TAG_UINT8:
        case GI_TYPE_TAG_UNICHAR:
        case GI_TYPE_TAG_UTF8:
        case GI_TYPE_TAG_FILENAME:
            // OK. Function expects pre-allocated C array to copy
            // data into.  But, from here, it is difficult to know
            // how big that array was supposed to have been.  So
            // we require that obj already be a bytevector.

            // Uniquely, g_unichar_to_utf8 requires a pre-allocated UTF8
            // output buffer.
            if (scm_is_bytevector(*object)) {
                // FIXME: we're just hoping that this
                // bytevector is big enough to hold the contents
                // that are going to be copied into it.
                arg->v_pointer = SCM_BYTEVECTOR_CONTENTS(*object);
            }
            break;

        case GI_TYPE_TAG_VOID:
        case GI_TYPE_TAG_GHASH:
        case GI_TYPE_TAG_GLIST:
        case GI_TYPE_TAG_GSLIST:
            g_assert_not_reached();
            break;

        case GI_TYPE_TAG_INTERFACE:
            g_assert_not_reached();
            break;

        case GI_TYPE_TAG_GTYPE:
        case GI_TYPE_TAG_ERROR:
            g_assert_not_reached();
            break;

        case GI_TYPE_TAG_ARRAY:
            // FIXME: uniquely, g_main_context_query requires a
            // pre-allocated array of PollFD structs.
            g_assert_not_reached();
            break;

        default:
            g_assert_not_reached();
        }
    }
}

void
gig_argument_c_to_scm(C2S_ARG_DECL)
{
    if (!entry->is_ptr) {
        switch (entry->type_tag) {
        case GI_TYPE_TAG_VOID:
            *object = SCM_UNSPECIFIED;
            break;

        case GI_TYPE_TAG_BOOLEAN:
        case GI_TYPE_TAG_DOUBLE:
        case GI_TYPE_TAG_FLOAT:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
        case GI_TYPE_TAG_UINT8:
        case GI_TYPE_TAG_UNICHAR:
        case GI_TYPE_TAG_GTYPE:
            c_immediate_to_scm(C2S_ARGS);
            break;

        case GI_TYPE_TAG_ARRAY:
        case GI_TYPE_TAG_UTF8:
        case GI_TYPE_TAG_FILENAME:
        case GI_TYPE_TAG_GHASH:
        case GI_TYPE_TAG_GLIST:
        case GI_TYPE_TAG_GSLIST:
        case GI_TYPE_TAG_ERROR:
            g_assert_not_reached();
            break;

        case GI_TYPE_TAG_INTERFACE:
            c_interface_to_scm(C2S_ARGS);
            break;
        default:
            g_assert_not_reached();
            break;
        }
    }
    else if (entry->may_be_null && arg->v_pointer == NULL) {
        *object = SCM_BOOL_F;
    }
    else {
        switch (entry->type_tag) {
        case GI_TYPE_TAG_BOOLEAN:
        case GI_TYPE_TAG_DOUBLE:
        case GI_TYPE_TAG_FLOAT:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
        case GI_TYPE_TAG_UINT8:
        case GI_TYPE_TAG_UNICHAR:
            g_critical("Unhandled argument type %s %d", __FILE__, __LINE__);
            // ret = convert_immediate_pointer_arg_to_object(object, arg_info, arg);
            break;

        case GI_TYPE_TAG_UTF8:
        case GI_TYPE_TAG_FILENAME:
            c_string_to_scm(C2S_ARGS);
            break;

        case GI_TYPE_TAG_VOID:
            c_void_pointer_to_scm(C2S_ARGS);
            break;

        case GI_TYPE_TAG_GHASH:
            // FIXME: unhandled
            g_critical("Unhandled hash argument type tag %d", entry->type_tag);
            g_assert_not_reached();
            break;

        case GI_TYPE_TAG_GLIST:
        case GI_TYPE_TAG_GSLIST:
            c_list_to_scm(C2S_ARGS);
            break;

        case GI_TYPE_TAG_INTERFACE:
            c_interface_pointer_to_scm(C2S_ARGS);
            break;

        case GI_TYPE_TAG_GTYPE:
            g_critical("Unhandled argument type %s %d", __FILE__, __LINE__);
            break;

        case GI_TYPE_TAG_ERROR:
            // FIXME: unhandled
            g_critical("Unhandled error argument type %s %d", __FILE__, __LINE__);
            //ret = gig_argument_convert_error_to_arg(object, arg_info, must_free, arg);
            break;

        case GI_TYPE_TAG_ARRAY:
            // g_critical("Unhandled array argument type %s %d", __FILE__, __LINE__);
            c_array_to_scm(C2S_ARGS);
            break;

        default:
            g_assert_not_reached();
        }
    }
}

gchar *
gig_argument_describe_return(GITypeInfo *type_info,
                             GITransfer transfer, gboolean null_ok, gboolean skip)
{
    GString *desc = g_string_new(NULL);
    if (skip) {
        g_string_append(desc, "unspecified");
        goto ret_end;
    }
    if (null_ok)
        g_string_append(desc, "#f for NULL or ");

    GITypeTag type_tag = g_type_info_get_tag(type_info);
    gboolean is_ptr = g_type_info_is_pointer(type_info);

    if (!is_ptr) {
        switch (type_tag) {
        case GI_TYPE_TAG_VOID:
            g_string_append(desc, "unspecified");
            break;
        default:
            describe_non_pointer_type(desc, type_info);
            break;
        }
    }
    else {
        switch (type_tag) {
        case GI_TYPE_TAG_BOOLEAN:
        case GI_TYPE_TAG_DOUBLE:
        case GI_TYPE_TAG_FLOAT:
        case GI_TYPE_TAG_INT16:
        case GI_TYPE_TAG_INT32:
        case GI_TYPE_TAG_INT64:
        case GI_TYPE_TAG_INT8:
        case GI_TYPE_TAG_UINT16:
        case GI_TYPE_TAG_UINT32:
        case GI_TYPE_TAG_UINT64:
        case GI_TYPE_TAG_UINT8:
        case GI_TYPE_TAG_UNICHAR:
        case GI_TYPE_TAG_VOID:
            g_string_append(desc, "a pointer");
            break;

        case GI_TYPE_TAG_UTF8:
        case GI_TYPE_TAG_FILENAME:
            g_string_append(desc, "a string");
            break;

        case GI_TYPE_TAG_GHASH:
            g_string_append(desc, "a hash");
            break;
        case GI_TYPE_TAG_GLIST:
        case GI_TYPE_TAG_GSLIST:
            g_string_append(desc, "a list");
            break;

        case GI_TYPE_TAG_INTERFACE:
        {
            GIBaseInfo *referenced_base_info = g_type_info_get_interface(type_info);
            GIInfoType referenced_base_type = g_base_info_get_type(referenced_base_info);
            GType referenced_base_gtype = g_registered_type_info_get_g_type(referenced_base_info);
            g_base_info_unref(referenced_base_info);

            if (referenced_base_type == GI_INFO_TYPE_STRUCT ||
                referenced_base_type == GI_INFO_TYPE_UNION ||
                referenced_base_type == GI_INFO_TYPE_OBJECT) {
                gchar *class_name = gig_type_class_name_from_gtype(referenced_base_gtype);
                g_string_append(desc, class_name);
                g_free(class_name);
            }
            else
                g_string_append(desc, g_type_name(referenced_base_gtype));
            break;
        }
        case GI_TYPE_TAG_GTYPE:
            g_string_append(desc, "a GType");
            break;
        case GI_TYPE_TAG_ERROR:
            g_string_append(desc, "a GError");
            break;
        case GI_TYPE_TAG_ARRAY:
            g_string_append(desc, "an array");
            break;
        default:
            g_assert_not_reached();
        }
    }
  ret_end:
    return g_string_free(desc, FALSE);
}


static void
c_immediate_to_scm(C2S_ARG_DECL)
{
    switch (entry->type_tag) {
    case GI_TYPE_TAG_BOOLEAN:
        *object = scm_from_bool(arg->v_boolean);
        break;
    case GI_TYPE_TAG_INT8:
        *object = scm_from_int8(arg->v_int8);
        break;
    case GI_TYPE_TAG_UINT8:
        *object = scm_from_uint8(arg->v_uint8);
        break;
    case GI_TYPE_TAG_INT16:
        *object = scm_from_int16(arg->v_int16);
        break;
    case GI_TYPE_TAG_UINT16:
        *object = scm_from_uint16(arg->v_uint16);
        break;
    case GI_TYPE_TAG_INT32:
        *object = scm_from_int32(arg->v_int32);
        break;
    case GI_TYPE_TAG_UINT32:
        *object = scm_from_uint32(arg->v_uint32);
        break;
    case GI_TYPE_TAG_INT64:
        *object = scm_from_int64(arg->v_int64);
        break;
    case GI_TYPE_TAG_UINT64:
        *object = scm_from_uint64(arg->v_uint64);
        break;
    case GI_TYPE_TAG_FLOAT:
        *object = scm_from_double((double)arg->v_float);
        break;
    case GI_TYPE_TAG_DOUBLE:
        *object = scm_from_double(arg->v_double);
        break;
    case GI_TYPE_TAG_GTYPE:
        gig_type_register(arg->v_size);
        *object = scm_from_size_t(arg->v_size);
        break;
    case GI_TYPE_TAG_UNICHAR:
        *object = SCM_MAKE_CHAR(arg->v_uint32);
        break;
    default:
        return;
    }
    return;
}

static void
c_interface_pointer_to_scm(C2S_ARG_DECL)
{
    g_assert_cmpint(entry->type_tag, ==, GI_TYPE_TAG_INTERFACE);
    g_assert_cmpint(entry->is_ptr, ==, TRUE);
    g_assert_nonnull(arg);

    GIBaseInfo *referenced_base_info = g_type_info_get_interface(entry->type_info);
    GIInfoType referenced_info_type = g_base_info_get_type(referenced_base_info);
    if (referenced_info_type == GI_INFO_TYPE_ENUM) {
        g_assert_nonnull(arg->v_pointer);
        gint val = *(gint *)arg->v_pointer;
        *object = scm_from_int(val);
    }
    else if (referenced_info_type == GI_INFO_TYPE_ENUM) {
        g_assert_nonnull(arg->v_pointer);
        guint val = *(guint *) arg->v_pointer;
        *object = scm_from_uint(val);
    }
    else if (referenced_info_type == GI_INFO_TYPE_CALLBACK) {
        g_assert_nonnull(arg->v_pointer);
        gpointer callback_ptr = arg->v_pointer;
        *object = scm_from_pointer(callback_ptr, NULL);
    }
    else if (referenced_info_type == GI_INFO_TYPE_STRUCT
             || referenced_info_type == GI_INFO_TYPE_UNION
             || referenced_info_type == GI_INFO_TYPE_OBJECT) {
        g_assert_nonnull(arg->v_pointer);
        GType referenced_base_gtype = g_registered_type_info_get_g_type(referenced_base_info);
        *object = gig_type_transfer_object(referenced_base_gtype, arg->v_pointer, entry->transfer);
    }
    g_base_info_unref(referenced_base_info);
}

static void
c_interface_to_scm(C2S_ARG_DECL)
{
    g_assert(entry->type_tag == GI_TYPE_TAG_INTERFACE);

    GIBaseInfo *referenced_base_info = g_type_info_get_interface(entry->type_info);
    GIInfoType referenced_info_type = g_base_info_get_type(referenced_base_info);
    if (referenced_info_type == GI_INFO_TYPE_ENUM || referenced_info_type == GI_INFO_TYPE_FLAGS) {
        *object = scm_from_uint32(arg->v_uint32);
    }
    else if (referenced_info_type == GI_INFO_TYPE_CALLBACK) {
        gpointer callback_ptr = arg->v_pointer;
        *object = scm_from_pointer(callback_ptr, NULL);
    }
    else {
        // This case of returning a struct directly.
        g_assert_not_reached();
    }
    g_base_info_unref(referenced_base_info);
}


static void
c_string_to_scm(C2S_ARG_DECL)
{
    // We can't transfer strings directly, since GObject and Guile use
    // different internal encodings.  So for GI_TRANSFER_EVERYTHGING,
    // we just free.
    switch (entry->type_tag) {

    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
        if (!arg->v_string)
            *object = scm_c_make_string(0, SCM_MAKE_CHAR(0));
        else {
            if (entry->type_tag == GI_TYPE_TAG_UTF8) {
                if (size != GIG_ARRAY_SIZE_UNKNOWN)
                    *object = scm_from_utf8_stringn(arg->v_string, size);
                else
                    *object = scm_from_utf8_string(arg->v_string);
            }
            else {
                if (size != GIG_ARRAY_SIZE_UNKNOWN)
                    *object = scm_from_locale_stringn(arg->v_string, size);
                else
                    *object = scm_from_locale_string(arg->v_string);
            }
            if (entry->transfer == GI_TRANSFER_EVERYTHING) {
                g_free(arg->v_string);
                arg->v_string = NULL;
            }
        }
        break;
    default:
        break;
    }
}

static void
c_array_to_scm(C2S_ARG_DECL)
{
    // Array length is a misnomer. A positive array length indicates that
    // we are supposed to use an array length passed in as an argument.
    if (entry->array_length_index >= 0)
        entry->array_fixed_size = size;

    switch (entry->array_type) {
    case GI_ARRAY_TYPE_BYTE_ARRAY:
        c_byte_array_to_scm(C2S_ARGS);
        break;
    case GI_ARRAY_TYPE_C:
        c_native_array_to_scm(C2S_ARGS);
        break;
    case GI_ARRAY_TYPE_ARRAY:
        c_garray_to_scm(C2S_ARGS);
        break;
    default:
        g_critical("Unhandled array type in %s:%d", __FILE__, __LINE__);
        g_assert_not_reached();
        break;
    }

    return;
}

static void
c_native_array_to_scm(C2S_ARG_DECL)
{
    gsize length = array_length(entry, arg);

    if (length == GIG_ARRAY_SIZE_UNKNOWN) {
        c_void_pointer_to_scm(C2S_ARGS);
        return;
    }
    *object = SCM_UNDEFINED;

    switch (entry->item_type_tag) {
    case GI_TYPE_TAG_INT8:
    case GI_TYPE_TAG_UINT8:
    case GI_TYPE_TAG_INT16:
    case GI_TYPE_TAG_UINT16:
    case GI_TYPE_TAG_INT32:
    case GI_TYPE_TAG_UINT32:
    case GI_TYPE_TAG_INT64:
    case GI_TYPE_TAG_UINT64:
    case GI_TYPE_TAG_FLOAT:
    case GI_TYPE_TAG_DOUBLE:
    case GI_TYPE_TAG_GTYPE:
    case GI_TYPE_TAG_BOOLEAN:
    case GI_TYPE_TAG_UNICHAR:
        // we already determined the item size earlier, nothing to do here
        break;
    case GI_TYPE_TAG_INTERFACE:
        switch (entry->referenced_base_type) {
        case GI_INFO_TYPE_ENUM:
        case GI_INFO_TYPE_FLAGS:
            if (entry->item_is_ptr) {
                // Don't think there are any output arrays of pointers to flags or enums
                g_critical("Unhandled array type in %s:%d", __FILE__, __LINE__);
                g_assert_not_reached();
            }
            break;
        case GI_INFO_TYPE_STRUCT:
        case GI_INFO_TYPE_OBJECT:
            g_assert(entry->referenced_object_type != G_TYPE_NONE);

            g_critical("Unhandled array type in %s:%d", __FILE__, __LINE__);
            g_assert_not_reached();
            break;
        default:
            g_critical("Unhandled array type in %s:%d", __FILE__, __LINE__);
            g_assert_not_reached();
            break;
        }
        break;
    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
    {
        *object = scm_c_make_vector(length, SCM_BOOL_F);
        scm_t_array_handle handle;
        gsize len;
        gssize inc;
        SCM *elt;

        elt = scm_vector_writable_elements(*object, &handle, &len, &inc);
        g_assert(len == length);

        for (gsize i = 0; i < length; i++, elt += inc) {
            gchar *str = ((char **)arg->v_pointer)[i];
            if (str) {
                if (entry->item_type_tag == GI_TYPE_TAG_UTF8)
                    *elt = scm_from_utf8_string(str);
                else
                    *elt = scm_from_locale_string(str);
            }
        }
        scm_array_handle_release(&handle);
        break;
    }
    default:
        g_critical("Unhandled array type in %s:%d", __FILE__, __LINE__);
        g_assert_not_reached();
    }

    if (SCM_UNBNDP(*object) && entry->item_size) {
        gsize sz = length * entry->item_size;
        // FIXME: maybe return a typed vector or list
        *object = scm_c_make_bytevector(sz);
        memcpy(SCM_BYTEVECTOR_CONTENTS(*object), arg->v_pointer, sz);
    }
}

static void
c_byte_array_to_scm(C2S_ARG_DECL)
{
    GByteArray *byte_array = arg->v_pointer;
    *object = scm_c_make_bytevector(byte_array->len);
    memcpy(SCM_BYTEVECTOR_CONTENTS(*object), byte_array->data, byte_array->len);
    if (entry->item_transfer == GI_TRANSFER_EVERYTHING)
        g_byte_array_free(byte_array, TRUE);
    else
        g_byte_array_free(byte_array, FALSE);
}

static void
c_garray_to_scm(C2S_ARG_DECL)
{
    GArray *array = arg->v_pointer;
    gpointer data = array->data;

    // We hopefully never have to deal with GArrays of pointer types,
    // given that GPtrArray exists.
    g_assert_false(entry->item_is_ptr);
    g_assert_cmpint(entry->item_size, !=, 0);

    *object = scm_c_make_vector(array->len, SCM_UNDEFINED);

    scm_t_array_handle handle;
    gsize len, item_size = entry->item_size;
    gssize inc;
    SCM *elt;

    elt = scm_vector_writable_elements(*object, &handle, &len, &inc);
    g_assert(len == array->len);

    for (gsize i = 0; i < len; i++, elt += inc, data += item_size) {
        *elt = scm_c_make_bytevector(item_size);
        memcpy(SCM_BYTEVECTOR_CONTENTS(*elt), data, item_size);
    }
    scm_array_handle_release(&handle);
}

static void
c_list_to_scm(C2S_ARG_DECL)
{
    // Dissect layers as in `fill_array_info`, except that less information
    // is needed.
    GITypeInfo *list_type_info = entry->type_info;
    GITransfer list_transfer = entry->transfer;
    GITypeTag list_type_tag = g_type_info_get_tag(list_type_info);
    GITypeInfo *item_type_info = g_type_info_get_param_type(list_type_info, 0);
    GITypeTag item_type_tag = g_type_info_get_tag(item_type_info);
    gboolean item_is_ptr = g_type_info_is_pointer(item_type_info);

    GITransfer item_transfer;
    if (list_transfer == GI_TRANSFER_EVERYTHING)
        item_transfer = GI_TRANSFER_EVERYTHING;
    else
        item_transfer = GI_TRANSFER_NOTHING;

    // Actual conversion
    gpointer list = arg->v_pointer, data;
    GList *_list = NULL;
    GSList *_slist = NULL;
    gsize length;

    // Step 1: allocate
    switch (list_type_tag) {
    case GI_TYPE_TAG_GLIST:
        _list = list;
        length = g_list_length(_list);
        break;
    case GI_TYPE_TAG_GSLIST:
        _slist = list;
        length = g_slist_length(_slist);
        break;
    default:
        g_assert_not_reached();
    }

    *object = scm_make_list(scm_from_size_t(length), SCM_UNDEFINED);

    SCM out_iter = *object;

    // Step 2: iterate
    while (list != NULL) {
        switch (list_type_tag) {
        case GI_TYPE_TAG_GLIST:
            data = &_list->data;
            list = _list = _list->next;
            break;
        case GI_TYPE_TAG_GSLIST:
            data = &_slist->data;
            list = _slist = _slist->next;
            break;
        default:
            g_assert_not_reached();
        }

        if (!item_is_ptr) {
            switch (item_type_tag) {
            case GI_TYPE_TAG_INT8:
                scm_set_car_x(out_iter, scm_from_int8(*(gint8 *) data));
                break;
            case GI_TYPE_TAG_INT16:
                scm_set_car_x(out_iter, scm_from_int16(*(gint16 *) data));
                break;
            case GI_TYPE_TAG_INT32:
                scm_set_car_x(out_iter, scm_from_int32(*(gint32 *) data));
                break;
            case GI_TYPE_TAG_INT64:
                scm_set_car_x(out_iter, scm_from_int64(*(gint64 *) data));
                break;
            case GI_TYPE_TAG_UINT8:
                scm_set_car_x(out_iter, scm_from_uint8(*(guint8 *) data));
                break;
            case GI_TYPE_TAG_UINT16:
                scm_set_car_x(out_iter, scm_from_uint16(*(guint16 *) data));
                break;
            case GI_TYPE_TAG_UINT32:
                scm_set_car_x(out_iter, scm_from_uint32(*(guint32 *) data));
                break;
            case GI_TYPE_TAG_UINT64:
                scm_set_car_x(out_iter, scm_from_uint64(*(guint64 *) data));
                break;
            case GI_TYPE_TAG_FLOAT:
                scm_set_car_x(out_iter, scm_from_double(*(float *)data));
                break;
            case GI_TYPE_TAG_DOUBLE:
                scm_set_car_x(out_iter, scm_from_double(*(double *)data));
                break;
            case GI_TYPE_TAG_UNICHAR:
                scm_set_car_x(out_iter, SCM_MAKE_CHAR(*(guint32 *) data));
                break;
            case GI_TYPE_TAG_GTYPE:
                gig_type_register(*(gsize *)data);
                scm_set_car_x(out_iter, scm_from_size_t(*(gsize *)data));
                break;
            default:
                g_critical("Unhandled item type in %s:%d", __FILE__, __LINE__);
                list = NULL;
            }
        }
        else {
            GIArgument _arg;
            GigArgMapEntry *ae = g_new0(GigArgMapEntry, 1);
            SCM elt;
            _arg.v_pointer = *(void **)data;

            ae->name = g_strdup("(list internal)");
            ae->type_info = item_type_info;
            ae->type_tag = item_type_tag;
            ae->is_ptr = item_is_ptr;
            ae->transfer = item_transfer;
            gig_argument_c_to_scm(subr, argpos, ae, &_arg, must_free, &elt, -1);
            scm_set_car_x(out_iter, elt);
            g_free(ae->name);
            g_free(ae);
        }

        out_iter = scm_cdr(out_iter);
    }
    g_base_info_unref(item_type_info);

}

static void
c_void_pointer_to_scm(C2S_ARG_DECL)
{
    if (size != GIG_ARRAY_SIZE_UNKNOWN) {
        SCM bv = scm_c_make_bytevector(size);
        memcpy(SCM_BYTEVECTOR_CONTENTS(bv), arg->v_pointer, size);
        *object = bv;
    }
    else
        *object = scm_from_pointer(arg->v_pointer, NULL);
}

gboolean
gig_argument_to_gssize(const gchar *func,
                       GIArgument *arg_in, GITypeTag type_tag, gssize *gssize_out)
{
    const gchar *type_name = g_type_tag_to_string(type_tag);

    switch (type_tag) {
    case GI_TYPE_TAG_INT8:
        *gssize_out = arg_in->v_int8;
        return TRUE;
    case GI_TYPE_TAG_UINT8:
        *gssize_out = arg_in->v_uint8;
        return TRUE;
    case GI_TYPE_TAG_INT16:
        *gssize_out = arg_in->v_int16;
        return TRUE;
    case GI_TYPE_TAG_UINT16:
        *gssize_out = arg_in->v_uint16;
        return TRUE;
    case GI_TYPE_TAG_INT32:
        *gssize_out = arg_in->v_int32;
        return TRUE;
    case GI_TYPE_TAG_UINT32:
        *gssize_out = arg_in->v_uint32;
        return TRUE;
    case GI_TYPE_TAG_INT64:
        if (arg_in->v_int64 > G_MAXSSIZE || arg_in->v_int64 < G_MINSSIZE) {
            scm_misc_error(func,
                           "Unable to marshal ~A to gssize",
                           scm_list_1(scm_from_utf8_string(type_name)));
            return FALSE;
        }
        *gssize_out = (gssize)arg_in->v_int64;
        return TRUE;
    case GI_TYPE_TAG_UINT64:
        if (arg_in->v_uint64 > G_MAXSSIZE) {
            scm_misc_error(func,
                           "Unable to marshal ~A to gssize",
                           scm_list_1(scm_from_utf8_string(type_name)));
            return FALSE;
        }
        *gssize_out = (gssize)arg_in->v_uint64;
        return TRUE;
    default:
        scm_misc_error(func,
                       "Unable to marshall ~A to gssize",
                       scm_list_1(scm_from_utf8_string(type_name)));
        return FALSE;
    }
}



#define SCONSTX(NAME) scm_permanent_object(scm_c_define(#NAME, scm_from_int(NAME)))

void
gig_init_argument(void)
{
    SCONSTX(GI_TYPE_TAG_VOID);
    SCONSTX(GI_TYPE_TAG_BOOLEAN);
    SCONSTX(GI_TYPE_TAG_INT8);
    SCONSTX(GI_TYPE_TAG_UINT8);
    SCONSTX(GI_TYPE_TAG_INT16);
    SCONSTX(GI_TYPE_TAG_UINT16);
    SCONSTX(GI_TYPE_TAG_INT32);
    SCONSTX(GI_TYPE_TAG_UINT32);
    SCONSTX(GI_TYPE_TAG_INT64);
    SCONSTX(GI_TYPE_TAG_UINT64);
    SCONSTX(GI_TYPE_TAG_FLOAT);
    SCONSTX(GI_TYPE_TAG_DOUBLE);
    SCONSTX(GI_TYPE_TAG_GTYPE);
    SCONSTX(GI_TYPE_TAG_UTF8);
    SCONSTX(GI_TYPE_TAG_FILENAME);
    SCONSTX(GI_TYPE_TAG_ARRAY);
    SCONSTX(GI_TYPE_TAG_INTERFACE);
    SCONSTX(GI_TYPE_TAG_GLIST);
    SCONSTX(GI_TYPE_TAG_GSLIST);
    SCONSTX(GI_TYPE_TAG_GHASH);
    SCONSTX(GI_TYPE_TAG_ERROR);
    SCONSTX(GI_TYPE_TAG_UNICHAR);
    SCONSTX(GI_TRANSFER_NOTHING);
    SCONSTX(GI_TRANSFER_CONTAINER);
    SCONSTX(GI_TRANSFER_EVERYTHING);
}