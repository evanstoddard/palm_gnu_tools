/* build-prc.cpp: build a .prc from a pile of files.

   Copyright (c) 1998-2000 Palm Computing, Inc. or its subsidiaries.
   All rights reserved.

   This is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program, build-prc v2.0, follows in the footsteps of obj-res and
   build-prc, the source code of which contains the following notices:

 * obj-res.c:  Dump out .prc compatible binary resource files from an object
 *
 * (c) 1996, 1997 Dionne & Associates
 * jeff@ryeham.ee.ryerson.ca
 *
 * This is Free Software, under the GNU Public Licence v2 or greater.
 *
 * Relocation added March 1997, Kresten Krab Thorup
 * krab@california.daimi.aau.dk

 * ptst.c:  build a .prc from a pile of files.
 *
 * (c) 1996, Dionne & Associates
 * (c) 1997, The Silver Hammer Group Ltd.
 * This is Free Software, under the GNU Public Licence v2 or greater.
 */

#include <map>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "getopt.h"
#include "utils.h"
#include "def.h"
#include "binres.hpp"
#include "pfd.hpp"
#include "pfdio.hpp"

static const char* version = "2.1";

void
usage() {
  printf ("\
Usage: Old-style: %s [options] outfile.prc 'App Name' apid file...\n",
	  progname);
  printf ("       New-style: %s [options] file...\n", progname);
  // Commented out options are intended but not yet implemented
  printf ("\
Files may be .bin/.grc, .prc/.ro, .def (new-style only), or linked executables\n");
  /* printf ("\
Files may be .bin, .grc, .prc, .def (new-style only), or linked executables,\n\
and may specify `f=#' to renumber and `f(type[#[-#]][,...])' to select\n"); */

  printf ("Options:\n");
  propt ("-o FILE, --output FILE",
	 "Set output .prc file name (new-style only)");
  propt ("-l, -L", "Build a GLib or a system library respectively");
  propt ("-a FILE, --appinfo FILE", "Add an AppInfo block");
  propt ("-s FILE, --sortinfo FILE", "Add a SortInfo block");
  propt ("-t TYPE, --type TYPE", "Set database type");
  propt ("-c CRID, --creator CRID", "Set database creator (new-style only)");
  propt ("-n NAME, --name NAME", "Set database name (new-style only)");
  propt ("-m NUM, --modification-number NUM",
	 "Set database modification number");
  propt ("-v NUM, --version-number NUM", "Set database version number");
  propt ("--readonly, --read-only, --appinfodirty, --appinfo-dirty, --backup,",
	 NULL);
  propt ("--oktoinstallnewer, --ok-to-install-newer, --resetafterinstall,",
	 NULL);
  propt ("--reset-after-install, --copyprevention, --copy-prevention,", NULL);

  propt ("--stream, --hidden, --launchabledata, --launchable-data",
	 "Set database attributes");
  propt ("-z N, --compress-data N",
	 "Set data resource compression method (0--2)");
  // propt ("-x, --provenance", "Output resource cross-reference");
  }

enum {
  OPTION_READONLY = 150,
  OPTION_APPINFO_DIRTY,
  OPTION_BACKUP,
  OPTION_OK_TO_INSTALL_NEWER,
  OPTION_RESET_AFTER_INSTALL,
  OPTION_COPY_PREVENTION,
  OPTION_STREAM,
  OPTION_HIDDEN,
  OPTION_LAUNCHABLE_DATA,
  OPTION_HELP,
  OPTION_VERSION
  };

static char* shortopts = "o:lLa:s:t:c:n:m:v:z:";

static struct option longopts[] = {
  { "output", required_argument, NULL, 'o' },
  { "appinfo", required_argument, NULL, 'a' },
  { "sortinfo", required_argument, NULL, 's' },
  { "type", required_argument, NULL, 't' },
  { "creator", required_argument, NULL, 'c' },
  { "name", required_argument, NULL, 'n' },
  { "modification-number", required_argument, NULL, 'm' },
  { "version-number", required_argument, NULL, 'v' },
  { "provenance", no_argument, NULL, 'x' },
  { "compress-data", required_argument, NULL, 'z' },

