// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Inkscape::IO::Resource - simple resource API
 *//*
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Martin Owens <doctormo@gmail.com>
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#ifdef _WIN32
#include <shlobj.h> // for SHGetSpecialFolderLocation
#endif

#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <glibmm/stringutils.h>
#include <glibmm/fileutils.h>

#include "path-prefix.h"
#include "io/sys.h"
#include "io/resource.h"

using Inkscape::IO::file_test;

namespace Inkscape {

namespace IO {

namespace Resource {

#define INKSCAPE_PROFILE_DIR "inkscape"

gchar *_get_path(Domain domain, Type type, char const *filename)
{
    gchar *path=nullptr;
    switch (domain) {
        case SYSTEM: {
            gchar const* temp = nullptr;
            switch (type) {
                case EXTENSIONS: temp = INKSCAPE_EXTENSIONDIR; break;
                case FILTERS: temp = INKSCAPE_FILTERDIR; break;
                case FONTS: temp = INKSCAPE_FONTSDIR; break;
                case ICONS: temp = INKSCAPE_ICONSDIR; break;
                case KEYS: temp = INKSCAPE_KEYSDIR; break;
                case MARKERS: temp = INKSCAPE_MARKERSDIR; break;
                case NONE: g_assert_not_reached(); break;
                case PAINT: temp = INKSCAPE_PAINTDIR; break;
                case PALETTES: temp = INKSCAPE_PALETTESDIR; break;
                case SCREENS: temp = INKSCAPE_SCREENSDIR; break;
                case SYMBOLS: temp = INKSCAPE_SYMBOLSDIR; break;
                case TEMPLATES: temp = INKSCAPE_TEMPLATESDIR; break;
                case THEMES: temp = INKSCAPE_THEMEDIR; break;
                case TUTORIALS: temp = INKSCAPE_TUTORIALSDIR; break;
                case UIS: temp = INKSCAPE_UIDIR; break;
                case PIXMAPS: temp = INKSCAPE_PIXMAPSDIR; break;
                default: temp = "";
            }
            path = g_strdup(temp);
        } break;
        case CREATE: {
            gchar const* temp = nullptr;
            switch (type) {
                case PAINT: temp = CREATE_PAINTDIR; break;
                case PALETTES: temp = CREATE_PALETTESDIR; break;
                default: temp = "";
            }
            path = g_strdup(temp);
        } break;
        case CACHE: {
            path = g_build_filename(g_get_user_cache_dir(), "inkscape", NULL);
        } break;
        case USER: {
            char const *name=nullptr;
            switch (type) {
                case EXTENSIONS: name = "extensions"; break;
                case FILTERS: name = "filters"; break;
                case FONTS: name = "fonts"; break;
                case ICONS: name = "icons"; break;
                case KEYS: name = "keys"; break;
                case MARKERS: name = "markers"; break;
                case NONE: name = ""; break;
                case PAINT: name = "paint"; break;
                case PALETTES: name = "palettes"; break;
                case SYMBOLS: name = "symbols"; break;
                case TEMPLATES: name = "templates"; break;
                case THEMES: name = "themes"; break;
                case UIS: name = "ui"; break;
                case PIXMAPS: name = "pixmaps"; break;
                default: return _get_path(SYSTEM, type, filename);
            }
            path = profile_path(name);
        } break;
    }


    if (filename && path) {
        gchar *temp=g_build_filename(path, filename, NULL);
        g_free(path);
        path = temp;
    }

    return path;
}



Util::ptr_shared get_path(Domain domain, Type type, char const *filename)
{
    char *path = _get_path(domain, type, filename);
    Util::ptr_shared result=Util::share_string(path);
    g_free(path);
    return result;
}
Glib::ustring get_path_ustring(Domain domain, Type type, char const *filename)
{
    Glib::ustring result;
    char *path = _get_path(domain, type, filename);
    if(path) {
      result = Glib::ustring(path);
      g_free(path);
    }
    return result;
}

/*
 * Same as get_path, but checks for file's existence and falls back
 * from USER to SYSTEM modes.
 *
 *  type      - The type of file to get, such as extension, template, ui etc
 *  filename  - The filename to get, i.e. preferences.xml
 *  localized - Prefer a localized version of the file, i.e. default.de.svg instead of default.svg.
 *              (will use gettext to determine the preferred language of the user)
 */
Glib::ustring get_filename(Type type, char const *filename, bool localized, bool silent)
{
    Glib::ustring result;

    char *user_filename = nullptr;
    char *sys_filename = nullptr;
    char *user_filename_localized = nullptr;
    char *sys_filename_localized = nullptr;

    // TRANSLATORS: 'en' is an ISO 639-1 language code.
    // Replace with language code for your language, i.e. the name of your .po file
    localized = localized && strcmp(_("en"), "en");

    if (localized) {
        Glib::ustring localized_filename = filename;
        localized_filename.insert(localized_filename.rfind('.'), ".");
        localized_filename.insert(localized_filename.rfind('.'), _("en"));

        user_filename_localized = _get_path(USER, type, localized_filename.c_str());
        sys_filename_localized = _get_path(SYSTEM, type, localized_filename.c_str());
    }
    user_filename = _get_path(USER, type, filename);
    sys_filename = _get_path(SYSTEM, type, filename);

    // impose the following load order:
    //   USER (localized) > USER > SYSTEM (localized) > SYSTEM
    if (localized && file_test(user_filename_localized, G_FILE_TEST_EXISTS)) {
        result = Glib::ustring(user_filename_localized);
        g_info("Found localized version of resource file '%s' in profile directory:\n\t%s", filename, result.c_str());
    } else if (file_test(user_filename, G_FILE_TEST_EXISTS)) {
        result = Glib::ustring(user_filename);
        g_info("Found resource file '%s' in profile directory:\n\t%s", filename, result.c_str());
    } else if (localized && file_test(sys_filename_localized, G_FILE_TEST_EXISTS)) {
        result = Glib::ustring(sys_filename_localized);
        g_info("Found localized version of resource file '%s' in system directory:\n\t%s", filename, result.c_str());
    } else if (file_test(sys_filename, G_FILE_TEST_EXISTS)) {
        result = Glib::ustring(sys_filename);
        g_info("Found resource file '%s' in system directory:\n\t%s", filename, result.c_str());
    } else if (!silent) {
        if (localized) {
            g_warning("Failed to find resource file '%s'. Looked in:\n\t%s\n\t%s\n\t%s\n\t%s",
                      filename, user_filename_localized, user_filename, sys_filename_localized, sys_filename);
        } else {
            g_warning("Failed to find resource file '%s'. Looked in:\n\t%s\n\t%s",
                      filename, user_filename, sys_filename);
        }
    }

    g_free(user_filename);
    g_free(sys_filename);
    g_free(user_filename_localized);
    g_free(sys_filename_localized);

    return result;
}

/*
 * Similar to get_filename, but takes a path (or filename) for relative resolution
 *
 *  path - A directory or filename that is considered local to the path resolution.
 *  filename - The filename that we are looking for.
 */
Glib::ustring get_filename(Glib::ustring path, Glib::ustring filename)
{
    // Test if it's a filename and get the parent directory instead
    if (Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR)) {
        return get_filename(g_path_get_dirname(path.c_str()), filename);
    }
    if (g_path_is_absolute(filename.c_str())) {
        if (Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
            return filename;
        }
    } else {
        Glib::ustring ret = Glib::build_filename(path, filename);
        if (Glib::file_test(ret, Glib::FILE_TEST_EXISTS)) {
            return ret;
        }
    }
    return Glib::ustring();
}

/*
 * Gets all the files in a given type, for all domain types.
 *
 *  domain - Optional domain (overload), will check return domains if not.
 *  type - The type of files, e.g. TEMPLATES
 *  extensions - A list of extensions to return, e.g. xml, svg
 *  exclusions - A list of names to exclude e.g. default.xml
 */
std::vector<Glib::ustring> get_filenames(Type type, std::vector<const char *> extensions, std::vector<const char *> exclusions)
{
    std::vector<Glib::ustring> ret;
    get_filenames_from_path(ret, get_path_ustring(USER, type), extensions, exclusions);
    get_filenames_from_path(ret, get_path_ustring(SYSTEM, type), extensions, exclusions);
    get_filenames_from_path(ret, get_path_ustring(CREATE, type), extensions, exclusions);
    return ret;
}

std::vector<Glib::ustring> get_filenames(Domain domain, Type type, std::vector<const char *> extensions, std::vector<const char *> exclusions)
{
    std::vector<Glib::ustring> ret;
    get_filenames_from_path(ret, get_path_ustring(domain, type), extensions, exclusions);
    return ret;
}
std::vector<Glib::ustring> get_filenames(Glib::ustring path, std::vector<const char *> extensions, std::vector<const char *> exclusions)
{
    std::vector<Glib::ustring> ret;
    get_filenames_from_path(ret, path, extensions, exclusions);
    return ret;
}

/*
 * Gets all folders inside each type, for all domain types.
 *
 *  domain - Optional domain (overload), will check return domains if not.
 *  type - The type of files, e.g. TEMPLATES
 *  extensions - A list of extensions to return, e.g. xml, svg
 *  exclusions - A list of names to exclude e.g. default.xml
 */
std::vector<Glib::ustring> get_foldernames(Type type, std::vector<const char *> exclusions)
{
    std::vector<Glib::ustring> ret;
    get_foldernames_from_path(ret, get_path_ustring(USER, type), exclusions);
    get_foldernames_from_path(ret, get_path_ustring(SYSTEM, type), exclusions);
    get_foldernames_from_path(ret, get_path_ustring(CREATE, type), exclusions);
    return ret;
}

std::vector<Glib::ustring> get_foldernames(Domain domain, Type type, std::vector<const char *> exclusions)
{
    std::vector<Glib::ustring> ret;
    get_foldernames_from_path(ret, get_path_ustring(domain, type), exclusions);
    return ret;
}
std::vector<Glib::ustring> get_foldernames(Glib::ustring path, std::vector<const char *> exclusions)
{
    std::vector<Glib::ustring> ret;
    get_foldernames_from_path(ret, path, exclusions);
    return ret;
}


/*
 * Get all the files from a specific path and any sub-dirs, populating &files vector
 *
 * &files - Output list to populate, will be populated with full paths
 * path - The directory to parse, will add nothing if directory doesn't exist
 * extensions - Only add files with these extensions, they must be duplicated
 * exclusions - Exclude files that exactly match these names.
 */
void get_filenames_from_path(std::vector<Glib::ustring> &files, Glib::ustring path, std::vector<const char *> extensions, std::vector<const char *> exclusions)
{
    if(!Glib::file_test(path, Glib::FILE_TEST_IS_DIR)) {
        return;
    }

    Glib::Dir dir(path);
    std::string file = dir.read_name();
    while (!file.empty()){
        // If not extensions are specified, don't reject ANY files.
        bool reject = !extensions.empty();

        // Unreject any file which has one of the extensions.
        for (auto &ext: extensions) {
	    reject ^= Glib::str_has_suffix(file, ext);
        }

        // Reject any file which matches the exclusions.
        for (auto &exc: exclusions) {
	    reject |= Glib::str_has_prefix(file, exc);
        }

        // Reject any filename which isn't a regular file
        Glib::ustring filename = Glib::build_filename(path, file);

        if(Glib::file_test(filename, Glib::FILE_TEST_IS_DIR)) {
            get_filenames_from_path(files, filename, extensions, exclusions);
        } else if(Glib::file_test(filename, Glib::FILE_TEST_IS_REGULAR) && !reject) {
            files.push_back(filename);
        }
        file = dir.read_name();
    }
}

/*
 * Get all the files from a specific path and any sub-dirs, populating &files vector
 *
 * &folders - Output list to populate, will be poulated with full paths
 * path - The directory to parse, will add nothing if directory doesn't exist
 * exclusions - Exclude files that exactly match these names.
 */
void get_foldernames_from_path(std::vector<Glib::ustring> &folders, Glib::ustring path,
                               std::vector<const char *> exclusions)
{
    if (!Glib::file_test(path, Glib::FILE_TEST_IS_DIR)) {
        return;
    }

    Glib::Dir dir(path);
    std::string file = dir.read_name();
    while (!file.empty()) {
        // If not extensions are specified, don't reject ANY files.
        bool reject = false;

        // Reject any file which matches the exclusions.
        for (auto &exc : exclusions) {
            reject |= Glib::str_has_prefix(file, exc);
        }

        // Reject any filename which isn't a regular file
        Glib::ustring filename = Glib::build_filename(path, file);

        if (Glib::file_test(filename, Glib::FILE_TEST_IS_DIR) && !reject) {
            folders.push_back(filename);
        }
        file = dir.read_name();
    }
}


/**
 * Get, or guess, or decide the location where the preferences.xml
 * file should be located. This also indicates where all other inkscape
 * shared files may optionally exist.
 */
char *profile_path(const char *filename)
{
    static const gchar *prefdir = nullptr;

    if (!prefdir) {
        // Check if profile directory is overridden using environment variable
        gchar const *userenv = g_getenv("INKSCAPE_PROFILE_DIR");
        if (userenv) {
            prefdir = g_strdup(userenv);
        }

#ifdef _WIN32
        // prefer c:\Documents and Settings\UserName\Application Data\ to c:\Documents and Settings\userName\;
        // TODO: CSIDL_APPDATA is C:\Users\UserName\AppData\Roaming these days
        //       should we migrate to AppData\Local? Then we can simply use the portable g_get_user_config_dir()
        if (!prefdir) {
            ITEMIDLIST *pidl = 0;
            if ( SHGetFolderLocation( NULL, CSIDL_APPDATA, NULL, 0, &pidl ) == S_OK ) {
                gchar * utf8Path = NULL;

                {
                    wchar_t pathBuf[MAX_PATH+1];
                    g_assert(sizeof(wchar_t) == sizeof(gunichar2));

                    if ( SHGetPathFromIDListW( pidl, pathBuf ) ) {
                        utf8Path = g_utf16_to_utf8( (gunichar2*)(&pathBuf[0]), -1, NULL, NULL, NULL );
                    }
                }

                if ( utf8Path ) {
                    if (!g_utf8_validate(utf8Path, -1, NULL)) {
                        g_warning( "SHGetPathFromIDListW() resulted in invalid UTF-8");
                        g_free( utf8Path );
                        utf8Path = 0;
                    } else {
                        prefdir = utf8Path;
                    }
                }
            }

            if (prefdir) {
                const char *prefdir_profile = g_build_filename(prefdir, INKSCAPE_PROFILE_DIR, NULL);
                g_free((void *)prefdir);
                prefdir = prefdir_profile;
            }
        }
#endif
        if (!prefdir) {
            prefdir = g_build_filename(g_get_user_config_dir(), INKSCAPE_PROFILE_DIR, NULL);
            // In case the XDG user config dir of the moment does not yet exist...
            int mode = S_IRWXU;
#ifdef S_IRGRP
            mode |= S_IRGRP;
#endif
#ifdef S_IXGRP
            mode |= S_IXGRP;
#endif
#ifdef S_IXOTH
            mode |= S_IXOTH;
#endif
            if ( g_mkdir_with_parents(prefdir, mode) == -1 ) {
                int problem = errno;
                g_warning("Unable to create profile directory (%s) (%d)", g_strerror(problem), problem);
            } else {
                gchar const *userDirs[] = { "keys", "templates", "icons", "extensions", "ui",
                                            "symbols", "paint", "themes", "palettes", nullptr };
                for (gchar const** name = userDirs; *name; ++name) {
                    gchar *dir = g_build_filename(prefdir, *name, NULL);
                    g_mkdir_with_parents(dir, mode);
                    g_free(dir);
                }
            }
        }
    }
    return g_build_filename(prefdir, filename, NULL);
}

/*
 * We return the profile_path because that is where most documentation
 * days log files will be generated in inkscape 0.92
 */
char *log_path(const char *filename)
{
    return profile_path(filename);
}

char *homedir_path(const char *filename)
{
    static const gchar *homedir = nullptr;
    homedir = g_get_home_dir();

    return g_build_filename(homedir, filename, NULL);
}

}

}

}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
