/* fileman.c  -  The GNU Privacy Assistant
 *	Copyright (C) 2000, 2001 G-N-U GmbH.
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

/*
 *	The file encryption/decryption/sign window
 */

#include "gpa.h"
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "argparse.h"
#include "stringhelp.h"

#include "gpapastrings.h"

#include "gtktools.h"
#include "gpawidgets.h"
#include "siglist.h"
#include "optionsmenu.h"
#include "helpmenu.h"
#include "icons.h"
#include "fileman.h"
#include "filesigndlg.h"
#include "encryptdlg.h"

struct _GPAFileManager {
  GtkWidget *window;
  GtkCList *clist_files;
};
typedef struct _GPAFileManager GPAFileManager;

/*
 *	File manager methods
 */

static void
fileman_destroy (gpointer param)
{
  GPAFileManager * fileman = param;

  free (fileman);
}

/* Return the currently selected files as a new list of filenames
 * structs. The list has to be freed by the caller, but the texts themselves
 * are still managed by the CList
 */
static GList *
get_selected_files (GtkCList *clist)
{
  GList *files = NULL;
  GList *selection = clist->selection;
  gint row;
  gchar *filename;

  while (selection)
    {
      row = GPOINTER_TO_INT (selection->data);
      gtk_clist_get_text (clist, row, 0, &filename);
      files = g_list_prepend (files, filename);
      selection = g_list_next (selection);
    }

  return files;
}


/* Add file filename to the clist. Return the index of the new row */
static gint
add_file (GPAFileManager *fileman, gchar *filename)
{
  gchar *entries[1];
  gchar *tmp;
  gint row;

  for (row = 0; row < fileman->clist_files->rows; row++)
    {
      gtk_clist_get_text (fileman->clist_files, row, 0, &tmp);
      if (g_str_equal (filename, tmp))
	{
	  gpa_window_error (_("The file is already open."), fileman->window);
	  return -1;
	}
    }
  entries[0] = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
  /* FIXME: Add the file status when/if gpgme supports it */

  row = gtk_clist_append (fileman->clist_files, entries);

  return row;
}


/*
 *	Callbacks
 */

/*
 *  File/Open
 */
static void
open_file (gpointer param)
{
  GPAFileManager * fileman = param;
  gchar * filename;
  gint row;

  filename = gpa_get_load_file_name (fileman->window, _("Open File"), NULL);
  if (filename)
    {
      row = add_file (fileman, filename);
      if (row >= 0)
	gtk_clist_select_row (fileman->clist_files, row, 0);
      free (filename);
    }
}

/*
 *  Verify Signed Files
 */


static void
verify_files (gpointer param)
{
}


/*
 * Sign Files
 */

static void
sign_files (gpointer param)
{
  GPAFileManager *fileman = param;
  GList * files;
  GList *signed_files, *cur;
  gint row;
  
  files = get_selected_files (fileman->clist_files);
  if (!files)
    return;

  signed_files = gpa_file_sign_dialog_run (fileman->window, files);
  if (signed_files)
    {
      gtk_clist_unselect_all (fileman->clist_files);
      cur = signed_files;
      while (cur)
	{
	  row = add_file (fileman, (gchar*)(cur->data));
	  if (row >= 0)
	    gtk_clist_select_row (fileman->clist_files, row, 0);
	  g_free (cur->data);
	  cur = g_list_next (cur);
	}
      g_list_free (signed_files);
    }
  g_list_free (files);
}

/*
 * Encrypt Files
 */

static void
encrypt_files (gpointer param)
{
  GPAFileManager *fileman = param;
  GList * files;
  GList *encrypted_files, *cur;
  gint row;

  files = get_selected_files (fileman->clist_files);
  if (!files)
    return;

  encrypted_files = gpa_file_encrypt_dialog_run (fileman->window, files);
  if (encrypted_files)
    {
      gtk_clist_unselect_all (fileman->clist_files);
      cur = encrypted_files;
      while (cur)
	{
	  row = add_file (fileman, (gchar*)(cur->data));
	  if (row >= 0)
	    gtk_clist_select_row (fileman->clist_files, row, 0);
	  g_free (cur->data);
	  cur = g_list_next (cur);
	}
      g_list_free (encrypted_files);
    }

  g_list_free (files);
}

/*
 * Decrypt Files
 */