  { "readonly", no_argument, NULL, OPTION_READONLY },
  { "read-only", no_argument, NULL, OPTION_READONLY },
  { "appinfodirty", no_argument, NULL, OPTION_APPINFO_DIRTY },
  { "appinfo-dirty", no_argument, NULL, OPTION_APPINFO_DIRTY },
  { "backup", no_argument, NULL, OPTION_BACKUP },
  { "oktoinstallnewer", no_argument, NULL, OPTION_OK_TO_INSTALL_NEWER },
  { "ok-to-install-newer", no_argument, NULL, OPTION_OK_TO_INSTALL_NEWER },
  { "reset-after-install", no_argument, NULL, OPTION_RESET_AFTER_INSTALL },
  { "resetafterinstall", no_argument, NULL, OPTION_RESET_AFTER_INSTALL },
  { "copy-prevention", no_argument, NULL, OPTION_COPY_PREVENTION },
  { "copyprevention", no_argument, NULL, OPTION_COPY_PREVENTION },
  { "stream", no_argument, NULL, OPTION_STREAM },
  { "hidden", no_argument, NULL, OPTION_HIDDEN },
  { "launchabledata", no_argument, NULL, OPTION_LAUNCHABLE_DATA },
  { "launchable-data", no_argument, NULL, OPTION_LAUNCHABLE_DATA },

  { "help", no_argument, NULL, OPTION_HELP },
  { "version", no_argument, NULL, OPTION_VERSION },
  { NULL, no_argument, NULL, 0 }
  };


typedef map<ResKey, string> ResourceProvenance;

static ResourceDatabase db;
static ResourceProvenance prov;
static struct binary_file_info bininfo;


void
add_resource (const char* origin, const ResKey& key, const Datablock& data) {
  ResourceProvenance::const_iterator prev_supplier = prov.find (key);
  if (prev_supplier == prov.end()) {
    db[key] = data;
    prov[key] = origin;
    }
  else {
    filename = origin;
    einfo (E_FILE | E_WARNING, "resource %.4s #%u already obtained from `%s'",
	   key.type, key.id, (*prev_supplier).second.c_str());
    }
  }


Datablock
slurp_file_as_datablock (const char* fname) {
#ifdef HAVE_FILE_LENGTH
  FILE* f = fopen (fname, "rb");
  if (f) {
    long length = file_length (f);
    Datablock block (length);
    if (fread (block.writable_contents(), 1, length, f) != size_t(length))
      einfo (E_NOFILE | E_PERROR, "error reading `%s'", fname);
    fclose (f);
    return block;
    }
  else {
    einfo (E_NOFILE | E_PERROR, "can't open `%s'", fname);
    return Datablock();
    }
#else
  long length;
  void* buffer = slurp_file (fname, "rb", &length);

  if (buffer) {
    Datablock block (length);
    memcpy (block.writable_contents(), buffer, length);
    free (buffer);
    return block;
    }
  else {
    einfo (E_NOFILE | E_PERROR, "can't read raw file `%s'", fname);
    return Datablock();
    }
#endif
  }


/* The aim of this priority mechanism is to correctly initialise settings
   from default values, the definition file, the old-style command line,
   and command line options, with settings from the latter overriding the
   former.  The following state is under the control of this mechanism:

	db	name, type, creator, all 9 attributes, version, modnum
	bininfo	maincode, emit_appl_extras, emit_data, stack_size  */

enum priority_level {
  default_pri, def_default_pri, def_specific_pri, old_cli_pri, option_pri
  };

static map<const void*, priority_level> priority;

