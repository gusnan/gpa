/* kexdeletedlg.c - The GNU Privacy Assistant
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <config.h>

#include <gpgme.h>
#include <gtk/gtk.h>
#include "gpa.h"
#include "gtktools.h"
#include "gpawidgets.h"
#include "keydeletedlg.h"
#include "keytable.h"

/* Emit a last warning that a secret key is going to be deleted, and ask for
 * confirmation.
 */
static gboolean
confirm_delete_secret (GtkWidget * parent)
{
  GtkWidget * window;
  GtkWidget * label;
  GtkWidget * hbox;
  
  window = gtk_dialog_new_with_buttons (_("Removing Secret Key"),
                                        GTK_WINDOW(parent),
                                        GTK_DIALOG_MODAL,
                                        _("_Yes"),
                                        GTK_RESPONSE_YES,
                                        _("_No"),
                                        GTK_RESPONSE_NO,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (window), GTK_RESPONSE_NO);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

  GtkWidget *image_warning;
  image_warning = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_DIALOG);

  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (image_warning), TRUE, TRUE, 0);
  label = gtk_label_new (_("If you delete this key, you won't be able to\n"
                           "read messages encrypted with it.\n\n"
                           "Are you really sure you want to delete it?"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  GtkBox *box = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window)));

  gtk_box_pack_start(GTK_BOX (box), hbox, TRUE, TRUE, 0);
  gtk_widget_show_all (window);
  if (gtk_dialog_run (GTK_DIALOG (window)) == GTK_RESPONSE_YES)
    {
      gtk_widget_destroy (window);
      return TRUE;
    }
  else
    {
      gtk_widget_destroy (window);
      return FALSE;
    }
}

/* Run the delete key dialog as a modal dialog and return TRUE if the
 * user chose Yes, FALSE otherwise. Display information about the public
 * key key in the dialog so that the user knows which key is to be
 * deleted. If has_secret_key is true, display a special warning for
 * deleting secret keys.
 */
gboolean
gpa_delete_dialog_run (GtkWidget * parent, gpgme_key_t key)
{
  GtkWidget * window;
  GtkWidget * vbox;
  GtkWidget * label;
  GtkWidget * info;

  gboolean has_secret_key = (gpa_keytable_lookup_key 
			     (gpa_keytable_get_secret_instance(), 
			      key->subkeys->fpr) != NULL);

  window = gtk_dialog_new_with_buttons (_("Remove Key"), GTK_WINDOW(parent),
                                        GTK_DIALOG_MODAL,
                                        _("_Yes"),
                                        GTK_RESPONSE_YES,
                                        _("_No"),
                                        GTK_RESPONSE_NO,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (window), GTK_RESPONSE_YES);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);

  vbox = gtk_dialog_get_content_area (GTK_DIALOG (window));
  // vbox = GTK_DIALOG (window)->vbox;
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

  label = gtk_label_new (_("You have selected the following key "
			   "for removal:"));
  gtk_widget_set_halign (GTK_WIDGET (label), 0.0);
  gtk_widget_set_valign (GTK_WIDGET (label), 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);
  
  info = gpa_key_info_new (key);
  gtk_box_pack_start (GTK_BOX (vbox), info, TRUE, TRUE, 5);

  if (has_secret_key)
    {
      label = gtk_label_new (_("This key has a secret key."
			       " Deleting this key cannot be undone,"
			       " unless you have a backup copy."));
      gtk_widget_set_halign (GTK_WIDGET (label), 0.0);
      gtk_widget_set_valign (GTK_WIDGET (label), 0.5);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);
    }
  else
    {
      label = gtk_label_new (_("This key is a public key."
			       " Deleting this key cannot be undone easily,"
			       " although you may be able to get a new copy "
			       " from the owner or from a key server."));
      gtk_widget_set_halign (GTK_WIDGET (label), 0.0);
      gtk_widget_set_valign (GTK_WIDGET (label), 0.5);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);
    }
  
  label = gtk_label_new (_("Are you sure you want to delete this key?"));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);

  gtk_widget_show_all (window);

  if (gtk_dialog_run (GTK_DIALOG (window)) == GTK_RESPONSE_YES)
    {
      if (has_secret_key)
        {
          gboolean result = confirm_delete_secret (window);
          gtk_widget_destroy (window);
          return result;
        }
      else
        {
          gtk_widget_destroy (window);
          return TRUE;
        }
    }
  else
    {
      gtk_widget_destroy (window);
      return FALSE;
    }
} /* gpa_delete_dialog_run */
