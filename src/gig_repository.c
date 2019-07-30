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

#include <libguile.h>
#include <girepository.h>
#include "gig_type.h"
#include "gig_object.h"
#include "gig_function.h"
#include "gig_util.h"

static SCM
require(SCM lib, SCM version)
{
    gchar *_lib, *_version = NULL;
    GITypelib *tl;
    GError *error = NULL;
    SCM slib;

    _lib = scm_to_utf8_string(lib);
    _version = scm_to_utf8_string(version);

    g_debug("requiring %s-%s", _lib, _version != NULL ? _version : "latest");
    tl = g_irepository_require(NULL, _lib, _version, 0, &error);
    g_free(_lib);
    g_free(_version);

    if (tl == NULL) {
        SCM err = scm_from_utf8_string(error->message);
        g_error_free(error);
        scm_misc_error("require", "~A", scm_list_1(err));
    }

    return SCM_UNSPECIFIED;
}

static SCM
infos(SCM lib)
{
    scm_dynwind_begin(0);
    gchar *_lib = scm_dynwind_or_bust("infos", scm_to_utf8_string(lib));
    gint n = g_irepository_get_n_infos(NULL, _lib);
    SCM infos = SCM_EOL;

    for (gint i = 0; i < n; i++) {
        GIBaseInfo *info = g_irepository_get_info(NULL, _lib, i);
        if (g_base_info_is_deprecated(info)) {
            g_base_info_unref(info);
            continue;
        }
        infos = scm_cons (gig_type_transfer_object(GI_TYPE_BASE_INFO, info, GI_TRANSFER_EVERYTHING),
                          infos);
    }
    scm_dynwind_end();

    return scm_reverse_x(infos, SCM_EOL);
}

void
gig_init_repository()
{
    scm_c_define_gsubr("require", 1, 1, 0, require);
    scm_c_define_gsubr("infos", 1, 0, 0, infos);
}