template <class T> static bool
superior (T& key, priority_level pri) {
  if (pri >= priority[&key]) {
    priority[&key] = pri;
    return true;
    }
  else
    return false;
  }

static bool
set_db_kind (database_kind kind, priority_level pri) {
  ResKey maincode;
  bool emit_appl_extras, emit_data;

  switch (kind) {
  case DK_APPLICATION:
    maincode = ResKey ("code", 1);
    emit_appl_extras = true;
    emit_data = true;
    break;

  case DK_GLIB:
    maincode = ResKey ("GLib", 0);
    emit_appl_extras = false;
    emit_data = true;
    break;

  case DK_SYSLIB:
    maincode = ResKey ("libr", 0);
    emit_appl_extras = false;
    emit_data = false;
    break;

  case DK_HACK:
    maincode = ResKey ("code", 1000);
    emit_appl_extras = false;
    emit_data = false;
    break;

  case DK_GENERIC:
  default:
    maincode = ResKey ("code", 0);
    emit_appl_extras = false;
    emit_data = false;
    break;
    }

  if (superior (bininfo.emit_appl_extras, pri))
    bininfo.emit_appl_extras = emit_appl_extras;
  if (superior (bininfo.emit_data, pri))
    bininfo.emit_data = emit_data;
  if (superior (bininfo.maincode, pri)) {
    bininfo.maincode = maincode;
    return true;
    }
  else
    return false;
  }

// I'm not sure I really understand how to do this, but we'll try:
extern "C" {

static void
db_header (database_kind kind, const struct database_header* h) {
  if (!set_db_kind (kind, def_default_pri))
    einfo (E_NOFILE, "`-l' and `-L' options conflict with definition file");

  if (superior (db.name, def_default_pri))
    strncpy (db.name, h->name, 32);
  if (superior (db.type, def_default_pri))
    strncpy (db.type, h->type, 4);
  if (superior (db.creator, def_default_pri))
    strncpy (db.creator, h->creator, 4);

  if (superior (db.readonly, def_default_pri))
    db.readonly = h->readonly;
  if (superior (db.appinfo_dirty, def_default_pri))
    db.appinfo_dirty = h->appinfo_dirty;
  if (superior (db.backup, def_default_pri))
    db.backup = h->backup;
  if (superior (db.ok_to_install_newer, def_default_pri))
    db.ok_to_install_newer = h->ok_to_install_newer;
  if (superior (db.reset_after_install, def_default_pri))
    db.reset_after_install = h->reset_after_install;
  if (superior (db.copy_prevention, def_default_pri))
    db.copy_prevention = h->copy_prevention;
  if (superior (db.stream, def_default_pri))
    db.stream = h->stream;
  if (superior (db.hidden, def_default_pri))
    db.hidden = h->hidden;
  if (superior (db.launchable_data, def_default_pri))
    db.launchable_data = h->launchable_data;

  if (superior (db.version, def_default_pri))
    db.version = h->version;
  if (superior (db.modnum, def_default_pri))
    db.modnum = h->modnum;
  }

static void
multicode_section (const char* secname) {
  unsigned int id = 2 + bininfo.extracode.size();

  if (bininfo.extracode.find (secname) == bininfo.extracode.end())
    bininfo.extracode[secname] = ResKey ("code", id);
  else
    einfo (E_FILE, "section `%s' duplicated", secname);
  }

static void
data (int allow) {
  if (superior (bininfo.emit_data, def_specific_pri))
    bininfo.emit_data = allow;
  }

static void
stack (unsigned long stack_size) {
  if (superior (bininfo.stack_size, def_specific_pri))
    bininfo.stack_size = stack_size;
  }

static void
trap (unsigned int resid, unsigned int vector, const char* fname) {
  Datablock res (2);
  unsigned char* s = res.writable_contents();
  put_word (s, vector);
  add_resource (filename, ResKey ("trap", resid), res);

  if (fname) {

    }
  else {
    }
  }

static void
version_resource (unsigned long resid, const char* text) {
  // tver resources are null-terminated
  long size = strlen (text) + 1;
  Datablock block (size);
  memcpy (block.writable_contents(), text, size);
  add_resource (filename, // the filename of the .def file
		ResKey ("tver", resid), block);
  }

}