static void
decrypt_files (gpointer param)
{
  GPAFileManager *fileman = param;
  GList * files;
  GList * cur;

  files = get_selected_files (fileman->clist_files);
  if (!files)
    return;

  for (cur = files; cur; cur = g_list_next (cur))
    {
      GpgmeError err;
      GpgmeData cipher, plain;
      const gchar *filename = cur->data;
      const gchar *extension;
      gchar *plain_filename;

      /* Find out the destination file */
      extension = g_strrstr (filename, ".");
      if (extension && (g_str_equal (extension, ".asc") || 
			g_str_equal (extension, ".gpg") ||
			g_str_equal (extension, ".pgp")))
	{
	  /* Remove the extension */
	  plain_filename = g_strdup (filename);
	  *(plain_filename + (extension-filename)) = '\0';
	}
      else
	{
	  plain_filename = g_strconcat (filename, ".txt");
	}
      /* Open the ciphertext */
      err = gpgme_data_new_from_file (&cipher, filename, 1);
      if (err == GPGME_File_Error)
	{
	  gchar *message;
	  message = g_strdup_printf ("%s: %s", filename, strerror(errno));
	  gpa_window_error (message, fileman->window);
	  g_free (message);
	  g_free (plain_filename);
	  break;
	}
      else if (err != GPGME_No_Error)
	{
	  gpa_gpgme_error (err);
	}
      /* Create a GpgmeData for the output */
      err = gpgme_data_new (&plain);
      if (err != GPGME_No_Error)
	{
	  gpa_gpgme_error (err);
	}
      /* Decrypt */
      err = gpgme_op_decrypt (ctx, cipher, plain);
      if (err == GPGME_No_Passphrase)
	{
	  gpa_window_error (_("Wrong passphrase!"), fileman->window);
	  break;
	}
      else if (err == GPGME_Canceled)
	{
	  break;
	}
      else if (err == GPGME_No_Data || err == GPGME_Decryption_Failed)
	{
	  gchar *message = g_strdup_printf (_("The file %s contained no valid"
					      " encrypted data."), filename);
	  gpa_window_error (message, fileman->window);
	}
      else if (err != GPGME_No_Error)
	{
	  gpa_gpgme_error (err);
	}
      else
	{
	  /* If everything went well, save the plaintext */
	  FILE *plain_file = gpa_fopen (plain_filename, fileman->window);
	  if (!plain_file)
	    {
	      gpgme_data_release (cipher);
	      gpgme_data_release (plain);
	      g_free (plain_filename);
	      break;
	    }
	  dump_data_to_file (plain, plain_file);
	  fclose (plain_file);
	  add_file (fileman, plain_filename);
	}
      gpgme_data_release (cipher);
      gpgme_data_release (plain);
      g_free (plain_filename);
    }

  g_list_free (files);
}


static void
close_window (gpointer param)
{
  GPAFileManager *fileman = param;
  gtk_widget_destroy (fileman->window);
}


/*
 *	Construct the file manager window
 */


static GtkWidget *
fileman_menu_new (GtkWidget * window, GPAFileManager *fileman)
{
  GtkItemFactory *factory;
  GtkItemFactoryEntry file_menu[] = {
    {_("/_File"), NULL, NULL, 0, "<Branch>"},
    {_("/File/_Open"), "<control>O", open_file, 0, NULL},
    {_("/File/sep1"), NULL, NULL, 0, "<Separator>"},
    {_("/File/_Sign"), NULL, sign_files, 0, NULL},
    {_("/File/C_heck"), "<control>P", verify_files, 0, NULL},
    {_("/File/_Encrypt"), NULL, encrypt_files, 0, NULL},
    /*    {_("/File/E_ncrypt as"), NULL, file_encryptAs, 0, NULL},
    {_("/File/_Protect by Password"), NULL, file_protect, 0, NULL},
    {_("/File/P_rotect as"), NULL, file_protectAs, 0, NULL},
    */
    {_("/File/_Decrypt"), NULL, decrypt_files, 0, NULL},
    /*
    {_("/File/Decrypt _as"), NULL, file_decryptAs, 0, NULL},
    */
    {_("/File/sep2"), NULL, NULL, 0, "<Separator>"},
    {_("/File/_Close"), NULL, close_window, 0, NULL},
    {_("/File/_Quit"), "<control>Q", gtk_main_quit, 0, NULL},
  };
  GtkItemFactoryEntry windows_menu[] = {
    {_("/_Windows"), NULL, NULL, 0, "<Branch>"},
    {_("/Windows/_Filemanager"), NULL, gpa_open_filemanager, 0, NULL},
    {_("/Windows/_Keyring Editor"), NULL, gpa_open_keyring_editor, 0, NULL},
  };
  GtkAccelGroup *accel_group;

  accel_group = gtk_accel_group_new ();
  factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
  gtk_item_factory_create_items (factory,
				 sizeof (file_menu) / sizeof (file_menu[0]),
				 file_menu, fileman);
  gpa_options_menu_add_to_factory (factory, window);
  gtk_item_factory_create_items (factory,
				 sizeof(windows_menu) /sizeof(windows_menu[0]),
				 windows_menu, fileman);
  gpa_help_menu_add_to_factory (factory, window);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  return gtk_item_factory_get_widget (factory, "<main>");
}

