/* options.h - global option declarations
 *	Copyright (C) 2002 Miguel Coca.
 *
 * This file is part of GPA
 *
 * GPA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GPA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <glib.h>
#include "options.h"
#include "gpa.h"
#include "gtktools.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/* Internal API */
static void gpa_options_save_settings (GpaOptions *options);
static void gpa_options_read_settings (GpaOptions *options);

/* GObject type functions */

static void gpa_options_init (GpaOptions *options);
static void gpa_options_class_init (GpaOptionsClass *klass);
static void gpa_options_finalize (GObject *object);

enum
{
  CHANGED_UI_MODE,
  CHANGED_DEFAULT_KEY,
  CHANGED_DEFAULT_KEYSERVER,
  CHANGED_BACKUP_GENERATED,
  LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;
static guint signals [LAST_SIGNAL] = { 0 };

GType
gpa_options_get_type (void)
{
  static GType style_type = 0;
  
  if (!style_type)
    {
      static const GTypeInfo style_info =
      {
        sizeof (GpaOptionsClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gpa_options_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GpaOptions),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gpa_options_init,
      };
      
      style_type = g_type_register_static (G_TYPE_OBJECT,
					   "GpaOptions",
					   &style_info, 0);
    }
  
  return style_type;
}

static void
gpa_options_class_init (GpaOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gpa_options_finalize;

  klass->changed_ui_mode = gpa_options_save_settings;
  klass->changed_default_key = gpa_options_save_settings;
  klass->changed_default_keyserver = gpa_options_save_settings;
  klass->changed_backup_generated = gpa_options_save_settings;

  /* Signals */
  signals[CHANGED_UI_MODE] =
          g_signal_new ("changed_ui_mode",
                        G_TYPE_FROM_CLASS (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GpaOptionsClass, changed_ui_mode),
                        NULL, NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);
  signals[CHANGED_DEFAULT_KEY] =
          g_signal_new ("changed_default_key",
                        G_TYPE_FROM_CLASS (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GpaOptionsClass, changed_default_key),
                        NULL, NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);
  signals[CHANGED_DEFAULT_KEYSERVER] =
          g_signal_new ("changed_default_keyserver",
                        G_TYPE_FROM_CLASS (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GpaOptionsClass,
                                         changed_default_keyserver),
                        NULL, NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);
  signals[CHANGED_BACKUP_GENERATED] =
          g_signal_new ("changed_backup_generated",
                        G_TYPE_FROM_CLASS (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GpaOptionsClass,
                                         changed_backup_generated),
                        NULL, NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);
}

static void
gpa_options_init (GpaOptions *options)
{
  options->options_file = NULL;
  options->simplified_ui = TRUE;
  options->backup_generated = FALSE;
  options->default_key = NULL;
  options->default_keyserver = NULL;
}

static void
gpa_options_finalize (GObject *object)
{
  GpaOptions *options = GPA_OPTIONS (object);
  
  g_free (options->options_file);
  g_free (options->default_key);
  g_free (options->default_keyserver);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* API */

/* Create a new GpaOptions object, reading the options, reading the options
 * from the file given */
GpaOptions *
gpa_options_new (const gchar *filename)
{
  GpaOptions *options;
  
  options = g_object_new (GPA_OPTIONS_TYPE, NULL);
  options->options_file = g_strdup (filename);
  gpa_options_read_settings (options);

  return options;
}

/* Set whether the ui should be in simplified mode */
void
gpa_options_set_simplified_ui (GpaOptions *options, gboolean value)
{
  options->simplified_ui = value;
  g_signal_emit (options, signals[CHANGED_UI_MODE], 0);
}

gboolean
gpa_options_get_simplified_ui (GpaOptions *options)
{
  return options->simplified_ui;
}

/* Choose the default key */
void
gpa_options_set_default_key (GpaOptions *options, const gchar *fpr)
{
  if (options->default_key)
    {
      g_free (options->default_key);
    }
  options->default_key = g_strdup (fpr);
  g_signal_emit (options, signals[CHANGED_DEFAULT_KEY], 0);
}

const gchar *
gpa_options_get_default_key (GpaOptions *options)
{
  return options->default_key;
}

/* Return the default key gpg would use, or at least a first
 * approximation. Currently this means the first secret key in the keyring.
 * If there's no secret key at all, return NULL
 */
static gchar *
determine_default_key (void)
{
  GpgmeKey key;
  GpgmeError err;
  GpgmeCtx ctx = gpa_gpgme_new ();
  gchar * fpr = NULL;
  err = gpgme_op_keylist_start (ctx, NULL, 1);
  if( err != GPGME_No_Error )
    gpa_gpgme_error (err);
  err = gpgme_op_keylist_next (ctx, &key);
  if( err == GPGME_No_Error )
    {
      /* Copy the ID and release the key, since it's no longer needed */
      fpr = g_strdup (gpgme_key_get_string_attr (key, GPGME_ATTR_FPR, 
                                                 NULL, 0 ));
      gpgme_key_release (key);
      err = gpgme_op_keylist_end (ctx);
      if( err != GPGME_No_Error )
        gpa_gpgme_error (err); 
    }
  else if( err == GPGME_EOF )
    /* No secret keys found */
    fpr = NULL;
  else
    gpa_gpgme_error (err);

  gpgme_release (ctx);
  return fpr;
}

void
gpa_options_update_default_key (GpaOptions *options)
{
  gboolean update = FALSE;
  GpgmeKey key = NULL;
  GpgmeCtx ctx = gpa_gpgme_new ();

  if (!options->default_key)
    {
      update = TRUE;
    }
  else if (gpgme_get_key (ctx, options->default_key, &key, TRUE, TRUE) == 
           GPGME_EOF)
    {
      gpa_window_error (_("The private key you selected as default is no "
                          "longer available.\n"
                          "GPA will try to choose a new default "
                          "key automatically."), NULL);
      update = TRUE;
    }

  if (update)
    {
      gchar *fpr = determine_default_key ();
      gpa_options_set_default_key (options, fpr);
      g_free (fpr);
    }

  if (key)
    {
      gpgme_key_unref (key);
    }

  gpgme_release (ctx);
}

/* Specify the default keyserver */
void
gpa_options_set_default_keyserver (GpaOptions *options, const gchar *keyserver)
{
  if (options->default_keyserver)
    {
      g_free (options->default_keyserver);
    }
  options->default_keyserver = g_strdup (keyserver);
  g_signal_emit (options, signals[CHANGED_DEFAULT_KEYSERVER], 0);
}

const gchar *
gpa_options_get_default_keyserver (GpaOptions *options)
{
  return options->default_keyserver;
}

/* Remember whether the default key has already been backed up */
void
gpa_options_set_backup_generated (GpaOptions *options, gboolean value)
{
  options->backup_generated = value;
  g_signal_emit (options, signals[CHANGED_BACKUP_GENERATED], 0);
}

gboolean
gpa_options_get_backup_generated (GpaOptions *options)
{
  return options->backup_generated;
}

/* Destroy the GpaOptions object */
void
gpa_options_destroy (GpaOptions *options)
{
  g_object_run_dispose (G_OBJECT (options));
}

static void
gpa_options_save_settings (GpaOptions *options)
{
  FILE *options_file;
  
  g_assert(options->options_file != NULL);
  options_file =  fopen (options->options_file, "w");
  if (!options_file)
    {
      g_warning("%s: %s", options->options_file, strerror (errno));
    }
  else
    {
      if (options->default_key)
        {
          fprintf (options_file, "default-key %s\n", options->default_key);
        }
      if (options->default_keyserver)
        {
          fprintf (options_file, "keyserver %s\n", 
                   options->default_keyserver);
        }
      if (options->backup_generated)
        {
          fprintf (options_file, "%s\n", "backup-generated");
        }
      if (!options->simplified_ui)
        {
          fprintf (options_file, "%s\n", "advanced-ui");
        }
      fclose (options_file);
    }
}

static gboolean
read_next_word (FILE *file, char *buffer, int size)
{
  int i = 0;
  int c;

  buffer[0] = '\0';
  /* Skip leading whitespace */
  while ((c = getc (file)) != EOF && isspace (c))
    ;
  if (c == EOF)
    {
      return FALSE;
    }
  else
    {
      buffer[i++] = c;
      /* Read the word */
      while ((c = getc (file)) != EOF && !isspace (c) && i < (size-1))
        {
          buffer[i++] = c;
        }
      buffer[i] = '\0';
      
      return TRUE;
    }
}

typedef enum 
 {
   PARSE_OPTIONS_STATE_START,
   PARSE_OPTIONS_STATE_HAVE_KEY,
   PARSE_OPTIONS_STATE_HAVE_KEYSERVER,
 } ParseOptionsState;

/* This MUST be called ONLY from gpa_options_new (). We don't emit any
 * signals when changing options here (it would overwrite the options file) */
static void
gpa_options_read_settings (GpaOptions *options)
{
  FILE *options_file;
  
  g_assert(options->options_file != NULL);
  options_file =  fopen (options->options_file, "r");
  if (!options_file)
    {
      /* If the config file just doesn't exist, it's not an error */
      if (errno != ENOENT)
        {
          g_warning("%s: %s", options->options_file, strerror (errno));
        }
    }
  else
    {
      /* Parse the file */
      gchar next_word[100];
      ParseOptionsState state = PARSE_OPTIONS_STATE_START;

      /* There is no error checking here intentionally. This way we won't
       * complain too much about mixing GPA versions */
      while (read_next_word (options_file, next_word, sizeof (next_word)))
        {
          switch (state)
            {
            case PARSE_OPTIONS_STATE_START:
              if (g_str_equal (next_word, "default-key"))
                {
                  state = PARSE_OPTIONS_STATE_HAVE_KEY;
                }
              else if (g_str_equal (next_word, "keyserver"))
                {
                  state = PARSE_OPTIONS_STATE_HAVE_KEYSERVER;
                }
              else if (g_str_equal (next_word, "backup-generated"))
                {
                  options->backup_generated = TRUE;
                }
              else if (g_str_equal (next_word, "advanced-ui"))
                {
                  options->simplified_ui = FALSE;
                }
              break;
            case PARSE_OPTIONS_STATE_HAVE_KEY:
              options->default_key = g_strdup (next_word);
              state = PARSE_OPTIONS_STATE_START;
              break;
            case PARSE_OPTIONS_STATE_HAVE_KEYSERVER:
              options->default_keyserver = g_strdup (next_word);
              state = PARSE_OPTIONS_STATE_START;
              break;
            default:
              /* Can't happen */
              return;
            }
        }
      fclose (options_file);
    }
}