int
main (int argc, char** argv) {
  int c, longind;
  bool work_desired = true;

  char* output_fname = NULL;

  progname = argv[0];

  set_db_kind (DK_APPLICATION, default_pri);
  init_database_header (&db);
  strncpy (db.type, "appl", 4);

  bininfo.stack_size = 4096;
  bininfo.force_rloc = false;
  bininfo.data_compression = 0;

  while ((c = getopt_long (argc, argv, shortopts, longopts, &longind)) != -1)
    switch (c) {
    case 'o':
      output_fname = optarg;
      break;

    case 'l':
      if (superior (db.type, option_pri))  strncpy (db.type, "GLib", 4);
      set_db_kind (DK_GLIB, option_pri);
      break;

    case 'L':
      if (superior (db.type, option_pri))  strncpy (db.type, "libr", 4);
      set_db_kind (DK_SYSLIB, option_pri);
      break;

    case 'a':
      db.appinfo = slurp_file_as_datablock (optarg);
      break;

    case 's':
      db.sortinfo = slurp_file_as_datablock (optarg);
      break;

    case 't':
      if (superior (db.type, option_pri))  strncpy (db.type, optarg, 4);
      break;

    case 'c':
      if (superior (db.creator, option_pri))  strncpy (db.creator, optarg, 4);
      break;

    case 'n':
      if (superior (db.name, option_pri))  strncpy (db.name, optarg, 32);
      break;

    case 'm':
      if (superior (db.modnum, option_pri))
	db.modnum = strtoul (optarg, NULL, 0);
      break;

    case 'v':
      if (superior (db.version, option_pri))
	db.version = strtoul (optarg, NULL, 0);
      break;

    case 'z':
      bininfo.data_compression = strtoul (optarg, NULL, 0);
      break;

    case OPTION_READONLY:
      if (superior (db.readonly, option_pri))  db.readonly = true;
      break;

    case OPTION_APPINFO_DIRTY:
      if (superior (db.appinfo_dirty, option_pri))  db.appinfo_dirty = true;
      break;

    case OPTION_BACKUP:
      if (superior (db.backup, option_pri))  db.backup = true;
      break;

    case OPTION_OK_TO_INSTALL_NEWER:
      if (superior (db.ok_to_install_newer, option_pri))
	db.ok_to_install_newer = true;
      break;

    case OPTION_RESET_AFTER_INSTALL:
      if (superior (db.reset_after_install, option_pri))
	db.reset_after_install = true;
      break;

    case OPTION_COPY_PREVENTION:
      if (superior (db.copy_prevention, option_pri))  db.copy_prevention = true;
      break;

    case OPTION_STREAM:
      if (superior (db.stream, option_pri))  db.stream = true;
      break;

    case OPTION_HIDDEN:
      if (superior (db.hidden, option_pri))  db.hidden = true;
      break;

    case OPTION_LAUNCHABLE_DATA:
      if (superior (db.launchable_data, option_pri))  db.launchable_data = true;
      break;

    case OPTION_HELP:
      usage();
      work_desired = false;
      break;

    case OPTION_VERSION:
      printf ("%s version %s\n", progname, version);
      work_desired = false;
      break;
      }

  if (!work_desired)
    return EXIT_SUCCESS;

  if (optind >= argc) {
    usage();
    return EXIT_FAILURE;
    }

  enum file_type first = file_type (argv[optind]);

  if (first == FT_PRC && !output_fname) {  // Old-style arguments
    if (argc - optind >= 3) {
      output_fname = argv[optind++];

      if (superior (db.name, old_cli_pri))
	strncpy (db.name, argv[optind], 32);
      else
	einfo (E_NOFILE, "`-n' option conflicts with old-style arguments");
      optind++;

      if (superior (db.creator, old_cli_pri))
	strncpy (db.creator, argv[optind], 4);
      else
	einfo (E_NOFILE, "`-c' option conflicts with old-style arguments");
      optind++;
      }
    else {
      usage();
      nerrors++;
      }
    }
  else {  // New-style arguments
    if (!output_fname) {
      static char fname[FILENAME_MAX];
      strcpy (fname, argv[optind]);
      output_fname = basename_with_changed_extension (fname, ".prc");
      }
    }

  if (first == FT_DEF) {
    struct def_callbacks def_funcs = default_def_callbacks;
    def_funcs.db_header = db_header;
    def_funcs.multicode_section = multicode_section;
    def_funcs.data = data;
    def_funcs.stack = stack;
    def_funcs.trap = trap;
    def_funcs.version_resource = version_resource;
    read_def_file (argv[optind++], &def_funcs);
    }

  if (nerrors)
    return EXIT_FAILURE;

  for (int i = optind; i < argc; i++)
    switch (file_type (argv[i])) {
    case FT_DEF:
      einfo (E_NOFILE,
	     (first == FT_DEF)? "only one definition file may be used"
			      : "the definition file must come first");
      break;

    case FT_RAW: {
      char buffer[FILENAME_MAX];
      strcpy (buffer, argv[i]);
      char *key = basename_with_changed_extension (buffer, NULL);
      char *dot = strchr (key, '.');
      /* A dot in the first four characters might just possibly be a very
	 strange resource type.  Any beyond there are extensions, which
	 we'll remove.  */
      if (dot && dot-key >= 4)  *dot = '\0';
      if (strlen (key) < 8) {
	while (strlen (key) < 4)  strcat (key, "x");
	while (strlen (key) < 8)  strcat (key, "0");
	filename = argv[i];
	einfo (E_FILE | E_WARNING,
	       "raw filename doesn't start with `typeNNNN'; treated as `%s'",
	       key);
	}
      else
	key[8] = '\0';  /* Ensure strtoul() doesn't get any extra digits.  */

      add_resource (argv[i], ResKey (key, strtoul (&key[4], NULL, 16)),
		    slurp_file_as_datablock (argv[i]));
      }
      break;

    case FT_BFD: {
      ResourceDatabase bfd_db = process_binary_file (argv[i], bininfo);
      for (ResourceDatabase::const_iterator it = bfd_db.begin();
	   it != bfd_db.end();
	   ++it)
	add_resource (argv[i], (*it).first, (*it).second);
      }
      break;

    case FT_PRC: {
      Datablock block = slurp_file_as_datablock (argv[i]);
      ResourceDatabase prc_db (block);
      for (ResourceDatabase::const_iterator it = prc_db.begin();
	   it != prc_db.end();
	   ++it)
	add_resource (argv[i], (*it).first, (*it).second);
      }
      break;
      }

  if (nerrors == 0) {
    if (db.name[0] == '\0')
      einfo (E_NOFILE | E_WARNING,
	     "creating `%s' without a name", output_fname);
    if (db.creator[0] == '\0')
      einfo (E_NOFILE | E_WARNING,
	     "creating `%s' without a creator id", output_fname);

    FILE* f = fopen (output_fname, "wb");
    if (f) {
      time_t now = time (NULL);
      struct tm* now_tm = localtime (&now);
      db.created = db.modified = *now_tm;
      if (db.write (f))
	fclose (f);
      else {
	einfo (E_NOFILE | E_PERROR, "error writing to `%s'", output_fname);
	fclose (f);
	remove (output_fname);
	}
      }
    else
      einfo (E_NOFILE | E_PERROR, "can't write to `%s'", output_fname);
    }

  return (nerrors == 0)? EXIT_SUCCESS : EXIT_FAILURE;
  }