static GtkWidget *
gpa_window_file_new (GPAFileManager * fileman)
{
  char *clistFileTitle[] = {
    _("File")
  };
  int i;

  GtkWidget *windowFile;
  GtkWidget *scrollerFile;
  GtkWidget *clistFile;

  windowFile = gtk_frame_new (_("Files in work"));
  scrollerFile = gtk_scrolled_window_new (NULL, NULL);
  clistFile = gtk_clist_new_with_titles (1, clistFileTitle);
  fileman->clist_files = GTK_CLIST (clistFile);
  gtk_clist_set_column_justification (GTK_CLIST (clistFile), 1,
				      GTK_JUSTIFY_CENTER);
  for (i = 0; i <= 1; i++)
    {
      gtk_clist_set_column_auto_resize (GTK_CLIST (clistFile), i, FALSE);
      gtk_clist_column_title_passive (GTK_CLIST (clistFile), i);
    } /* for */
  gtk_clist_set_selection_mode (GTK_CLIST (clistFile),
				GTK_SELECTION_EXTENDED);
  gtk_widget_grab_focus (clistFile);
  gtk_container_add (GTK_CONTAINER (scrollerFile), clistFile);
  gtk_container_add (GTK_CONTAINER (windowFile), scrollerFile);

  return windowFile;
} /* gpa_window_file_new */


static void
toolbar_file_open (GtkWidget *widget, gpointer param)
{
  open_file (param);
}

static void
toolbar_file_sign (GtkWidget *widget, gpointer param)
{
  sign_files (param);
}

static void
toolbar_file_verify (GtkWidget *widget, gpointer param)
{
  verify_files (param);
}

static void
toolbar_file_encrypt (GtkWidget *widget, gpointer param)
{
  encrypt_files (param);
}

static void
toolbar_file_decrypt (GtkWidget *widget, gpointer param)
{
  decrypt_files (param);
}



static GtkWidget *
gpa_fileman_toolbar_new (GtkWidget * window, GPAFileManager *fileman)
{
  GtkWidget *toolbar, *icon;

#ifdef __NEW_GTK__
  toolbar = gtk_toolbar_new ();
#else
  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
#endif
  
  /* Open */
  if ((icon = gpa_create_icon_widget (window, "openfile")))
    gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), _("Open"),
                            _("Open a file"), _("open file"), icon,
                            GTK_SIGNAL_FUNC (toolbar_file_open), fileman);
  /* Sign */
  if ((icon = gpa_create_icon_widget (window, "sign")))
    gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), _("Sign"),
			     _("Sign the selected file"), _("sign file"), icon,
			     GTK_SIGNAL_FUNC (toolbar_file_sign), fileman);
  /* Verify */
  if ((icon = gpa_create_icon_widget (window, "verify")))
    gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), _("Verify"),
			     _("Check signatures of selected file"), _("verify file"), icon,
			     GTK_SIGNAL_FUNC (toolbar_file_verify), fileman);
  /* Encrypt */
  if ((icon = gpa_create_icon_widget (window, "encrypt")))
    gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), _("Encrypt"),
			     _("Encrypt the selected file"), _("encrypt file"),
			     icon, GTK_SIGNAL_FUNC (toolbar_file_encrypt),
			     fileman);
  /* Decrypt */
  if ((icon = gpa_create_icon_widget (window, "decrypt")))
    gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), _(" Decrypt "),
			     _("Decrypt the selected file"), _("decrypt file"),
			     icon, GTK_SIGNAL_FUNC (toolbar_file_decrypt),
			     fileman);

#if 0  /* FIXME: Help is not available yet. :-( */
  /* Help */
  if ((icon = gpa_create_icon_widget (window, "help")))
    gtk_toolbar_append_item (GTK_TOOLBAR (toolbar), _("Help"),
			     _("Understanding the GNU Privacy Assistant"),
			     _("help"), icon,
			     GTK_SIGNAL_FUNC (help_help), NULL);
#endif

  return toolbar;
} 

GtkWidget *
gpa_fileman_new ()
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *menubar;
  GtkWidget *fileBox;
  GtkWidget *windowFile;
  GtkWidget *toolbar;
  GPAFileManager * fileman;

  fileman = xmalloc (sizeof(GPAFileManager));

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window),
			_("GNU Privacy Assistant - File Manager"));
  gtk_widget_set_usize (window, 640, 480);
  gtk_object_set_data_full (GTK_OBJECT (window), "user_data", fileman,
			    fileman_destroy);
  fileman->window = window;
  /* Realize the window so that we can create pixmaps without warnings */
  gtk_widget_realize (window);

  vbox = gtk_vbox_new (FALSE, 0);
  menubar = fileman_menu_new (window, fileman);
  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);

  /* set up the toolbar */
  toolbar = gpa_fileman_toolbar_new(window, fileman);
  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, TRUE, 0);
  
  fileBox = gtk_hbox_new (TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (fileBox), 5);
  windowFile = gpa_window_file_new (fileman);
  gtk_box_pack_start (GTK_BOX (fileBox), windowFile, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (vbox), fileBox, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  /*  gpa_popupMenu_init ();
*/

  return window;
} /* gpa_fileman_new */

