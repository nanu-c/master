// rfx-builder.c
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <errno.h>

#include "main.h"
#include "rfx-builder.h"
#include "support.h"
#include "interface.h"
#include "paramwindow.h"
#include "effects.h"

static GtkWidget *copy_script_okbutton;

void
on_new_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  rfx_build_window_t *rfxbuilder;

  if (!check_builder_programs()) return;
  rfxbuilder=make_rfx_build_window(NULL,RFX_STATUS_ANY);
  gtk_widget_show (rfxbuilder->dialog);
}

void
on_edit_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  rfx_build_window_t *rfxbuilder;
  gchar *script_name;
  gshort status=GPOINTER_TO_INT (user_data);

  if (!check_builder_programs()) return;
  if (status!=RFX_STATUS_TEST) return; // we only edit test effects currently

  if ((script_name=prompt_for_script_name (NULL,RFX_STATUS_TEST))==NULL) return;
  if (!strlen (script_name)) {
    g_free (script_name);
    return;
  }
  if ((rfxbuilder=make_rfx_build_window(script_name,RFX_STATUS_TEST))==NULL) {
    g_free (script_name);
    return; // script not loaded
  }
  g_free (script_name);
  rfxbuilder->mode=RFX_BUILDER_MODE_EDIT;
  rfxbuilder->oname=g_strdup (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->name_entry)));
  gtk_widget_show (rfxbuilder->dialog);
}


void
on_copy_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gshort status=GPOINTER_TO_INT (user_data);

  if (!check_builder_programs()) return;
  if (status!=RFX_STATUS_TEST) return; // we only copy to test effects currently

  // prompt_for_script_name will create our builder window from the input
  // script and set the name : note, we have no rfxbuilder reference

  // TODO - use RFXBUILDER_MODE-*
  prompt_for_script_name (NULL,RFX_STATUS_COPY);
}


void
on_rename_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gshort status=GPOINTER_TO_INT (user_data);

  if (status!=RFX_STATUS_TEST) return; // we only copy to test effects currently

  prompt_for_script_name (NULL,RFX_STATUS_RENAME);
}





rfx_build_window_t *make_rfx_build_window (const gchar *script_name, lives_rfx_status_t status) {
  // TODO - set mnemonic widgets for entries

  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *hseparator;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;
  GtkWidget *scrollw;
  GtkWidget *top_vbox;
  GObject *spinbutton_adj;

  GSList *radiobutton_type_group = NULL;
  GList *langc=NULL;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)g_malloc (sizeof(rfx_build_window_t));

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  gchar *tmp,*tmp2;

  rfxbuilder->rfx_version=g_strdup(RFX_VERSION);

  rfxbuilder->type=RFX_BUILD_TYPE_EFFECT1; // the default

  rfxbuilder->num_reqs=0;
  rfxbuilder->num_params=0;
  rfxbuilder->num_paramw_hints=0;

  rfxbuilder->num_triggers=0;

  rfxbuilder->has_init_trigger=FALSE;
  rfxbuilder->props=0;

  rfxbuilder->plugin_version=1;

  rfxbuilder->pre_code=g_strdup ("");
  rfxbuilder->loop_code=g_strdup ("");
  rfxbuilder->post_code=g_strdup ("");

  rfxbuilder->field_delim=g_strdup ("|");

  rfxbuilder->script_name=NULL;  // use default name.script
  rfxbuilder->oname=NULL;

  rfxbuilder->mode=RFX_BUILDER_MODE_NEW;

  rfxbuilder->table_swap_row1=-1;

  /////////////////////////////////////////////////////////


  rfxbuilder->dialog = gtk_dialog_new ();
  if (script_name==NULL) {
    gtk_window_set_title (GTK_WINDOW (rfxbuilder->dialog), _("LiVES: - New Test RFX"));
  }
  else {
    gtk_window_set_title (GTK_WINDOW (rfxbuilder->dialog), _("LiVES: - Edit Test RFX"));
  }
  gtk_window_set_position (GTK_WINDOW (rfxbuilder->dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (rfxbuilder->dialog), TRUE);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(rfxbuilder->dialog),GTK_WINDOW(mainw->LiVES));
  }
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(rfxbuilder->dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(rfxbuilder->dialog),FALSE);
  }

  gtk_window_add_accel_group (GTK_WINDOW (rfxbuilder->dialog), accel_group);

  gtk_container_set_border_width (GTK_CONTAINER (rfxbuilder->dialog), 8);
  gtk_window_set_default_size (GTK_WINDOW (rfxbuilder->dialog), (PREF_RFXDIALOG_W<mainw->scr_width-20)?
			       PREF_RFXDIALOG_W:mainw->scr_width-20, (PREF_RFXDIALOG_H<mainw->scr_height-60)?
			       PREF_RFXDIALOG_H:mainw->scr_height-60);

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(rfxbuilder->dialog));
  gtk_box_set_spacing (GTK_BOX (dialog_vbox), 8);
  gtk_widget_show (dialog_vbox);

  top_vbox = gtk_vbox_new (FALSE, 10);
  gtk_widget_show (top_vbox);


  scrollw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrollw);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), scrollw, TRUE, TRUE, 0);

   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollw), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrollw), 
					 top_vbox);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(scrollw)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(scrollw)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }







  // types
  hbox = gtk_hbox_new (FALSE, 20);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (top_vbox), hbox, TRUE, TRUE, 0);


  label = gtk_label_new (_("Type:"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->type_effect1_radiobutton = gtk_radio_button_new (NULL);
  rfxbuilder->type_effect2_radiobutton = gtk_radio_button_new (NULL);
  rfxbuilder->type_effect0_radiobutton = gtk_radio_button_new (NULL);
  rfxbuilder->type_tool_radiobutton = gtk_radio_button_new (NULL);
  rfxbuilder->type_utility_radiobutton = gtk_radio_button_new (NULL);

  label = gtk_label_new (_("Effect -"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (rfxbuilder->type_effect1_radiobutton);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->type_effect1_radiobutton, TRUE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (rfxbuilder->type_effect1_radiobutton), radiobutton_type_group);
  radiobutton_type_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rfxbuilder->type_effect1_radiobutton));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rfxbuilder->type_effect1_radiobutton), TRUE);


  label = gtk_label_new (_("Transition -"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (rfxbuilder->type_effect2_radiobutton);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->type_effect2_radiobutton, TRUE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (rfxbuilder->type_effect2_radiobutton), radiobutton_type_group);
  radiobutton_type_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rfxbuilder->type_effect2_radiobutton));


  label = gtk_label_new (_("Generator -"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (rfxbuilder->type_effect0_radiobutton);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->type_effect0_radiobutton, TRUE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (rfxbuilder->type_effect0_radiobutton), radiobutton_type_group);
  radiobutton_type_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rfxbuilder->type_effect0_radiobutton));

  label = gtk_label_new (_("Tool -"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (rfxbuilder->type_tool_radiobutton);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->type_tool_radiobutton, TRUE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (rfxbuilder->type_tool_radiobutton), radiobutton_type_group);
  radiobutton_type_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rfxbuilder->type_tool_radiobutton));


  label = gtk_label_new (_("Utility -"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (rfxbuilder->type_utility_radiobutton);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->type_utility_radiobutton, TRUE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (rfxbuilder->type_utility_radiobutton), radiobutton_type_group);
  radiobutton_type_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rfxbuilder->type_utility_radiobutton));

  // name

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (top_vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new (_("Name:          "));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->name_entry = gtk_entry_new ();
  gtk_widget_show (rfxbuilder->name_entry);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->name_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->name_entry,(_ ("The name of the plugin. No spaces allowed.")));

  // version

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (top_vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new (_("Version:       "));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new (rfxbuilder->plugin_version, rfxbuilder->plugin_version, 1000000., 1., 1., 0.);
  rfxbuilder->spinbutton_version = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);

  gtk_widget_show (rfxbuilder->spinbutton_version);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->spinbutton_version, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->spinbutton_version,(_ ("The script version.")));

  // author

  label = gtk_label_new (_("    Author:       "));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->author_entry = gtk_entry_new ();
  gtk_widget_show (rfxbuilder->author_entry);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->author_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->author_entry,(_ ("The script author.")));


  // URL

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (top_vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new (_("    URL (optional):       "));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->url_entry = gtk_entry_new ();
  gtk_widget_show (rfxbuilder->url_entry);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->url_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->url_entry,(_ ("URL for the plugin maintainer.")));


  // menu entry

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (top_vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new (_("Menu text:    "));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->menu_text_entry = gtk_entry_new ();
  gtk_widget_show (rfxbuilder->menu_text_entry);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->menu_text_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->menu_text_entry,(_ ("The text to show in the menu.")));


  rfxbuilder->action_desc_hsep = gtk_hseparator_new ();
  gtk_widget_show (rfxbuilder->action_desc_hsep);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->action_desc_hsep, FALSE, TRUE, 0);


  // action description

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (top_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->action_desc_label = gtk_label_new (_("Action description:        "));
  gtk_widget_show (rfxbuilder->action_desc_label);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->action_desc_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->action_desc_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->action_desc_entry = gtk_entry_new ();
  gtk_widget_show (rfxbuilder->action_desc_entry);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->action_desc_entry, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->action_desc_entry,
			(_ ("Describe what the plugin is doing. E.g. \"Edge detecting\"")));

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (top_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->min_frames_label = gtk_label_new (_("Minimum frames:"));
  gtk_widget_show (rfxbuilder->min_frames_label);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->min_frames_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->min_frames_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new (1., 1., 1000., 1., 1., 0);
  rfxbuilder->spinbutton_min_frames = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (rfxbuilder->spinbutton_min_frames);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->spinbutton_min_frames, TRUE, FALSE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->spinbutton_min_frames,
			(_ ("Minimum number of frames this effect/tool can be applied to. Normally 1.")));

  // requirements


  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  rfxbuilder->requirements_button=gtk_button_new_with_mnemonic (_ ("_Requirements..."));
  gtk_widget_show (rfxbuilder->requirements_button);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->requirements_button, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->requirements_button,
			(_ ("Enter any binaries required by the plugin.")));

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  rfxbuilder->properties_button=gtk_button_new_with_mnemonic (_ ("_Properties..."));
  gtk_widget_show (rfxbuilder->properties_button);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->properties_button, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->properties_button,
			(_ ("Set properties for the plugin. Optional.")));

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  rfxbuilder->params_button=gtk_button_new_with_mnemonic (_ ("_Parameters..."));
  gtk_widget_show (rfxbuilder->params_button);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->params_button, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->params_button,
			(_ ("Set up parameters used in pre/loop/post/trigger code. Optional.")));

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  rfxbuilder->param_window_button=gtk_button_new_with_mnemonic (_ ("Parameter _Window Hints..."));
  gtk_widget_show (rfxbuilder->param_window_button);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->param_window_button, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->param_window_button,
			(_ ("Set hints about how to lay out the parameter window. Optional.")));

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  langc = g_list_append (langc, (gpointer)"0xF0 - LiVES-Perl");

  rfxbuilder->langc_combo = lives_standard_combo_new ((tmp=g_strdup(_("_Language code:"))),TRUE,langc,LIVES_BOX(top_vbox),
						      (tmp2=g_strdup(_ ("Language for pre/loop/post/triggers. Optional."))));

  g_free(tmp);
  g_free(tmp2);
  g_list_free(langc);
  gtk_widget_show_all(gtk_widget_get_parent(rfxbuilder->langc_combo));

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  rfxbuilder->pre_button=gtk_button_new_with_mnemonic (_ ("_Pre loop code..."));
  gtk_widget_show (rfxbuilder->pre_button);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->pre_button, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->pre_button,
			(_ ("Code to be executed before the loop. Optional.")));

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  rfxbuilder->loop_button=gtk_button_new_with_mnemonic (_ ("_Loop code..."));
  gtk_widget_show (rfxbuilder->loop_button);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->loop_button, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->loop_button,
			(_ ("Loop code to be applied to each frame.")));

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  rfxbuilder->post_button=gtk_button_new_with_mnemonic (_ ("_Post loop code..."));
  gtk_widget_show (rfxbuilder->post_button);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->post_button, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->post_button,
			(_ ("Code to be executed after the loop. Optional.")));

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (top_vbox), hseparator, FALSE, TRUE, 0);

  rfxbuilder->trigger_button=gtk_button_new_with_mnemonic (_ ("_Trigger code..."));
  gtk_widget_show (rfxbuilder->trigger_button);
  gtk_box_pack_start (GTK_BOX (top_vbox), rfxbuilder->trigger_button, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text( rfxbuilder->trigger_button,
			(_ ("Set trigger code for when the parameter window is shown, or when a parameter is changed. Optional (except for Utilities).")));


  dialog_action_area = GTK_DIALOG (rfxbuilder->dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (rfxbuilder->dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0);


  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (rfxbuilder->dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);

  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (on_rfxbuilder_ok),
		    (gpointer)rfxbuilder);

  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (on_rfxbuilder_cancel),
		    (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->requirements_button, "clicked", 
		   G_CALLBACK (on_list_table_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->properties_button, "clicked", 
		   G_CALLBACK (on_properties_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->params_button, "clicked", 
		   G_CALLBACK (on_list_table_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->param_window_button, "clicked", 
		   G_CALLBACK (on_list_table_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->trigger_button, "clicked", 
		   G_CALLBACK (on_list_table_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->pre_button, "clicked", 
		   G_CALLBACK (on_code_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->loop_button, "clicked", 
		   G_CALLBACK (on_code_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect(rfxbuilder->post_button, "clicked", 
		   G_CALLBACK (on_code_clicked),
		   (gpointer)rfxbuilder);

  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_effect1_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);
  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_effect2_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);
  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_effect0_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);
  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_tool_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);
  g_signal_connect_after (GTK_OBJECT (rfxbuilder->type_utility_radiobutton), "toggled",
			  G_CALLBACK (after_rfxbuilder_type_toggled),
			  (gpointer)rfxbuilder);


  g_signal_connect (GTK_OBJECT (rfxbuilder->dialog), "delete_event",
		    G_CALLBACK (gtk_true),
		    NULL);

  // edit mode
  if (script_name!=NULL) {
    // we assume the name is valid
    gchar *script_file;
    
    switch (status) {
    case RFX_STATUS_CUSTOM:
      script_file=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				    PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,script_name,NULL);
    break;
    case RFX_STATUS_BUILTIN:
      script_file=g_build_filename (prefs->prefix_dir,PLUGIN_SCRIPTS_DIR,
				    PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS,script_name,NULL);
    break;
    default:
      script_file=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				    PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,script_name,NULL);
    break;
    }
    if (!script_to_rfxbuilder (rfxbuilder,script_file)) {
      gchar *msg=g_strdup_printf (_ ("\n\nUnable to parse the script file:\n%s\n%s\n"),script_file,mainw->msg);
      // must use blocking error dialogs as the scriptname window is modal
      do_blocking_error_dialog (msg);
      g_free (msg);
      rfxbuilder_destroy (rfxbuilder);
      g_free (script_file);
      return NULL;
    }
    g_free (script_file);
    switch (rfxbuilder->type) {
    case RFX_BUILD_TYPE_TOOL:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rfxbuilder->type_tool_radiobutton),TRUE);
      break;
    case RFX_BUILD_TYPE_EFFECT0:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rfxbuilder->type_effect0_radiobutton),TRUE);
      break;
    case RFX_BUILD_TYPE_UTILITY:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rfxbuilder->type_utility_radiobutton),TRUE);
      break;
    case RFX_BUILD_TYPE_EFFECT2:
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rfxbuilder->type_effect2_radiobutton),TRUE);
      break;
    default:
      break;
    }
  }
  else {
    gtk_widget_grab_focus (rfxbuilder->name_entry);
  }
  return rfxbuilder;
}


void
after_rfxbuilder_type_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  gtk_widget_set_sensitive (rfxbuilder->pre_button,TRUE);
  gtk_widget_set_sensitive (rfxbuilder->loop_button,TRUE);
  gtk_widget_set_sensitive (rfxbuilder->post_button,TRUE);
  gtk_widget_set_sensitive (rfxbuilder->requirements_button,TRUE);
  gtk_widget_set_sensitive (rfxbuilder->properties_button,TRUE);
  gtk_widget_show (rfxbuilder->spinbutton_min_frames);
  gtk_widget_show (rfxbuilder->min_frames_label);
  gtk_widget_show (rfxbuilder->action_desc_label);
  gtk_widget_show (rfxbuilder->action_desc_entry);
  gtk_widget_show (rfxbuilder->action_desc_hsep);

  if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_effect1_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT1;
  }
  else if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_effect2_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT2;
    gtk_widget_set_sensitive (rfxbuilder->properties_button,FALSE);
  }
  else if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_effect0_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT0;
    gtk_widget_hide (rfxbuilder->spinbutton_min_frames);
    gtk_widget_hide (rfxbuilder->min_frames_label);
  }
  else if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_tool_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_TOOL;
    gtk_widget_hide (rfxbuilder->spinbutton_min_frames);
    gtk_widget_set_sensitive (rfxbuilder->properties_button,FALSE);
    gtk_widget_hide (rfxbuilder->min_frames_label);
  }
  else if (togglebutton==GTK_TOGGLE_BUTTON (rfxbuilder->type_utility_radiobutton)) {
    rfxbuilder->type=RFX_BUILD_TYPE_UTILITY;
    gtk_widget_hide (rfxbuilder->spinbutton_min_frames);
    gtk_widget_set_sensitive (rfxbuilder->properties_button,FALSE);
    gtk_widget_set_sensitive (rfxbuilder->requirements_button,FALSE);
    gtk_widget_set_sensitive (rfxbuilder->pre_button,FALSE);
    gtk_widget_set_sensitive (rfxbuilder->loop_button,FALSE);
    gtk_widget_set_sensitive (rfxbuilder->post_button,FALSE);
    gtk_widget_hide (rfxbuilder->min_frames_label);
    gtk_widget_hide (rfxbuilder->action_desc_label);
    gtk_widget_hide (rfxbuilder->action_desc_entry);
    gtk_widget_hide (rfxbuilder->action_desc_hsep);
  }

}






void on_list_table_clicked (GtkButton *button, gpointer user_data) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *vseparator;
  GtkWidget *button_box;

  // buttons
  GtkWidget *new_entry_button;
  GtkWidget *edit_entry_button;
  GtkWidget *remove_entry_button;
  GtkWidget *okbutton;
  GtkWidget *scrolledwindow;
  GtkWidget *cancelbutton;

  int i;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  if (button==GTK_BUTTON (rfxbuilder->requirements_button)) {
    rfxbuilder->table_type=RFX_TABLE_TYPE_REQUIREMENTS;
  }
  else if (button==GTK_BUTTON (rfxbuilder->params_button)) {
    rfxbuilder->table_type=RFX_TABLE_TYPE_PARAMS;
  }
  else if (button==GTK_BUTTON (rfxbuilder->param_window_button)) {
    rfxbuilder->table_type=RFX_TABLE_TYPE_PARAM_WINDOW;
  }
  else if (button==GTK_BUTTON (rfxbuilder->trigger_button)) {
    rfxbuilder->table_type=RFX_TABLE_TYPE_TRIGGERS;
  }

  dialog = gtk_dialog_new ();

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_REQUIREMENTS) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - RFX Requirements"));
    rfxbuilder->onum_reqs=rfxbuilder->num_reqs;
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAMS) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - RFX Parameters"));
    rfxbuilder->onum_params=rfxbuilder->num_params;
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - RFX Parameter Window Hints"));
    rfxbuilder->onum_paramw_hints=rfxbuilder->num_paramw_hints;
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_TRIGGERS) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - RFX Triggers"));
    rfxbuilder->onum_triggers=rfxbuilder->num_triggers;
  }

  gtk_widget_show (dialog);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
  }

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_set_spacing (GTK_BOX (dialog_vbox), 10);
  gtk_widget_show (dialog_vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_box_pack_start (GTK_BOX (hbox), scrolledwindow, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  // create table and add rows
  rfxbuilder->ptable_rows=rfxbuilder->table_rows=0;

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_REQUIREMENTS) {
    rfxbuilder->table=gtk_table_new (rfxbuilder->num_reqs,1,FALSE);
    for (i=0;i<rfxbuilder->num_reqs;i++) {
      on_table_add_row (NULL,(gpointer)rfxbuilder);
      gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry[i]),rfxbuilder->reqs[i]);
    }
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAMS) {
    rfxbuilder->copy_params=(lives_param_t *)g_malloc (RFXBUILD_MAX_PARAMS*sizeof(lives_param_t));
    rfxbuilder->table=gtk_table_new (rfxbuilder->num_params,3,FALSE);
    for (i=0;i<rfxbuilder->num_params;i++) {
      param_copy (&rfxbuilder->params[i],&rfxbuilder->copy_params[i],FALSE);
      on_table_add_row (NULL,(gpointer)rfxbuilder);
    }
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    rfxbuilder->table=gtk_table_new (rfxbuilder->table_rows,2,FALSE);
    for (i=0;i<rfxbuilder->num_paramw_hints;i++) {
      on_table_add_row (NULL,(gpointer)rfxbuilder);
    }
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_TRIGGERS) {
    rfxbuilder->copy_triggers=(rfx_trigger_t *)g_malloc ((RFXBUILD_MAX_PARAMS+1)*sizeof(rfx_trigger_t));
    rfxbuilder->table=gtk_table_new (rfxbuilder->table_rows,1,FALSE);
    for (i=0;i<rfxbuilder->num_triggers;i++) {
      rfxbuilder->copy_triggers[i].when=rfxbuilder->triggers[i].when;
      rfxbuilder->copy_triggers[i].code=g_strdup (rfxbuilder->triggers[i].code);
      on_table_add_row (NULL,(gpointer)rfxbuilder);
    }
  }
  gtk_widget_set_size_request (scrolledwindow,500,100);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), rfxbuilder->table);
  gtk_widget_show (rfxbuilder->table);


  // button box on right
  vseparator = gtk_vseparator_new ();
  gtk_widget_show (vseparator);
  gtk_box_pack_start (GTK_BOX (hbox), vseparator, TRUE, TRUE, 10);

  button_box=gtk_vbutton_box_new();
  gtk_box_pack_start (GTK_BOX (hbox), button_box, FALSE, FALSE, 0);
  gtk_widget_show (button_box);

  new_entry_button=gtk_button_new_with_mnemonic (_ ("_New Entry"));
  gtk_widget_show (new_entry_button);
  gtk_box_pack_start (GTK_BOX (button_box), new_entry_button, FALSE, FALSE, 0);

  edit_entry_button=gtk_button_new_with_mnemonic (_ ("_Edit Entry"));
  gtk_widget_show (edit_entry_button);
  gtk_box_pack_start (GTK_BOX (button_box), edit_entry_button, FALSE, FALSE, 0);

  remove_entry_button=gtk_button_new_with_mnemonic (_ ("_Remove Entry"));
  gtk_widget_show (remove_entry_button);
  gtk_box_pack_start (GTK_BOX (button_box), remove_entry_button, FALSE, FALSE, 0);

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    rfxbuilder->move_up_button=gtk_button_new_with_mnemonic (_ ("Move _Up"));
    gtk_widget_show (rfxbuilder->move_up_button);
    gtk_box_pack_start (GTK_BOX (button_box), rfxbuilder->move_up_button, FALSE, FALSE, 0);
    
    rfxbuilder->move_down_button=gtk_button_new_with_mnemonic (_ ("Move _Down"));
    gtk_widget_show (rfxbuilder->move_down_button);
    gtk_box_pack_start (GTK_BOX (button_box), rfxbuilder->move_down_button, FALSE, FALSE, 0);
  }

  dialog_action_area = GTK_DIALOG (dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_REQUIREMENTS) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_requirements_ok),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_requirements_cancel),
		      user_data);
    g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		      G_CALLBACK (gtk_true),
		      NULL);
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAMS) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_params_ok),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_params_cancel),
		      user_data);
    g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		      G_CALLBACK (gtk_true),
		      NULL);
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_param_window_ok),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_param_window_cancel),
		      user_data);
    g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		      G_CALLBACK (gtk_true),
		      NULL);
  }
  else if (rfxbuilder->table_type==RFX_TABLE_TYPE_TRIGGERS) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_triggers_ok),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_triggers_cancel),
		      user_data);
    g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		      G_CALLBACK (gtk_true),
		      NULL);
  }



  g_signal_connect (GTK_OBJECT (new_entry_button), "clicked",
		    G_CALLBACK (on_table_add_row),
		    user_data);

  g_signal_connect (GTK_OBJECT (edit_entry_button), "clicked",
		    G_CALLBACK (on_table_edit_row),
		    user_data);

  g_signal_connect (GTK_OBJECT (remove_entry_button), "clicked",
		    G_CALLBACK (on_table_delete_row),
		    user_data);

  if (rfxbuilder->table_type==RFX_TABLE_TYPE_PARAM_WINDOW) {
    g_signal_connect (GTK_OBJECT (rfxbuilder->move_up_button), "clicked",
		      G_CALLBACK (on_table_swap_row),
		      user_data);
    
    g_signal_connect (GTK_OBJECT (rfxbuilder->move_down_button), "clicked",
		      G_CALLBACK (on_table_swap_row),
		      user_data);
  }
}

void on_requirements_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->onum_reqs;i++) {
    g_free (rfxbuilder->reqs[i]);
  }
  for (i=0;i<rfxbuilder->num_reqs;i++) {
    rfxbuilder->reqs[i]=g_strdup (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry[i])));
  }
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_requirements_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  rfxbuilder->num_reqs=rfxbuilder->onum_reqs;
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_properties_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  if (rfxbuilder->type!=RFX_BUILD_TYPE_EFFECT0) {
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rfxbuilder->prop_slow))) {
      rfxbuilder->props|=RFX_PROPS_SLOW;
    }
    else if (rfxbuilder->props&RFX_PROPS_SLOW) {
      rfxbuilder->props^=RFX_PROPS_SLOW;
    }
  }
  else {
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (rfxbuilder->prop_batchg))) {
      rfxbuilder->props|=RFX_PROPS_BATCHG;
    }
    else if (rfxbuilder->props&RFX_PROPS_BATCHG) {
      rfxbuilder->props^=RFX_PROPS_BATCHG;
    }
  }
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_properties_cancel (GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_params_ok (GtkButton *button, gpointer user_data) { 
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->onum_params;i++) {
    g_free (rfxbuilder->params[i].name);
    g_free (rfxbuilder->params[i].label);
    if (rfxbuilder->params[i].type==LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->params[i].list!=NULL) g_list_free (rfxbuilder->params[i].list);
    }
    if (rfxbuilder->copy_params[i].def!=NULL) g_free (rfxbuilder->params[i].def);
  }
  if (rfxbuilder->onum_params) {
    g_free (rfxbuilder->params);
  }
  rfxbuilder->params=(lives_param_t *)g_malloc (rfxbuilder->num_params*sizeof(lives_param_t));
  for (i=0;i<rfxbuilder->num_params;i++) {
    param_copy (&rfxbuilder->copy_params[i],&rfxbuilder->params[i],FALSE);

    // this is the only place these should be freed
    g_free (rfxbuilder->copy_params[i].name);
    g_free (rfxbuilder->copy_params[i].label);
    if (rfxbuilder->copy_params[i].type==LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[i].list!=NULL) g_list_free (rfxbuilder->copy_params[i].list);
    }
    if (rfxbuilder->copy_params[i].def!=NULL) g_free (rfxbuilder->copy_params[i].def);
  }
  if (rfxbuilder->num_params) {
    g_free (rfxbuilder->copy_params);
  }
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}


void on_params_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->num_params;i++) {
    // this is the only place these should be freed
    g_free (rfxbuilder->copy_params[i].name);
    g_free (rfxbuilder->copy_params[i].label);
    if (rfxbuilder->copy_params[i].type==LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[i].list!=NULL) g_list_free (rfxbuilder->copy_params[i].list);
    }
    if (rfxbuilder->copy_params[i].def!=NULL) g_free (rfxbuilder->copy_params[i].def);
  }
  if (rfxbuilder->num_params) {
    g_free (rfxbuilder->copy_params);
  }
  rfxbuilder->num_params=rfxbuilder->onum_params;
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_param_window_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;

  for (i=0;i<rfxbuilder->onum_paramw_hints;i++) {
    g_free (rfxbuilder->paramw_hints[i]);
  }
  for (i=0;i<rfxbuilder->num_paramw_hints;i++) {
    rfxbuilder->paramw_hints[i]=g_strdup_printf ("%s%s%s",gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry[i])),rfxbuilder->field_delim,gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry2[i])));
  }
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_param_window_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  rfxbuilder->num_paramw_hints=rfxbuilder->onum_paramw_hints;
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_code_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  GtkTextIter startiter,enditer;

  gtk_text_buffer_get_start_iter (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter);
  gtk_text_buffer_get_end_iter (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&enditer);


  switch (rfxbuilder->codetype) {
  case RFX_CODE_TYPE_PRE:
    g_free (rfxbuilder->pre_code);
    rfxbuilder->pre_code=gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter,&enditer,FALSE);
    break;

  case RFX_CODE_TYPE_LOOP:
    g_free (rfxbuilder->loop_code);
    rfxbuilder->loop_code=gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter,&enditer,FALSE);
    break;

  case RFX_CODE_TYPE_POST:
    g_free (rfxbuilder->post_code);
    rfxbuilder->post_code=gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter,&enditer,FALSE);
    break;

  case RFX_CODE_TYPE_STRDEF:
    {
      gint maxlen=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max));
      gchar buf[++maxlen];
      
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].def!=NULL) g_free (rfxbuilder->copy_params[rfxbuilder->edit_param].def);
      g_snprintf (buf,maxlen,"%s",gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter,&enditer,FALSE));

      rfxbuilder->copy_params[rfxbuilder->edit_param].def=subst (buf,rfxbuilder->field_delim,"");
      break;
    }
  case RFX_CODE_TYPE_STRING_LIST:
    {
      gchar *values=gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter,&enditer,FALSE);
      gchar **lines=g_strsplit (values,"\n",-1);
      gint numlines=get_token_count (values,'\n');
      int i;
      gint defindex=get_int_param (rfxbuilder->copy_params[rfxbuilder->edit_param].def);

      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list!=NULL) {
	g_list_free (rfxbuilder->copy_params[rfxbuilder->edit_param].list);
	rfxbuilder->copy_params[rfxbuilder->edit_param].list=NULL;
      }
      for (i=0;i<numlines;i++) {
	if (i<numlines-1||strlen (lines[i])) {
	  rfxbuilder->copy_params[rfxbuilder->edit_param].list=g_list_append 
	    (rfxbuilder->copy_params[rfxbuilder->edit_param].list,g_strdup (lines[i]));
	}
      }
      g_strfreev (lines);

      // set "default" combo - TODO - try to retain old default using string matching
      lives_combo_populate (LIVES_COMBO (rfxbuilder->param_def_combo), 
			     rfxbuilder->copy_params[rfxbuilder->edit_param].list);
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list==NULL||
	  defindex>g_list_length (rfxbuilder->copy_params[rfxbuilder->edit_param].list)) {
	set_int_param (rfxbuilder->copy_params[rfxbuilder->edit_param].def,(defindex=0));
      }
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list!=NULL) {
	gtk_entry_set_text (GTK_ENTRY((GTK_COMBO(rfxbuilder->param_def_combo))->entry), 
			    (gchar *)g_list_nth_data (rfxbuilder->copy_params[rfxbuilder->edit_param].list,defindex));
      }

    }
  }
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_code_cancel (GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}
void on_triggers_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->onum_triggers;i++) {
    g_free (rfxbuilder->triggers[i].code);
  }
  if (rfxbuilder->onum_triggers) {
    g_free (rfxbuilder->triggers);
  }
  rfxbuilder->triggers=(rfx_trigger_t *)g_malloc (rfxbuilder->num_triggers*sizeof(rfx_trigger_t));
  for (i=0;i<rfxbuilder->num_triggers;i++) {
    rfxbuilder->triggers[i].when=rfxbuilder->copy_triggers[i].when;
    rfxbuilder->triggers[i].code=g_strdup (rfxbuilder->copy_triggers[i].code);
    g_free (rfxbuilder->copy_triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    g_free (rfxbuilder->copy_triggers);
  }
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

void on_triggers_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  int i;
  for (i=0;i<rfxbuilder->num_triggers;i++) {
    g_free (rfxbuilder->copy_triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    g_free (rfxbuilder->copy_triggers);
  }
  rfxbuilder->num_triggers=rfxbuilder->onum_triggers;
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}


void on_properties_clicked (GtkButton *button, gpointer user_data) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *label;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - RFX Properties"));
  gtk_widget_show (dialog);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(rfxbuilder->dialog));
  }
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
  }

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  gtk_window_set_default_size (GTK_WINDOW (rfxbuilder->dialog), 300, 200);

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_set_spacing (GTK_BOX (dialog_vbox), 10);
  gtk_widget_show (dialog_vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  if (rfxbuilder->type!=RFX_BUILD_TYPE_EFFECT0) {
    gtk_widget_show (hbox);
  }
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->prop_slow= gtk_check_button_new ();
  label=gtk_label_new_with_mnemonic (_ ("_Slow (hint to GUI)"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),rfxbuilder->prop_slow);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->prop_slow, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
  gtk_widget_show (label);
  gtk_widget_show (rfxbuilder->prop_slow);
  if (rfxbuilder->props&RFX_PROPS_SLOW) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rfxbuilder->prop_slow),TRUE);
  }

  if (rfxbuilder->type==RFX_BUILD_TYPE_EFFECT0) {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);
    
    rfxbuilder->prop_batchg= gtk_check_button_new ();
    label=gtk_label_new_with_mnemonic (_ ("_Batch mode generator"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),rfxbuilder->prop_batchg);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->prop_batchg, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_widget_show (label);
    gtk_widget_show (rfxbuilder->prop_batchg);
    if (rfxbuilder->props&RFX_PROPS_BATCHG) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rfxbuilder->prop_batchg),TRUE);
    }
  }

  dialog_action_area = GTK_DIALOG (dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);

  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (on_properties_ok),
		    user_data);

  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (on_properties_cancel),
		    user_data);
  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		    G_CALLBACK (gtk_true),
		    NULL);
  

}




void on_table_add_row (GtkButton *button, gpointer user_data) {
  // horrible, horrible code...
  GtkWidget *entry,*entry2,*entry3;
  GtkWidget *param_dialog=NULL;
  GtkWidget *param_window_dialog=NULL;
  GtkWidget *trigger_dialog=NULL;

  GtkTextIter startiter,enditer;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  lives_param_t *param=NULL;
  int i;

  gchar *tmpx;
  gchar *ctext;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_REQUIREMENTS:
    if (rfxbuilder->table_rows>=RFXBUILD_MAX_REQ) return;

    for (i=0;i<rfxbuilder->table_rows;i++) {
      gtk_editable_set_editable (GTK_EDITABLE (rfxbuilder->entry[i]), FALSE);
    }


    entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();
    gtk_widget_show (entry);
    gtk_editable_set_editable (GTK_EDITABLE (entry), TRUE);

    gtk_table_resize (GTK_TABLE (rfxbuilder->table),++rfxbuilder->table_rows,1);
    gtk_table_attach (GTK_TABLE (rfxbuilder->table), entry, 0, 1, rfxbuilder->table_rows-1+rfxbuilder->ptable_rows, rfxbuilder->table_rows+rfxbuilder->ptable_rows,
		      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);
    while (g_main_context_iteration (NULL,FALSE));
    gtk_widget_grab_focus (entry);
    if (button!=NULL) {
      rfxbuilder->num_reqs++;
    }
    break;

  case RFX_TABLE_TYPE_PARAMS:
    entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();
    entry2 = rfxbuilder->entry2[rfxbuilder->table_rows] = gtk_entry_new ();

    if (button!=NULL) {
      gboolean param_ok=FALSE;
      param=&rfxbuilder->copy_params[rfxbuilder->num_params];

      param->def=NULL;
      param->list=NULL;
      param->type=LIVES_PARAM_UNKNOWN;

      param_dialog=make_param_dialog(-1,rfxbuilder);
      do {
	if (gtk_dialog_run (GTK_DIALOG (param_dialog))==GTK_RESPONSE_CANCEL) {
	  g_free (param->def);
	  gtk_widget_destroy (entry);
	  gtk_widget_destroy (entry2);
	  gtk_widget_destroy (param_dialog);
	  return;
	}
	param_ok=perform_param_checks (rfxbuilder,rfxbuilder->num_params,rfxbuilder->num_params+1);
      } while (!param_ok);
      
      rfxbuilder->num_params++;

      gtk_entry_set_text (GTK_ENTRY (entry2),gtk_entry_get_text (GTK_ENTRY (rfxbuilder->param_name_entry)));
    }
    else {
      gtk_entry_set_text (GTK_ENTRY (entry2),rfxbuilder->params[rfxbuilder->table_rows].name);
    }

    gtk_entry_set_text (GTK_ENTRY (entry),(tmpx=g_strdup_printf ("p%d",rfxbuilder->table_rows)));
    g_free(tmpx);
    gtk_widget_show (entry);
     
    gtk_table_attach (GTK_TABLE (rfxbuilder->table), entry, 0, 1, rfxbuilder->table_rows+rfxbuilder->ptable_rows, rfxbuilder->table_rows+1+rfxbuilder->ptable_rows,
		      (GtkAttachOptions) (0),
		      (GtkAttachOptions) (0), 0, 0);


    gtk_widget_show (entry2);
    gtk_editable_set_editable (GTK_EDITABLE (entry2), FALSE);

    gtk_table_resize (GTK_TABLE (rfxbuilder->table),++rfxbuilder->table_rows,3);



    gtk_table_attach (GTK_TABLE (rfxbuilder->table), entry2, 1, 2, rfxbuilder->table_rows-1+rfxbuilder->ptable_rows, rfxbuilder->table_rows+rfxbuilder->ptable_rows,
		      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);

    entry3 = rfxbuilder->entry3[rfxbuilder->table_rows-1] = gtk_entry_new ();

    if (button==NULL) {
      param=&rfxbuilder->params[rfxbuilder->table_rows-1];
    }
    else {
      param_set_from_dialog ((param=&rfxbuilder->copy_params[rfxbuilder->table_rows-1]),rfxbuilder);
    }
    
    switch (param->type) {
    case LIVES_PARAM_NUM:
      gtk_entry_set_text (GTK_ENTRY (entry3),(tmpx=g_strdup_printf ("num%d",param->dp)));
      g_free(tmpx);
      break;
    case LIVES_PARAM_BOOL:
      gtk_entry_set_text (GTK_ENTRY (entry3),"bool");
      break;
    case LIVES_PARAM_COLRGB24:
      gtk_entry_set_text (GTK_ENTRY (entry3),"colRGB24");
      break;
    case LIVES_PARAM_STRING:
      gtk_entry_set_text (GTK_ENTRY (entry3),"string");
      break;
    case LIVES_PARAM_STRING_LIST:
      gtk_entry_set_text (GTK_ENTRY (entry3),"string_list");
      break;
    default:
      break;
    }

    gtk_widget_show (entry3);
    gtk_editable_set_editable (GTK_EDITABLE (entry3), FALSE);

    gtk_table_attach (GTK_TABLE (rfxbuilder->table), entry3, 2, 3, rfxbuilder->table_rows-1+rfxbuilder->ptable_rows, rfxbuilder->table_rows+rfxbuilder->ptable_rows,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    if (button==NULL) return;
    
    gtk_widget_queue_resize(gtk_widget_get_parent(GTK_WIDGET(rfxbuilder->table)));
    gtk_widget_destroy (param_dialog);
    break;


  case RFX_TABLE_TYPE_PARAM_WINDOW:
    if (button!=NULL) {
      param_window_dialog=make_param_window_dialog(-1,rfxbuilder);
      if (gtk_dialog_run (GTK_DIALOG (param_window_dialog))==GTK_RESPONSE_CANCEL) {
	gtk_widget_destroy (param_window_dialog);
	return;
      }
      entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();
      ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_kw_combo));
      gtk_entry_set_text (GTK_ENTRY (entry),ctext);
      g_free(ctext);
      rfxbuilder->num_paramw_hints++;
    }
    else {
      gchar **array=g_strsplit(rfxbuilder->paramw_hints[rfxbuilder->table_rows],rfxbuilder->field_delim,2);
      entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (entry),array[0]);
      g_strfreev (array);
    }

    gtk_widget_show (entry);
    gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);

    gtk_table_resize (GTK_TABLE (rfxbuilder->table),++rfxbuilder->table_rows,2);
    gtk_table_attach (GTK_TABLE (rfxbuilder->table), entry, 0, 1, rfxbuilder->table_rows-1+rfxbuilder->ptable_rows, rfxbuilder->table_rows+rfxbuilder->ptable_rows,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    entry2 = rfxbuilder->entry2[rfxbuilder->table_rows-1] = gtk_entry_new ();
    if (button!=NULL) {
      if (!strcmp (gtk_entry_get_text (GTK_ENTRY (entry)),"layout")) {
	gtk_entry_set_text (GTK_ENTRY (entry2),gtk_entry_get_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry)));
      }
      else {
	// TODO - use lives_rfx_special_t->has_subtype,name,num_params
	ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_sp_combo));
	if (!strcmp (ctext,"framedraw")) {
	  gchar *ctext2=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_spsub_combo));
	  gtk_entry_set_text (GTK_ENTRY (entry2),(tmpx=g_strdup_printf ("%s%s%s%s%s",ctext,rfxbuilder->field_delim,ctext2,rfxbuilder->field_delim,gtk_entry_get_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry)))));
	  g_free(ctext2);
	}
	else {
	  gtk_entry_set_text (GTK_ENTRY (entry2),(tmpx=g_strdup_printf ("%s%s%s",ctext,rfxbuilder->field_delim,gtk_entry_get_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry)))));
	}
	g_free(tmpx);
	g_free(ctext);
      }
    }
    else {
      gchar **array=g_strsplit(rfxbuilder->paramw_hints[rfxbuilder->table_rows-1],rfxbuilder->field_delim,2);
      gtk_entry_set_text (GTK_ENTRY (entry2),array[1]);
      g_strfreev (array);
    }
    gtk_widget_show (entry2);
    gtk_editable_set_editable (GTK_EDITABLE (entry2), FALSE);

    gtk_table_attach (GTK_TABLE (rfxbuilder->table), entry2, 1, 2, rfxbuilder->table_rows-1+rfxbuilder->ptable_rows, rfxbuilder->table_rows+rfxbuilder->ptable_rows,
		      (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		      (GtkAttachOptions) (0), 0, 0);

    if (button==NULL) return;

    gtk_widget_destroy (param_window_dialog);
    gtk_widget_queue_resize (gtk_widget_get_parent(GTK_WIDGET(rfxbuilder->table)));
    break;


 case RFX_TABLE_TYPE_TRIGGERS:
    entry = rfxbuilder->entry[rfxbuilder->table_rows] = gtk_entry_new ();
    
    if (button!=NULL) {
      trigger_dialog=make_trigger_dialog(-1,rfxbuilder);
      if (gtk_dialog_run (GTK_DIALOG (trigger_dialog))==GTK_RESPONSE_CANCEL) {
	gtk_widget_destroy (trigger_dialog);
	return;
      }
      gtk_entry_set_text (GTK_ENTRY (entry),gtk_entry_get_text (GTK_ENTRY (rfxbuilder->trigger_when_entry)));
      rfxbuilder->num_triggers++;
    }
    else {
      gchar *tmpx2=NULL;
      gtk_entry_set_text (GTK_ENTRY (entry),rfxbuilder->triggers[rfxbuilder->table_rows].when?(tmpx2=g_strdup_printf ("%d",rfxbuilder->triggers[rfxbuilder->table_rows].when-1)):"init");
      if (tmpx2!=NULL) g_free(tmpx2);
    }
    
    gtk_widget_show (entry);
    gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);

    gtk_table_resize (GTK_TABLE (rfxbuilder->table),++rfxbuilder->table_rows,1);
    gtk_table_attach (GTK_TABLE (rfxbuilder->table), entry, 0, 1, rfxbuilder->table_rows-1+rfxbuilder->ptable_rows, rfxbuilder->table_rows+rfxbuilder->ptable_rows,
		      (GtkAttachOptions) (GTK_FILL),
		      (GtkAttachOptions) (0), 0, 0);

    if (button==NULL) return;

    gtk_text_buffer_get_start_iter (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter);
    gtk_text_buffer_get_end_iter (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&enditer);

    rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].code=gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter,&enditer,FALSE);
    
    rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].when=atoi (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->trigger_when_entry)))+1;
    if (!strcmp(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->trigger_when_entry)),"init")) rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].when=0;

    if (!rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].when) {
      rfxbuilder->has_init_trigger=TRUE;
    }
    else {
      rfxbuilder->params[rfxbuilder->copy_triggers[rfxbuilder->table_rows-1].when-1].onchange=TRUE;
    }

    gtk_widget_destroy (trigger_dialog);
    gtk_widget_queue_resize (gtk_widget_get_parent(GTK_WIDGET(rfxbuilder->table)));
    break;
}
}



void param_set_from_dialog (lives_param_t *copy_param, rfx_build_window_t *rfxbuilder) {
  // set parameter values from param_dialog
  // this is called after adding a new copy_param or editing an existing one
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  copy_param->name=g_strdup (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->param_name_entry)));
  copy_param->label=g_strdup (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->param_label_entry)));
  
  if (!strcmp (ctext,"num")) {
    copy_param->type=LIVES_PARAM_NUM;
  }
  else if (!strcmp (ctext,"bool")) {
    copy_param->type=LIVES_PARAM_BOOL;
  }
  else if (!strcmp (ctext,"colRGB24")) {
    copy_param->type=LIVES_PARAM_COLRGB24;
  }
  else if (!strcmp (ctext,"string")) {
    copy_param->type=LIVES_PARAM_STRING;
  }
  else if (!strcmp (ctext,"string_list")) {
    copy_param->type=LIVES_PARAM_STRING_LIST;
  }

  g_free(ctext);

  copy_param->dp=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_dp));
  copy_param->group=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_group));
  copy_param->onchange=FALSE;  // no trigger assigned yet
  
  // TODO - check
  if (copy_param->type!=LIVES_PARAM_STRING&&copy_param->def!=NULL) g_free (copy_param->def);
  if (copy_param->type!=LIVES_PARAM_STRING_LIST&&copy_param->list!=NULL) g_list_free (copy_param->list);
  
  switch (copy_param->type) {
  case LIVES_PARAM_NUM:
    copy_param->min=gtk_spin_button_get_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_min));
    copy_param->max=gtk_spin_button_get_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    copy_param->step_size=gtk_spin_button_get_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step));
    copy_param->wrap=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rfxbuilder->param_wrap_checkbutton));
    if (!copy_param->dp) {
      copy_param->def=g_malloc (sizint);
      set_int_param (copy_param->def,gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    }
    else {
      copy_param->def=g_malloc (sizdbl);
      set_double_param (copy_param->def,gtk_spin_button_get_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    }
    break;
  case LIVES_PARAM_BOOL:
    copy_param->def=g_malloc (sizint);
    set_bool_param (copy_param->def,gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)));
    copy_param->dp=0;
    break;
  case LIVES_PARAM_STRING:
    copy_param->max=gtk_spin_button_get_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max));
    copy_param->dp=0;
    break;
  case LIVES_PARAM_STRING_LIST:
    copy_param->dp=0;
    copy_param->def=g_malloc (sizdbl);
    set_int_param (copy_param->def,lives_list_index (copy_param->list,gtk_entry_get_text (GTK_ENTRY (GTK_COMBO(rfxbuilder->param_def_combo)->entry))));
    break;
  case LIVES_PARAM_COLRGB24:
    copy_param->def=g_malloc (3*sizint);
    set_colRGB24_param (copy_param->def,gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def)),gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_min)),gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max)));
    copy_param->dp=0;
    break;
  default:
    break;
  }
}





void on_table_edit_row (GtkButton *button, gpointer user_data) {
  GtkWidget *param_dialog;
  GtkWidget *paramw_dialog;
  GtkWidget *trigger_dialog;

  GtkTextIter startiter,enditer;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  lives_param_t *param;
  int i;
  gint found=-1;
  gboolean param_ok=FALSE;

  gchar *tmpx;
  gchar *ctext;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_REQUIREMENTS:
    for (i=0;i<rfxbuilder->table_rows&&found==-1;i++) {
      if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)) found=i;
    }
    if (found==-1) return;
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (found==i) { 
	gtk_editable_set_editable (GTK_EDITABLE (rfxbuilder->entry[i]), TRUE);
	while (g_main_context_iteration (NULL,FALSE));
	gtk_widget_grab_focus (rfxbuilder->entry[i]);
      }
      else {
	gtk_editable_set_editable (GTK_EDITABLE (rfxbuilder->entry[i]), FALSE);
      }
    }
    break;

  case RFX_TABLE_TYPE_PARAMS:
    for (i=0;i<rfxbuilder->table_rows&&found==-1;i++) {
      if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)||gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry2[i]),NULL,NULL)||gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry3[i]),NULL,NULL)) found=i;
    }
    if (found==-1) return;

    param_dialog=make_param_dialog(found,rfxbuilder);
    do {
      if (gtk_dialog_run (GTK_DIALOG (param_dialog))==GTK_RESPONSE_CANCEL) {
	gtk_widget_destroy (param_dialog);
	return;
      }
      param_ok=perform_param_checks (rfxbuilder,found,rfxbuilder->num_params);
    } while (!param_ok);
    
    param_set_from_dialog ((param=&rfxbuilder->copy_params[found]),rfxbuilder);
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry2[found]),param->name);
    switch (param->type) {
    case LIVES_PARAM_NUM:
      gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry3[found]),(tmpx=g_strdup_printf ("num%d",param->dp)));
      g_free(tmpx);
      break;
    case LIVES_PARAM_BOOL:
      gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry3[found]),"bool");
      break;
    case LIVES_PARAM_COLRGB24:
      gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry3[found]),"colRGB24");
      break;
    case LIVES_PARAM_STRING:
      gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry3[found]),"string");
      break;
    case LIVES_PARAM_STRING_LIST:
      gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry3[found]),"string_list");
      break;
    default:
      break;
    }
    gtk_widget_destroy (param_dialog);
    break;

  case RFX_TABLE_TYPE_PARAM_WINDOW:
    for (i=0;i<rfxbuilder->table_rows&&found==-1;i++) {
      if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)||gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry2[i]),NULL,NULL)) found=i;
    }
    if (found==-1) return;
    paramw_dialog=make_param_window_dialog(found,rfxbuilder);
    if (gtk_dialog_run (GTK_DIALOG (paramw_dialog))==GTK_RESPONSE_CANCEL) {
      gtk_widget_destroy (paramw_dialog);
      return;
    }
    ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_kw_combo));
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry[found]),ctext);
    g_free(ctext);
    if (!strcmp (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry[found])),"layout")) {
      gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry2[found]),gtk_entry_get_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry)));
    }
    else {
      // TODO - use lives_rfx_special_t->has_subtype,name,num_params
      ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_sp_combo));
      if (!strcmp (ctext,"framedraw")) {
	gchar *ctext2=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->paramw_spsub_combo));
	gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry2[found]),
			    (tmpx=g_strdup_printf ("%s%s%s%s%s",ctext,rfxbuilder->field_delim,ctext2,rfxbuilder->field_delim,
						   gtk_entry_get_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry)))));
	g_free(ctext2);
      }
      else {
	gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry2[found]),
			    (tmpx=g_strdup_printf ("%s%s%s",ctext,rfxbuilder->field_delim,gtk_entry_get_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry)))));
      }
      g_free(ctext);
      g_free(tmpx);
    }
    gtk_widget_destroy (paramw_dialog);
    break;

  case RFX_TABLE_TYPE_TRIGGERS:
    for (i=0;i<rfxbuilder->table_rows&&found==-1;i++) {
      if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)) found=i;
    }
    if (found==-1) return;
    trigger_dialog=make_trigger_dialog(found,rfxbuilder);
    if (gtk_dialog_run (GTK_DIALOG (trigger_dialog))==GTK_RESPONSE_CANCEL) {
      gtk_widget_destroy (trigger_dialog);
      return;
    }

    gtk_text_buffer_get_start_iter (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter);
    gtk_text_buffer_get_end_iter (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&enditer);

    g_free (rfxbuilder->copy_triggers[found].code);
    rfxbuilder->copy_triggers[found].code=gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)),&startiter,&enditer,FALSE);

    gtk_widget_destroy (trigger_dialog);
    break;
  }
}



void on_table_swap_row (GtkButton *button, gpointer user_data) {
  gint found=-1;
  gchar *entry_text;
  int i;
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_PARAM_WINDOW:
    for (i=0;i<rfxbuilder->table_rows&&found==-1;i++) {
      if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)||gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry2[i]),NULL,NULL)) found=i;
    }
  default:
    break;
  }

  if (found==-1) if ((found=rfxbuilder->table_swap_row1)==-1) return;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_PARAM_WINDOW:
    if (button==GTK_BUTTON (rfxbuilder->move_up_button)) {
      rfxbuilder->table_swap_row2=found-1;
    }
    else if (button==GTK_BUTTON (rfxbuilder->move_down_button)) {
      rfxbuilder->table_swap_row2=found+1;
    }
    if (rfxbuilder->table_swap_row2<0||rfxbuilder->table_swap_row2>=rfxbuilder->table_rows) return;

    entry_text=g_strdup(gtk_entry_get_text(GTK_ENTRY(rfxbuilder->entry[found])));
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry[found]),gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry[rfxbuilder->table_swap_row2])));
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry[rfxbuilder->table_swap_row2]),entry_text);
    g_free (entry_text);

    entry_text=g_strdup(gtk_entry_get_text(GTK_ENTRY(rfxbuilder->entry2[found])));
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry2[found]),gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry2[rfxbuilder->table_swap_row2])));
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry2[rfxbuilder->table_swap_row2]),entry_text);
    g_free (entry_text);

    break;
  default:
    break;
  }
  rfxbuilder->table_swap_row1=rfxbuilder->table_swap_row2;

  // TODO - more of this...
  gtk_editable_select_region (GTK_EDITABLE (rfxbuilder->entry2[rfxbuilder->table_swap_row1]),0,-1);
}


void on_table_delete_row (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  int i;
  gint move=0;
  gboolean triggers_adjusted=FALSE;

  switch (rfxbuilder->table_type) {
  case RFX_TABLE_TYPE_REQUIREMENTS:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (move) {
	rfxbuilder->entry[i-1]=rfxbuilder->entry[i];
      }
      else if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)) {
	gtk_widget_destroy (rfxbuilder->entry[i]);
	if (rfxbuilder->table_rows>1) {
	  gtk_table_resize (GTK_TABLE (rfxbuilder->table),rfxbuilder->table_rows-1,1);
	}
	move=i+1;
	rfxbuilder->ptable_rows++;
      }
    }
    if (!move) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_reqs--;
    break;

  case RFX_TABLE_TYPE_PARAMS:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (move) {
	// note - parameters become renumbered here
	gchar *tmpx;
	rfxbuilder->entry[i-1]=rfxbuilder->entry[i];
	gtk_entry_set_text (GTK_ENTRY (rfxbuilder->entry[i-1]),(tmpx=g_strdup_printf ("p%d",i-1)));
	g_free(tmpx);
	rfxbuilder->entry2[i-1]=rfxbuilder->entry2[i];
	rfxbuilder->entry3[i-1]=rfxbuilder->entry3[i];
	param_copy (&rfxbuilder->copy_params[i],&rfxbuilder->copy_params[i-1],FALSE);
	g_free (rfxbuilder->copy_params[i].name);
	g_free (rfxbuilder->copy_params[i].label);
	g_free (rfxbuilder->copy_params[i].def);
      }
      else if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)||gtk_editable_get_selection_bounds (GTK_EDITABLE (rfxbuilder->entry2[i]),NULL,NULL)||gtk_editable_get_selection_bounds (GTK_EDITABLE (rfxbuilder->entry2[i]),NULL,NULL)) {
	if (rfxbuilder->copy_params[i].onchange) {
	  do_blocking_error_dialog(_ ("\n\nCannot remove this parameter as it has a trigger.\nPlease remove the trigger first.\n\n"));
	  return;
	}
	gtk_widget_destroy (rfxbuilder->entry[i]);
	gtk_widget_destroy (rfxbuilder->entry2[i]);
	gtk_widget_destroy (rfxbuilder->entry3[i]);
	g_free (rfxbuilder->copy_params[i].name);
	g_free (rfxbuilder->copy_params[i].label);
	g_free (rfxbuilder->copy_params[i].def);

	if (rfxbuilder->table_rows>1) {
	  gtk_table_resize (GTK_TABLE (rfxbuilder->table),rfxbuilder->table_rows-1,3);
	}
	move=i+1;
	rfxbuilder->ptable_rows++;
      }
    }
    if (!move) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_params--;
    for (i=0;i<rfxbuilder->num_triggers;i++) {
      if (rfxbuilder->triggers[i].when>move) {
	rfxbuilder->params[rfxbuilder->triggers[i].when-1].onchange=FALSE;
	rfxbuilder->params[--rfxbuilder->triggers[i].when-1].onchange=TRUE;
      }
      triggers_adjusted=TRUE;
    }
    if (triggers_adjusted) {
      do_blocking_error_dialog (_ ("\n\nSome triggers were adjusted.\nPlease check the trigger code.\n"));
    }
    break;

  case RFX_TABLE_TYPE_PARAM_WINDOW:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (move) {
	rfxbuilder->entry[i-1]=rfxbuilder->entry[i];
	rfxbuilder->entry2[i-1]=rfxbuilder->entry2[i];
      }
      else if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)||gtk_editable_get_selection_bounds (GTK_EDITABLE (rfxbuilder->entry2[i]),NULL,NULL)) {
	gtk_widget_destroy (rfxbuilder->entry[i]);
	gtk_widget_destroy (rfxbuilder->entry2[i]);
	if (rfxbuilder->table_rows>1) {
	  gtk_table_resize (GTK_TABLE (rfxbuilder->table),rfxbuilder->table_rows-1,1);
	}
	move=i+1;
	rfxbuilder->ptable_rows++;
      }
    }
    if (!move) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_paramw_hints--;
    break;

  case RFX_TABLE_TYPE_TRIGGERS:
    for (i=0;i<rfxbuilder->table_rows;i++) {
      if (move) {
	rfxbuilder->entry[i-1]=rfxbuilder->entry[i];
	rfxbuilder->copy_triggers[i-1].when=rfxbuilder->copy_triggers[i].when;
	rfxbuilder->copy_triggers[i-1].code=g_strdup (rfxbuilder->copy_triggers[i].code);
	g_free (rfxbuilder->copy_triggers[i].code);
      }
      else if (gtk_editable_get_selection_bounds(GTK_EDITABLE(rfxbuilder->entry[i]),NULL,NULL)) {
	gint when=atoi (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry[i])))+1;

	if (!strcmp(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry[i])),"init")) rfxbuilder->has_init_trigger=FALSE;
	else rfxbuilder->params[when-1].onchange=FALSE;

	gtk_widget_destroy (rfxbuilder->entry[i]);
	g_free (rfxbuilder->copy_triggers[i].code);
	if (rfxbuilder->table_rows>1) {
	  gtk_table_resize (GTK_TABLE (rfxbuilder->table),rfxbuilder->table_rows-1,1);
	}
	move=i+1;
	rfxbuilder->ptable_rows++;
      }
    }
    if (!move) return;
    rfxbuilder->table_rows--;
    rfxbuilder->num_triggers--;
    break;
  }
}


GtkWidget * make_param_dialog (gint pnum, rfx_build_window_t *rfxbuilder) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *label;
  GtkWidget *hseparator;
  GObject *spinbutton_adj;

  GList *typelist=NULL;

  lives_colRGB24_t rgb;

  gchar *tmp,*tmp2;

  dialog = gtk_dialog_new ();
  if (pnum<0) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - New RFX Parameter"));
  }
  else {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Edit RFX Parameter"));
  }
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
  }

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  //gtk_window_set_default_size (GTK_WINDOW (rfxbuilder->dialog), 300, 200);

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_set_spacing (GTK_BOX (dialog_vbox), 10);
  gtk_widget_show (dialog_vbox);

  // name

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("_Name:    "));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->param_name_entry = gtk_entry_new ();
  gtk_widget_show (rfxbuilder->param_name_entry);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_name_entry, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),rfxbuilder->param_name_entry);

  if (pnum>=0) {
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->param_name_entry),rfxbuilder->copy_params[pnum].name);
  }
  gtk_widget_set_tooltip_text( rfxbuilder->param_name_entry,(_ ("Name of the parameter, must be unique in the plugin.")));


  // label

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("_Label:    "));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->param_label_entry = gtk_entry_new ();
  gtk_widget_show (rfxbuilder->param_label_entry);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_label_entry, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),rfxbuilder->param_label_entry);

  if (pnum>=0) {
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->param_label_entry),rfxbuilder->copy_params[pnum].label);
  }
  gtk_widget_set_tooltip_text( rfxbuilder->param_label_entry,(_ ("Label to be shown by the parameter. An underscore represents mnemonic accelerator.")));

  // group

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->bg_label = gtk_label_new_with_mnemonic (_("Button _Group: "));
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->bg_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->bg_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new (0., 0., 16., 1., 1., 0.);
  rfxbuilder->spinbutton_param_group = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->spinbutton_param_group, TRUE, FALSE, 0);
  if (pnum>=0) {
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_group),(gdouble)rfxbuilder->copy_params[pnum].group);
  }
  gtk_widget_set_tooltip_text( rfxbuilder->spinbutton_param_group,(_ ("A non-zero value can be used to group radio buttons.")));
  gtk_label_set_mnemonic_widget (GTK_LABEL (rfxbuilder->bg_label),rfxbuilder->spinbutton_param_group);

  // type

  typelist = g_list_append (typelist, (gpointer)"num");
  typelist = g_list_append (typelist, (gpointer)"bool");
  typelist = g_list_append (typelist, (gpointer)"string");
  typelist = g_list_append (typelist, (gpointer)"colRGB24");
  typelist = g_list_append (typelist, (gpointer)"string_list");

  rfxbuilder->param_type_combo = lives_standard_combo_new ((tmp=g_strdup(_("_Type:         "))),TRUE,typelist,
							   LIVES_BOX(dialog_vbox),(tmp2=g_strdup(_ ("Parameter type (select from list)."))));
  g_free(tmp);
  g_free(tmp2);
  g_list_free(typelist);

  gtk_widget_show_all(gtk_widget_get_parent(rfxbuilder->param_type_combo));

  if (pnum>=0) {
    rfxbuilder->edit_param=pnum;
    switch(rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rfxbuilder->param_type_combo),0);
      break;
    case LIVES_PARAM_BOOL:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rfxbuilder->param_type_combo),1);
      break;
    case LIVES_PARAM_COLRGB24:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rfxbuilder->param_type_combo),2);
      break;
    case LIVES_PARAM_STRING:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rfxbuilder->param_type_combo),3);
      break;
    case LIVES_PARAM_STRING_LIST:
      gtk_combo_box_set_active(GTK_COMBO_BOX(rfxbuilder->param_type_combo),4);
      break;
    default:
      break;
    }
  }
  else rfxbuilder->edit_param=rfxbuilder->num_params;


  // dp

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->param_dp_label = gtk_label_new_with_mnemonic (_("Decimal _places: "));
  gtk_widget_show (rfxbuilder->param_dp_label);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_dp_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->param_dp_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new (0., 0., 16., 1., 1., 0.);
  rfxbuilder->spinbutton_param_dp = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_widget_show (rfxbuilder->spinbutton_param_dp);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->spinbutton_param_dp, TRUE, FALSE, 0);
  if (pnum>=0) {
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp),(gdouble)rfxbuilder->copy_params[pnum].dp);
  }

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hseparator, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (rfxbuilder->param_dp_label),rfxbuilder->spinbutton_param_dp);


  // default val

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->param_def_label = gtk_label_new_with_mnemonic (_("_Default value:    "));
  gtk_widget_show (rfxbuilder->param_def_label);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_def_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->param_def_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new (0., -G_MAXINT, G_MAXINT, 1., 1., 0.);
  rfxbuilder->spinbutton_param_def = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (rfxbuilder->param_def_label),rfxbuilder->spinbutton_param_def);

  // extra bits for string/string_list
  gtk_widget_show (rfxbuilder->spinbutton_param_def);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->spinbutton_param_def, TRUE, FALSE, 0);

  rfxbuilder->param_strdef_button = gtk_button_new();
  gtk_button_set_use_underline (GTK_BUTTON (rfxbuilder->param_strdef_button),TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_strdef_button, TRUE, FALSE, 0);

  rfxbuilder->param_strlist_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), rfxbuilder->param_strlist_hbox, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (_("_Default: "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (rfxbuilder->param_strlist_hbox), label, FALSE, FALSE, 0);

  rfxbuilder->param_def_combo = lives_combo_new ();
  gtk_box_pack_start (GTK_BOX (rfxbuilder->param_strlist_hbox), rfxbuilder->param_def_combo, FALSE, FALSE, 0);
  gtk_widget_show(rfxbuilder->param_def_combo);

  gtk_label_set_mnemonic_widget (GTK_LABEL (rfxbuilder->param_def_label),rfxbuilder->param_def_combo);

  if (pnum>=0) {
    switch (rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      if (!rfxbuilder->copy_params[pnum].dp) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),
				   (gdouble)get_int_param (rfxbuilder->copy_params[pnum].def));
      }
      else {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),
				   get_double_param (rfxbuilder->copy_params[pnum].def));
      }
      break;
    case LIVES_PARAM_BOOL:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),
				 (gdouble)get_bool_param (rfxbuilder->copy_params[pnum].def));
      break;
    case LIVES_PARAM_COLRGB24:
      get_colRGB24_param (rfxbuilder->copy_params[pnum].def,&rgb);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),(gdouble)rgb.red);
      break;
    default:
      break;
    }
  }

  // min

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->param_min_label = gtk_label_new_with_mnemonic (_("_Minimum value: "));
  gtk_widget_show (rfxbuilder->param_min_label);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_min_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->param_min_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new (0., -G_MAXINT, G_MAXINT, 1., 1., 0.);
  rfxbuilder->spinbutton_param_min = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);

  gtk_widget_show (rfxbuilder->spinbutton_param_min);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->spinbutton_param_min, TRUE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (rfxbuilder->param_min_label),rfxbuilder->spinbutton_param_min);

  // max

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->param_max_label = gtk_label_new_with_mnemonic (_("Ma_ximum value: "));
  gtk_widget_show (rfxbuilder->param_max_label);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_max_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->param_max_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new (RFX_DEF_NUM_MAX, -G_MAXINT, G_MAXINT, 1., 1., 0.);
  rfxbuilder->spinbutton_param_max = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (rfxbuilder->param_max_label),rfxbuilder->spinbutton_param_max);

  gtk_widget_show (rfxbuilder->spinbutton_param_max);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->spinbutton_param_max, TRUE, FALSE, 0);


  // step size

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->param_step_label = gtk_label_new_with_mnemonic (_("     _Step size:   "));
  gtk_widget_show (rfxbuilder->param_step_label);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_step_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->param_step_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_adj = (GObject *)gtk_adjustment_new (1., 1., G_MAXINT, 1., 1., 0.);
  rfxbuilder->spinbutton_param_step = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);

  gtk_widget_show (rfxbuilder->spinbutton_param_step);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->spinbutton_param_step, TRUE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (rfxbuilder->param_step_label),rfxbuilder->spinbutton_param_step);
  gtk_widget_set_tooltip_text( rfxbuilder->spinbutton_param_step,
			       (_ ("How much the parameter is adjusted when the spinbutton arrows are pressed.")));

  // wrap

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->param_wrap_eventbox=gtk_event_box_new();

  rfxbuilder->param_wrap_label = gtk_label_new_with_mnemonic (_("_Wrap: "));
  gtk_widget_show (rfxbuilder->param_wrap_label);
  gtk_widget_show (rfxbuilder->param_wrap_eventbox);

  gtk_container_add(GTK_CONTAINER(rfxbuilder->param_wrap_eventbox),rfxbuilder->param_wrap_label);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(rfxbuilder->param_wrap_label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(rfxbuilder->param_wrap_eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (rfxbuilder->param_wrap_eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  rfxbuilder->param_wrap_checkbutton = gtk_check_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_wrap_eventbox, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->param_wrap_checkbutton, FALSE, FALSE, 10);
  gtk_widget_show (rfxbuilder->param_wrap_checkbutton);
  GTK_WIDGET_SET_FLAGS (rfxbuilder->param_wrap_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_set_tooltip_text( rfxbuilder->param_wrap_checkbutton,(_ ("Whether the value wraps max->min and min->max.")));
  gtk_label_set_mnemonic_widget (GTK_LABEL (rfxbuilder->param_wrap_label),rfxbuilder->param_wrap_checkbutton);

  g_signal_connect (GTK_OBJECT (rfxbuilder->param_wrap_eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    rfxbuilder->param_wrap_checkbutton);




  dialog_action_area = GTK_DIALOG (dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		    G_CALLBACK (gtk_true),
		    NULL);

  g_signal_connect (GTK_OBJECT (rfxbuilder->param_strdef_button), "clicked",
		    G_CALLBACK (on_code_clicked),
		    (gpointer)rfxbuilder);

  g_signal_connect (GTK_OBJECT(rfxbuilder->param_type_combo),"changed",G_CALLBACK (on_param_type_changed),
		    (gpointer)rfxbuilder);

  g_signal_connect_after (GTK_OBJECT (rfxbuilder->spinbutton_param_dp), "value_changed",
			  G_CALLBACK (after_param_dp_changed),
			  (gpointer)rfxbuilder);

  rfxbuilder->def_spin_f=g_signal_connect_after (GTK_OBJECT (rfxbuilder->spinbutton_param_def), "value_changed",
			  G_CALLBACK (after_param_def_changed),
			  (gpointer)rfxbuilder);

  rfxbuilder->min_spin_f=g_signal_connect_after (GTK_OBJECT (rfxbuilder->spinbutton_param_min), "value_changed",
			  G_CALLBACK (after_param_min_changed),
			  (gpointer)rfxbuilder);

  rfxbuilder->max_spin_f=g_signal_connect_after (GTK_OBJECT (rfxbuilder->spinbutton_param_max), "value_changed",
			  G_CALLBACK (after_param_max_changed),
			  (gpointer)rfxbuilder);

  if (pnum>=0) {
    on_param_type_changed (LIVES_COMBO (rfxbuilder->param_type_combo),(gpointer)rfxbuilder);
    switch (rfxbuilder->copy_params[pnum].type) {
    case LIVES_PARAM_NUM:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),rfxbuilder->copy_params[pnum].min);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),rfxbuilder->copy_params[pnum].max);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_step),
				 rfxbuilder->copy_params[pnum].step_size);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rfxbuilder->param_wrap_checkbutton),
				   rfxbuilder->copy_params[pnum].wrap);
      break;
    case LIVES_PARAM_COLRGB24:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),(gdouble)rgb.green);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),(gdouble)rgb.blue);
      break;
    case LIVES_PARAM_STRING:
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),rfxbuilder->copy_params[pnum].max);
      break;
    default:
      break;
    }
  }
  return dialog;
}


void after_param_dp_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gint dp=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinbutton));

  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),dp);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),dp);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),dp);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_step),dp);

  if (dp>0) {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1./(double)lives_10pow(dp),
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))-
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_step),MAXFLOATLEN+dp);
  }
  else {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1,
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))-
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_step),MAXINTLEN);
  }
}


void after_param_min_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gint dp;
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp (ctext,"num")) {
    g_free(ctext);
    return;
  }

  g_free(ctext);

  g_signal_handler_block (rfxbuilder->spinbutton_param_max,rfxbuilder->max_spin_f);
  g_signal_handler_block (rfxbuilder->spinbutton_param_def,rfxbuilder->def_spin_f);

  if ((dp=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)))>0) {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)),
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_def),MAXFLOATLEN+dp);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)),G_MAXFLOAT);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),
			      1./(double)lives_10pow(dp),gtk_spin_button_get_value 
			      (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))-
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_step),MAXFLOATLEN+dp);
  }
  else {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
			      gtk_spin_button_get_value_as_int 
			      (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)),
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_def),MAXINTLEN);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)),
			      G_MAXINT);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1,
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))-
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_step),MAXINTLEN);
  }

  g_signal_handler_unblock (rfxbuilder->spinbutton_param_max,rfxbuilder->max_spin_f);
  g_signal_handler_unblock (rfxbuilder->spinbutton_param_def,rfxbuilder->def_spin_f);

}

void after_param_max_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gint dp;
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp (ctext,"num")) {
    g_free(ctext);
    return;
  }

  g_free(ctext);

  g_signal_handler_block (rfxbuilder->spinbutton_param_min,rfxbuilder->min_spin_f);
  g_signal_handler_block (rfxbuilder->spinbutton_param_def,rfxbuilder->def_spin_f);

  if ((dp=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)))>0) {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)),
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_def),MAXFLOATLEN+dp);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),-G_MAXFLOAT,
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max)));
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),
			      1./(double)lives_10pow(dp),
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))-
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_step),MAXFLOATLEN+dp);
  }
  else {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),
			      gtk_spin_button_get_value_as_int 
			      (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)),
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_def),MAXINTLEN);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),-G_MAXINT,
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max)));
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1,
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))
			      -gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min)));
    gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_step),MAXINTLEN);
  }

  g_signal_handler_unblock (rfxbuilder->spinbutton_param_min,rfxbuilder->min_spin_f);
  g_signal_handler_unblock (rfxbuilder->spinbutton_param_def,rfxbuilder->def_spin_f);

}

void after_param_def_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  if (strcmp (ctext,"num")) {
    g_free(ctext);
    return;
  }

  g_free(ctext);

  if (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp))) {
    gdouble dbl_def=gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def));
    if (dbl_def<gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min))) {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),dbl_def);
    }
    else if (dbl_def>gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))) {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),dbl_def);
    }
  }
  else {
    gint int_def=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def));
    if (int_def<gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min))) {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),(gdouble)int_def);
    }
    else if (int_def>gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max))) {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),(gdouble)int_def);
    }
  }
}


void on_param_type_changed (GtkComboBox *param_type_combo, gpointer user_data) {

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gint pnum=rfxbuilder->edit_param;
  gint dp;

  gchar *ctext=lives_combo_get_active_text(LIVES_COMBO(rfxbuilder->param_type_combo));

  gtk_widget_show (rfxbuilder->param_min_label);
  gtk_widget_show (rfxbuilder->param_max_label);
  gtk_widget_show (rfxbuilder->spinbutton_param_min);
  gtk_widget_show (rfxbuilder->spinbutton_param_max);
  gtk_widget_show (rfxbuilder->spinbutton_param_dp);
  gtk_widget_show (rfxbuilder->spinbutton_param_def);
  gtk_widget_show (rfxbuilder->param_dp_label);
  gtk_widget_show (rfxbuilder->param_def_label);
  gtk_widget_hide (rfxbuilder->param_strdef_button);
  gtk_widget_hide (rfxbuilder->param_strlist_hbox);
  gtk_widget_hide (rfxbuilder->spinbutton_param_group);
  gtk_widget_hide (rfxbuilder->bg_label);
  gtk_widget_hide (rfxbuilder->spinbutton_param_step);
  gtk_widget_hide (rfxbuilder->param_step_label);
  gtk_widget_hide (rfxbuilder->param_wrap_label);
  gtk_widget_hide (rfxbuilder->param_wrap_checkbutton);
  gtk_widget_hide (rfxbuilder->param_wrap_eventbox);

  if (!strcmp (ctext,"string_list")) {
    gint defindex;

    if (rfxbuilder->copy_params[pnum].type!=LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->copy_params[pnum].def!=NULL) {
	g_free (rfxbuilder->copy_params[pnum].def);
      }
      rfxbuilder->copy_params[pnum].def=g_malloc (sizint);
      set_int_param (rfxbuilder->copy_params[pnum].def,0);
    }

    rfxbuilder->copy_params[pnum].type=LIVES_PARAM_STRING_LIST;
    defindex=get_int_param (rfxbuilder->copy_params[pnum].def);

    lives_combo_populate (LIVES_COMBO (rfxbuilder->param_def_combo), rfxbuilder->copy_params[pnum].list);

    if (rfxbuilder->copy_params[pnum].list==NULL||defindex>g_list_length (rfxbuilder->copy_params[pnum].list)) {
      set_int_param (rfxbuilder->copy_params[pnum].def,(defindex=0));
    }

    if (rfxbuilder->copy_params[pnum].list!=NULL) {
      gtk_entry_set_text (GTK_ENTRY((GTK_COMBO(rfxbuilder->param_def_combo))->entry),
			  (gchar *)g_list_nth_data (rfxbuilder->copy_params[pnum].list,defindex));
    }

    gtk_widget_hide (rfxbuilder->spinbutton_param_dp);
    gtk_widget_hide (rfxbuilder->param_dp_label);
    gtk_widget_hide (rfxbuilder->spinbutton_param_min);
    gtk_widget_hide (rfxbuilder->param_min_label);
    gtk_widget_hide (rfxbuilder->spinbutton_param_def);
    gtk_widget_hide (rfxbuilder->param_def_label);
    gtk_widget_hide (rfxbuilder->spinbutton_param_max);
    gtk_widget_hide (rfxbuilder->param_max_label);

    gtk_button_set_label (GTK_BUTTON (rfxbuilder->param_strdef_button),(_("Set _values")));
    gtk_widget_show (rfxbuilder->param_strdef_button);
    gtk_widget_show (rfxbuilder->param_strlist_hbox);
  }
  else {
    if (!strcmp (ctext,"num")) {
      rfxbuilder->copy_params[pnum].type=LIVES_PARAM_NUM;
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_def_label),(_("Default value:    ")));
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_min_label),(_("Minimum value: ")));
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_max_label),(_("Maximum value: ")));
      gtk_widget_show (rfxbuilder->spinbutton_param_step);
      gtk_widget_show (rfxbuilder->param_step_label);
      gtk_widget_show (rfxbuilder->param_wrap_label);
      gtk_widget_show (rfxbuilder->param_wrap_checkbutton);
      gtk_widget_show (rfxbuilder->param_wrap_eventbox);

      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),
				  gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)));
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),
				  gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)));
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),
				  gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)));
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_step),
				  gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)));

      if ((dp=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_dp)))>0) {
	gdouble min=gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min));
	gdouble max=RFX_DEF_NUM_MAX;
	gdouble def=gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def));
	gdouble step=gtk_spin_button_get_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_step));
      
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),-G_MAXFLOAT,G_MAXFLOAT);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),-G_MAXFLOAT,G_MAXFLOAT);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),-G_MAXFLOAT,G_MAXFLOAT);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1./dp,(max-min)>1.?(max-min):1.);
	gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_def),MAXFLOATLEN);
	gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_step),MAXFLOATLEN+dp);
	if (def<min) {
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),min);
	}
	if (def>max) {
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),max);
	}
	if (step>(max-min)) {
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),max-min);
	}
	if (step<1.) {
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1.);
	}
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),max);
      }
      else {
	gint min=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min));
	gint max=RFX_DEF_NUM_MAX;
	gint def=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def));
	gint step=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_step));

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),-G_MAXINT,G_MAXINT);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),-G_MAXINT,G_MAXINT);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),-G_MAXINT,G_MAXINT);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1,(max-min)>1?(max-min):1);
	gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_def),MAXINTLEN);
	gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_step),MAXINTLEN);
	if (def<min) {
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),(gdouble)min);
	}
	if (def>max) {
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),(gdouble)max);
	}
	if (step>(max-min)) {
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),(gdouble)(max-min));
	}
	if (step<1) {
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_step),1);
	}
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),max);
      }
    }
    else if (!strcmp (ctext,"bool")) {
      rfxbuilder->copy_params[pnum].type=LIVES_PARAM_BOOL;
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_def_label),(_("Default value:    ")));
      gtk_widget_hide (rfxbuilder->param_min_label);
      gtk_widget_hide (rfxbuilder->param_max_label);
      gtk_widget_hide (rfxbuilder->spinbutton_param_min);
      gtk_widget_hide (rfxbuilder->spinbutton_param_max);
      gtk_widget_hide (rfxbuilder->spinbutton_param_dp);
      gtk_widget_hide (rfxbuilder->param_dp_label);
      gtk_widget_show (rfxbuilder->spinbutton_param_group);
      gtk_widget_show (rfxbuilder->bg_label);
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),0,1);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),0);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),0);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),0);
    }
    else if (!strcmp (ctext,"colRGB24")) {
      rfxbuilder->copy_params[pnum].type=LIVES_PARAM_COLRGB24;
      gtk_widget_hide (rfxbuilder->spinbutton_param_dp);
      gtk_widget_hide (rfxbuilder->param_dp_label);
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_def_label),(_ ("Default _Red:  ")));
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_min_label),(_ ("Default _Green:")));
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_max_label),(_ ("Default _Blue: ")));
      gtk_entry_set_width_chars (GTK_ENTRY (rfxbuilder->spinbutton_param_def),4);
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_def),0,255);
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_min),0,255);
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),0,255);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),0);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_min),0);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),0);
    }
    else if (!strcmp (ctext,"string")) {
      rfxbuilder->copy_params[pnum].type=LIVES_PARAM_STRING;
      gtk_widget_hide (rfxbuilder->spinbutton_param_dp);
      gtk_widget_hide (rfxbuilder->param_dp_label);
      gtk_widget_hide (rfxbuilder->spinbutton_param_min);
      gtk_widget_hide (rfxbuilder->param_min_label);
      gtk_widget_hide (rfxbuilder->spinbutton_param_def);
      
      gtk_button_set_label (GTK_BUTTON (rfxbuilder->param_strdef_button),(_("Set _default")));
      gtk_widget_show (rfxbuilder->param_strdef_button);
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_def_label),(_ ("Default value:  ")));
      gtk_label_set_text (GTK_LABEL (rfxbuilder->param_max_label),(_ ("Maximum length (chars): ")));
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),1,RFX_MAXSTRINGLEN);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(rfxbuilder->spinbutton_param_max),RFX_TEXT_MAGIC);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_def),0);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max),0);
      
      if (rfxbuilder->copy_params[pnum].def==NULL) {
	rfxbuilder->copy_params[pnum].def=g_strdup ("");
      }
    }
  }

  g_free(ctext);
}






GtkWidget * make_param_window_dialog (gint pnum, rfx_build_window_t *rfxbuilder) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;

  GList *kwlist=NULL;
  GList *splist=NULL;
  GList *spsublist=NULL;


  gchar *kw=NULL;
  gchar *sp=NULL;
  gchar *spsub=NULL;
  gchar *rest=NULL;

  dialog = gtk_dialog_new ();
  if (pnum<0) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - New RFX Parameter Window Hint"));
  }
  else {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Edit RFX Parameter Window Hint"));
  }
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
  }

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_set_spacing (GTK_BOX (dialog_vbox), 10);
  gtk_widget_show (dialog_vbox);

  if (pnum>=0) {
    kw=g_strdup (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry[pnum])));
    if (!strcmp (kw,"layout")) {
      rest=g_strdup (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry2[pnum])));
    }
    else if (!strcmp (kw,"special")) {
      gint numtok=get_token_count (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry2[pnum])),
				   (int)rfxbuilder->field_delim[0]);
      gchar **array=g_strsplit(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->entry2[pnum])),rfxbuilder->field_delim,3);
      sp=g_strdup (array[0]);
      if (!strcmp (sp,"framedraw")) {
	if (numtok>1) {
	  spsub=g_strdup (array[1]);
	}
	if (numtok>2) {
	  rest=g_strdup (array[2]);
	}
      }
      else {
	if (numtok>1) {
	  rest=g_strdup (array[1]);
	}
      }
      g_strfreev (array);
    }
  }

  // kw
  kwlist = g_list_append (kwlist, (gpointer)"layout");
  kwlist = g_list_append (kwlist, (gpointer)"special");

  rfxbuilder->paramw_kw_combo = lives_standard_combo_new (_("_Keyword:         "),TRUE,kwlist,LIVES_BOX(dialog_vbox),NULL);
  gtk_widget_show_all(gtk_widget_get_parent(rfxbuilder->paramw_kw_combo));
  g_list_free(kwlist);

  if (pnum>=0&&kw!=NULL) {
    lives_combo_set_active_string (LIVES_COMBO (rfxbuilder->paramw_kw_combo),kw);
  }


  // type
  splist = g_list_append (splist, (gpointer)"aspect");
  splist = g_list_append (splist, (gpointer)"framedraw");
  splist = g_list_append (splist, (gpointer)"fileread");
  splist = g_list_append (splist, (gpointer)"password");
  if (rfxbuilder->type==RFX_BUILD_TYPE_EFFECT2) {
    splist = g_list_append (splist, (gpointer)"mergealign");
  }

  rfxbuilder->paramw_sp_combo = lives_standard_combo_new (_("Special _Type:         "),TRUE,splist,LIVES_BOX(dialog_vbox),NULL);
  gtk_widget_show_all(gtk_widget_get_parent(rfxbuilder->paramw_sp_combo));

  g_list_free(splist);

  if (pnum>=0&&sp!=NULL) {
    lives_combo_set_active_string (LIVES_COMBO (rfxbuilder->paramw_sp_combo),sp);
  }

  // subtype

  spsublist = g_list_append (spsublist, (gpointer)"rectdemask");
  spsublist = g_list_append (spsublist, (gpointer)"multrect");
  spsublist = g_list_append (spsublist, (gpointer)"singlepoint");

  rfxbuilder->paramw_spsub_combo = lives_standard_combo_new (_("Special _Subtype:         "),TRUE,spsublist,LIVES_BOX(dialog_vbox),NULL);
  gtk_widget_show_all(gtk_widget_get_parent(rfxbuilder->paramw_spsub_combo));

  g_list_free(spsublist);

  if (pnum>=0&&spsub!=NULL) {
    lives_combo_set_active_string (LIVES_COMBO (rfxbuilder->paramw_spsub_combo),spsub);
  }


  // paramwindow rest

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  rfxbuilder->paramw_rest_label = gtk_label_new (_("Row:    "));
  gtk_widget_show (rfxbuilder->paramw_rest_label);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->paramw_rest_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (rfxbuilder->paramw_rest_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  rfxbuilder->paramw_rest_entry = gtk_entry_new ();
  gtk_widget_show (rfxbuilder->paramw_rest_entry);
  gtk_box_pack_start (GTK_BOX (hbox), rfxbuilder->paramw_rest_entry, TRUE, TRUE, 0);
  if (pnum>=0&&rest!=NULL) {
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->paramw_rest_entry),rest);
  }

  if (kw!=NULL) g_free (kw);
  if (sp!=NULL) g_free (sp);
  if (spsub!=NULL) g_free (spsub);
  if (rest!=NULL) g_free (rest);


  gtk_widget_grab_focus (rfxbuilder->paramw_rest_entry);

  dialog_action_area = GTK_DIALOG (dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		    G_CALLBACK (gtk_true),
		    NULL);

  g_signal_connect (GTK_OBJECT(rfxbuilder->paramw_kw_combo),"changed",
		    G_CALLBACK (on_paramw_kw_changed),(gpointer)rfxbuilder);

  g_signal_connect (GTK_OBJECT(rfxbuilder->paramw_sp_combo),"changed",
		    G_CALLBACK (on_paramw_sp_changed),(gpointer)rfxbuilder);

  g_signal_connect (GTK_OBJECT(rfxbuilder->paramw_spsub_combo),"changed",
		    G_CALLBACK (on_paramw_spsub_changed),(gpointer)rfxbuilder);

  on_paramw_kw_changed (GTK_COMBO_BOX (rfxbuilder->paramw_kw_combo),(gpointer)rfxbuilder);

  return dialog;
}



void on_paramw_kw_changed (GtkComboBox *combo, gpointer user_data) {

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gchar *ctext=lives_combo_get_active_text(combo);

  if (!strcmp (ctext,"special")) {
    gtk_widget_show_all (gtk_widget_get_parent(rfxbuilder->paramw_sp_combo));
    on_paramw_sp_changed (GTK_COMBO_BOX(rfxbuilder->paramw_sp_combo),(gpointer)rfxbuilder);
    gtk_widget_grab_focus (lives_combo_get_entry(LIVES_COMBO(rfxbuilder->paramw_sp_combo)));
  }
  else {
    gtk_label_set_text (GTK_LABEL (rfxbuilder->paramw_rest_label),(_("Row:    ")));
    gtk_widget_hide_all (gtk_widget_get_parent(rfxbuilder->paramw_sp_combo));
    gtk_widget_hide_all (gtk_widget_get_parent(rfxbuilder->paramw_spsub_combo));
    gtk_widget_grab_focus (rfxbuilder->paramw_rest_entry);
  }

  g_free(ctext);
}


void on_paramw_sp_changed (GtkComboBox *combo, gpointer user_data) {

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gint npars;
  gchar *tmpx;
  gchar *ctext=lives_combo_get_active_text(combo);

  if (!strcmp (ctext,"framedraw")) {
    gtk_widget_show_all (gtk_widget_get_parent(rfxbuilder->paramw_spsub_combo));
    on_paramw_spsub_changed (GTK_COMBO_BOX(rfxbuilder->paramw_spsub_combo),(gpointer)rfxbuilder);
    gtk_widget_grab_focus (lives_combo_get_entry(LIVES_COMBO(rfxbuilder->paramw_spsub_combo)));
  }
  else {
    if (!strcmp(ctext,"fileread")||!strcmp(ctext,"password")) npars=1;
    else npars=2;
    gtk_label_set_text (GTK_LABEL (rfxbuilder->paramw_rest_label),
			(tmpx=g_strdup_printf(_("Linked parameters (%d):    "),npars)));
    g_free(tmpx);
    gtk_widget_hide_all (gtk_widget_get_parent(rfxbuilder->paramw_spsub_combo));
    gtk_widget_grab_focus (rfxbuilder->paramw_rest_entry);
  }

  g_free(ctext);
}


void on_paramw_spsub_changed (GtkComboBox *combo, gpointer user_data) {

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;
  gchar *ctext=lives_combo_get_active_text(combo);

  if (!strcmp (ctext,"rectdemask")||
      !strcmp (ctext,"multrect")) {
    gtk_label_set_text (GTK_LABEL (rfxbuilder->paramw_rest_label),(_("Linked parameters (4):    ")));
  }
  else if (!strcmp (ctext,"singlepoint")) {
    gtk_label_set_text (GTK_LABEL (rfxbuilder->paramw_rest_label),(_("Linked parameters (2):    ")));
  }

  g_free(ctext);
}





GtkWidget * make_trigger_dialog (gint tnum, rfx_build_window_t *rfxbuilder) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *label;
  GtkWidget *combo;
  GtkWidget *scrolledwindow;

  GList *whenlist=NULL;

  int i;

  dialog = gtk_dialog_new ();
  if (tnum<0) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - New RFX Trigger"));
  }
  else {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Edit RFX Trigger"));
  }
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
  }

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  //gtk_window_set_default_size (GTK_WINDOW (rfxbuilder->dialog), 300, 200);

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_set_spacing (GTK_BOX (dialog_vbox), 10);
  gtk_widget_show (dialog_vbox);

  // when
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  label = gtk_label_new (_("When:         "));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  if (tnum>=0) whenlist = g_list_append (whenlist, (gpointer)(rfxbuilder->copy_triggers[tnum].when?
							      g_strdup_printf ("%d",rfxbuilder->copy_triggers[tnum].
									       when-1):"init"));
  else {
    if (!rfxbuilder->has_init_trigger) {
      whenlist = g_list_append (whenlist, (gpointer)"init");
    }
    for (i=0;i<rfxbuilder->num_params;i++) {
      if (!rfxbuilder->params[i].onchange) {
	whenlist = g_list_append (whenlist, g_strdup_printf ("%d",i));
      }
    }
  }

  combo = lives_combo_new ();
  lives_combo_populate (LIVES_COMBO (combo), whenlist);
  g_list_free(whenlist);
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, FALSE, 0);
  gtk_widget_show(combo);
  rfxbuilder->trigger_when_entry=lives_combo_get_entry(LIVES_COMBO(combo));


  // code area

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), scrolledwindow, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  rfxbuilder->code_textview = gtk_text_view_new ();
  gtk_widget_show (rfxbuilder->code_textview);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), rfxbuilder->code_textview);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (rfxbuilder->code_textview), TRUE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (rfxbuilder->code_textview), GTK_WRAP_WORD);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (rfxbuilder->code_textview), TRUE);

  if (tnum>=0) {
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)), 
			      rfxbuilder->copy_triggers[tnum].code, -1);
    gtk_widget_grab_focus(rfxbuilder->code_textview);
  }
    
  gtk_widget_set_size_request (scrolledwindow,400,100);

  dialog_action_area = GTK_DIALOG (dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		    G_CALLBACK (gtk_true),
		    NULL);

  return dialog;
}




void on_code_clicked (GtkButton *button, gpointer user_data) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *scrolledwindow;

  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  gchar *tmpx;

  dialog = gtk_dialog_new ();
  gtk_widget_show (dialog);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
  }

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  //gtk_window_set_default_size (GTK_WINDOW (rfxbuilder->dialog), 300, 200);

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_set_spacing (GTK_BOX (dialog_vbox), 10);
  gtk_widget_show (dialog_vbox);


  // code area
  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), scrolledwindow, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  rfxbuilder->code_textview = gtk_text_view_new ();
  g_object_ref (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolledwindow)));
  g_object_ref (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolledwindow)));
  gtk_widget_show (rfxbuilder->code_textview);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), rfxbuilder->code_textview);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (rfxbuilder->code_textview), TRUE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (rfxbuilder->code_textview), GTK_WRAP_WORD);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (rfxbuilder->code_textview), TRUE);

  gtk_widget_grab_focus (rfxbuilder->code_textview);

  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (rfxbuilder->code_textview),GTK_WRAP_NONE);


  // TODO !!
  /*  if (glib_major_version>=2&&glib_minor_version>=4) {
      gtk_text_view_set_accepts_tab (GTK_TEXT_VIEW (rfxbuilder->code_textview),TRUE);
    } */

  if (button==GTK_BUTTON (rfxbuilder->pre_button)) {
    rfxbuilder->codetype=RFX_CODE_TYPE_PRE;
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Pre Loop Code"));
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)), 
			      rfxbuilder->pre_code, -1);
  }

  else if (button==GTK_BUTTON (rfxbuilder->loop_button)) {
    rfxbuilder->codetype=RFX_CODE_TYPE_LOOP;
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Loop Code"));
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)), 
			      rfxbuilder->loop_code, -1);
  }

  else if (button==GTK_BUTTON (rfxbuilder->post_button)) {
    rfxbuilder->codetype=RFX_CODE_TYPE_POST;
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Post Loop Code"));
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)), 
			      rfxbuilder->post_code, -1);
  }

  else if (button==GTK_BUTTON (rfxbuilder->param_strdef_button)) {
    if (rfxbuilder->copy_params[rfxbuilder->edit_param].type!=LIVES_PARAM_STRING_LIST) {
      gint len,maxlen=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_param_max));

      if ((len=strlen ((gchar *)rfxbuilder->copy_params[rfxbuilder->edit_param].def))>maxlen) len=maxlen;
      
      rfxbuilder->codetype=RFX_CODE_TYPE_STRDEF;
      gtk_window_set_title (GTK_WINDOW (dialog), (tmpx=g_strdup_printf 
						  (_("LiVES: - Default text (max length %d)"),maxlen)));
      g_free(tmpx);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (rfxbuilder->code_textview),GTK_WRAP_WORD);

      gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)), 
				(gchar *)rfxbuilder->copy_params[rfxbuilder->edit_param].def, len);
    }
    else {
      int i;
      gchar *string=g_strdup (""),*string_new;
      GtkTextIter start_iter;

      rfxbuilder->codetype=RFX_CODE_TYPE_STRING_LIST;
      gtk_window_set_title (GTK_WINDOW (dialog), (tmpx=g_strdup (_("LiVES: - Enter values, one per line"))));
      g_free(tmpx);
      if (rfxbuilder->copy_params[rfxbuilder->edit_param].list!=NULL) {
	for (i=0;i<g_list_length (rfxbuilder->copy_params[rfxbuilder->edit_param].list);i++) {
	  string_new=g_strconcat (string, (gchar *)g_list_nth_data (rfxbuilder->copy_params[rfxbuilder->edit_param].list,i),
				  "\n",NULL);
	  if (string!=string_new) g_free (string);
	  string=string_new;
	}
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)), string, 
				  strlen (string)-1);
	g_free (string);
	gtk_text_buffer_get_start_iter (gtk_text_view_get_buffer(GTK_TEXT_VIEW(rfxbuilder->code_textview)),&start_iter);
	gtk_text_buffer_place_cursor (gtk_text_view_get_buffer (GTK_TEXT_VIEW (rfxbuilder->code_textview)), &start_iter);
      }
    }
  }

  gtk_widget_set_size_request (scrolledwindow,600,400);

  dialog_action_area = GTK_DIALOG (dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);

  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (on_code_ok),
		    user_data);
  
  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (on_code_cancel),
		    user_data);

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		    G_CALLBACK (gtk_true),
		    NULL);


}


void on_rfxbuilder_ok (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  if (!perform_rfxbuilder_checks (rfxbuilder)) return;
  if (!rfxbuilder_to_script (rfxbuilder)) return;

  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  while (g_main_context_iteration (NULL,FALSE));
  rfxbuilder_destroy (rfxbuilder);
}

void on_rfxbuilder_cancel (GtkButton *button, gpointer user_data) {
  rfx_build_window_t *rfxbuilder=(rfx_build_window_t *)user_data;

  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  while (g_main_context_iteration (NULL,FALSE));
  rfxbuilder_destroy (rfxbuilder);
}



void rfxbuilder_destroy (rfx_build_window_t *rfxbuilder) {
  int i;

  for (i=0;i<rfxbuilder->num_reqs;i++) {
    g_free (rfxbuilder->reqs[i]);
  }
  for (i=0;i<rfxbuilder->num_params;i++) {
    g_free (rfxbuilder->params[i].name);
    g_free (rfxbuilder->params[i].label);
    if (rfxbuilder->params[i].type==LIVES_PARAM_STRING_LIST) {
      if (rfxbuilder->params[i].list!=NULL)g_list_free (rfxbuilder->params[i].list);
    }
    g_free (rfxbuilder->params[i].def);
  }
  if (rfxbuilder->num_params) {
    g_free (rfxbuilder->params);
  }
  for (i=0;i<rfxbuilder->num_paramw_hints;i++) {
    g_free (rfxbuilder->paramw_hints[i]);
  }
  for (i=0;i<rfxbuilder->num_triggers;i++) {
    g_free (rfxbuilder->triggers[i].code);
  }
  if (rfxbuilder->num_triggers) {
    g_free (rfxbuilder->triggers);
  }

  g_free (rfxbuilder->pre_code);
  g_free (rfxbuilder->loop_code);
  g_free (rfxbuilder->post_code);

  g_free (rfxbuilder->field_delim);

  if (rfxbuilder->script_name!=NULL) {
    g_free (rfxbuilder->script_name);
  }
  if (rfxbuilder->oname!=NULL) {
    g_free (rfxbuilder->oname);
  }
  g_free(rfxbuilder->rfx_version);

  g_free (rfxbuilder);
}


gboolean perform_rfxbuilder_checks (rfx_build_window_t *rfxbuilder) {
  gchar *name=g_strdup (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->name_entry)));

  if (!strlen (name)) {
    do_blocking_error_dialog (_ ("\n\nName must not be blank.\n"));
    g_free (name);
    return FALSE;
  }
  if (get_token_count (name,' ')>1) {
    do_blocking_error_dialog (_ ("\n\nName must not contain spaces.\n"));
    g_free (name);
    return FALSE;
  }
  if (!strlen(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->menu_text_entry)))) {
    do_blocking_error_dialog (_ ("\n\nMenu text must not be blank.\n"));
    g_free (name);
    return FALSE;
  }
  if (!strlen(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->action_desc_entry)))&&
      rfxbuilder->type!=RFX_BUILD_TYPE_UTILITY) {
    do_blocking_error_dialog (_ ("\n\nAction description must not be blank.\n"));
    g_free (name);
    return FALSE;
  }
  if (!strlen(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->author_entry)))) {
    do_blocking_error_dialog (_ ("\n\nAuthor must not be blank.\n"));
    g_free (name);
    return FALSE;
  }

  if (rfxbuilder->mode!=RFX_BUILDER_MODE_NEW&&!(rfxbuilder->mode==RFX_BUILDER_MODE_EDIT&&
						!strcmp (rfxbuilder->oname,name))) {
    if (find_rfx_plugin_by_name (name,RFX_STATUS_TEST)>-1||find_rfx_plugin_by_name 
	(name,RFX_STATUS_CUSTOM)>-1||find_rfx_plugin_by_name (name,RFX_STATUS_BUILTIN)>-1) {
      do_blocking_error_dialog (_ ("\n\nThere is already a plugin with this name.\nName must be unique.\n"));
      g_free (name);
      return FALSE;
    }
  }

  if (!strlen(rfxbuilder->loop_code)&&rfxbuilder->type!=RFX_BUILD_TYPE_UTILITY) {
    do_blocking_error_dialog (_("\n\nLoop code should not be blank.\n"));
    g_free (name);
    return FALSE;
  }

  if (rfxbuilder->num_triggers==0&&rfxbuilder->type==RFX_BUILD_TYPE_UTILITY) {
    do_blocking_error_dialog (_ ("\n\nTrigger code should not be blank for a utility.\n"));
    g_free (name);
    return FALSE;
  }

  g_free (name);
  return TRUE;
}

gboolean perform_param_checks (rfx_build_window_t *rfxbuilder, gint index, gint rows) {
  int i;

  if (!strlen(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->param_name_entry)))) {
    do_blocking_error_dialog (_ ("\n\nParameter name must not be blank.\n"));
    return FALSE;
  }
  for (i=0;i<rows;i++) {
    if (i!=index&&!(strcmp(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->param_name_entry)),
			   rfxbuilder->copy_params[i].name))) {
      do_blocking_error_dialog(_("\n\nDuplicate parameter name detected. Parameter names must be unique in a plugin.\n\n"));
      return FALSE;
    }
  }
  return TRUE;
}


gboolean rfxbuilder_to_script (rfx_build_window_t *rfxbuilder) {
  FILE *sfile;
  int i,j;
  int retval;
  gchar **array;
  lives_colRGB24_t rgb;
  guint32 props;
  gdouble stepwrap;

  gchar *name=g_strdup (gtk_entry_get_text (GTK_ENTRY (rfxbuilder->name_entry)));
  gchar *script_file,*script_file_dir;
  gchar *script_name=g_strdup_printf ("%s.%s",name,RFXBUILDER_SCRIPT_SUFFIX);
  gchar *new_name;
  gchar *buf;
  gchar *msg;
  gchar *tmp,*tmp2;

  if (rfxbuilder->mode!=RFX_BUILDER_MODE_EDIT) {
    if (!(new_name=prompt_for_script_name (script_name,RFX_STATUS_TEST))) return FALSE;
    g_free (script_name);
    script_name=new_name;
  }

  mainw->com_failed=FALSE;

  script_file_dir=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,NULL);


  if (!g_file_test(script_file_dir,G_FILE_TEST_IS_DIR)) {
    if (g_mkdir_with_parents(script_file_dir,S_IRWXU)==-1) return FALSE;
  }

  script_file=g_strdup_printf ("%s%s",script_file_dir,script_name);
  g_free (script_file_dir);
  g_free (script_name);
  msg=g_strdup_printf (_ ("Writing script file %s..."),script_file);
  d_print (msg);
  g_free (msg);

  if (!check_file (script_file,TRUE)) {
    g_free (name);
    g_free (script_file);
    d_print_failed();
    return FALSE;
  }
  
  do {
    retval=0;
    if (!(sfile=fopen(script_file,"w"))) {
      retval=do_write_failed_error_s_with_retry(script_file,g_strerror(errno),GTK_WINDOW(rfxbuilder->dialog));
      if (retval==LIVES_CANCEL) {
	g_free (msg);
	g_free (name);
	g_free (script_file);
	d_print_failed();
	return FALSE;
      }
    }
    else {
  
      mainw->write_failed=FALSE;

      lives_fputs("Script file generated from LiVES\n\n",sfile);
      lives_fputs("<define>\n",sfile);
      lives_fputs(rfxbuilder->field_delim,sfile);
      lives_fputs(rfxbuilder->rfx_version,sfile);
      lives_fputs("\n</define>\n\n",sfile);
      lives_fputs("<name>\n",sfile);
      lives_fputs(name,sfile);
      lives_fputs("\n</name>\n\n",sfile);
      lives_fputs("<version>\n",sfile);
      buf=g_strdup_printf ("%d",gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_version)));
      lives_fputs (buf,sfile);
      g_free (buf);
      lives_fputs("\n</version>\n\n",sfile);
      lives_fputs("<author>\n",sfile);
      lives_fputs(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->author_entry)),sfile);
      lives_fputs(rfxbuilder->field_delim,sfile);
      lives_fputs(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->url_entry)),sfile);
      lives_fputs("\n</author>\n\n",sfile);
      lives_fputs("<description>\n",sfile);
      lives_fputs(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->menu_text_entry)),sfile);
      lives_fputs(rfxbuilder->field_delim,sfile);
      lives_fputs(gtk_entry_get_text (GTK_ENTRY (rfxbuilder->action_desc_entry)),sfile);
      lives_fputs(rfxbuilder->field_delim,sfile);
      if (rfxbuilder->type==RFX_BUILD_TYPE_UTILITY) {
	buf=g_strdup ("-1");
      }
      else {
	buf=g_strdup_printf ("%d",gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_min_frames)));
      }
      lives_fputs (buf,sfile);
      g_free (buf);
      lives_fputs(rfxbuilder->field_delim,sfile);
      switch (rfxbuilder->type) {
      case RFX_BUILD_TYPE_EFFECT2:
	buf=g_strdup ("2");
	break;
      case RFX_BUILD_TYPE_EFFECT0:
	buf=g_strdup ("0");
	break;
      default:
	buf=g_strdup ("1");
      }
      lives_fputs (buf,sfile);
      g_free (buf);
      lives_fputs(rfxbuilder->field_delim,sfile);
      lives_fputs("\n</description>\n\n",sfile);
      lives_fputs("<requires>\n",sfile);
      for (i=0;i<rfxbuilder->num_reqs;i++) {
	lives_fputs(rfxbuilder->reqs[i],sfile);
	if (i<rfxbuilder->num_reqs-1)
	  lives_fputs("\n",sfile);
      }
      lives_fputs("\n</requires>\n\n",sfile);
      lives_fputs("<params>\n",sfile);
      for (i=0;i<rfxbuilder->num_params;i++) {
	lives_fputs(rfxbuilder->params[i].name,sfile);
	lives_fputs(rfxbuilder->field_delim,sfile);
	lives_fputs(rfxbuilder->params[i].label,sfile);
	lives_fputs(rfxbuilder->field_delim,sfile);
	switch (rfxbuilder->params[i].type) {
	case LIVES_PARAM_NUM:
	  stepwrap=rfxbuilder->params[i].step_size;
	  if (rfxbuilder->params[i].wrap) stepwrap=-stepwrap;
	  lives_fputs ("num",sfile);
	  buf=g_strdup_printf ("%d",rfxbuilder->params[i].dp);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  if (!rfxbuilder->params[i].dp) {
	    buf=g_strdup_printf ("%d",get_int_param (rfxbuilder->params[i].def));
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    buf=g_strdup_printf ("%d",(gint)rfxbuilder->params[i].min);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    buf=g_strdup_printf ("%d",(gint)rfxbuilder->params[i].max);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    if (stepwrap!=1.) {
	      buf=g_strdup_printf ("%d",(gint)stepwrap);
	      lives_fputs (buf,sfile);
	      g_free (buf);
	      lives_fputs(rfxbuilder->field_delim,sfile);
	    }
	  }
	  else {
	    gchar *pattern=g_strdup_printf ("%%.%df",rfxbuilder->params[i].dp);
	    buf=g_strdup_printf (pattern,get_double_param (rfxbuilder->params[i].def));
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    buf=g_strdup_printf (pattern,rfxbuilder->params[i].min);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    buf=g_strdup_printf (pattern,rfxbuilder->params[i].max);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    if (stepwrap!=1.) {
	      buf=g_strdup_printf (pattern,stepwrap);
	      lives_fputs (buf,sfile);
	      g_free (buf);
	      lives_fputs(rfxbuilder->field_delim,sfile);
	    }
	    g_free (pattern);
	  }
	  break;
	case LIVES_PARAM_BOOL:
	  lives_fputs ("bool",sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  buf=g_strdup_printf ("%d",get_bool_param (rfxbuilder->params[i].def));
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  if (rfxbuilder->params[i].group!=0) {
	    buf=g_strdup_printf ("%d",rfxbuilder->params[i].group);
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	  }
	  break;
	case LIVES_PARAM_COLRGB24:
	  lives_fputs ("colRGB24",sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  get_colRGB24_param (rfxbuilder->params[i].def,&rgb);
	  buf=g_strdup_printf ("%d",rgb.red);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  buf=g_strdup_printf ("%d",rgb.green);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  buf=g_strdup_printf ("%d",rgb.blue);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  break;
	case LIVES_PARAM_STRING:
	  lives_fputs ("string",sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  lives_fputs ((tmp=U82L (tmp2=subst ((gchar *)rfxbuilder->params[i].def,"\n","\\n"))),sfile);
	  g_free(tmp);
	  g_free(tmp2);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  buf=g_strdup_printf ("%d",(gint)rfxbuilder->params[i].max);
	  lives_fputs (buf,sfile);
	  g_free (buf);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  break;
	case LIVES_PARAM_STRING_LIST:
	  lives_fputs ("string_list",sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  if (rfxbuilder->params[i].def!=NULL) {
	    buf=g_strdup_printf ("%d",get_bool_param (rfxbuilder->params[i].def));
	    lives_fputs (buf,sfile);
	    g_free (buf);
	    lives_fputs(rfxbuilder->field_delim,sfile);
	    for (j=0;j<g_list_length (rfxbuilder->params[i].list);j++) {
	      lives_fputs ((tmp=U82L (tmp2=subst ((gchar *)g_list_nth_data 
						  (rfxbuilder->params[i].list,j),"\n","\\n"))),sfile);
	      g_free(tmp);
	      g_free(tmp2);
	      lives_fputs(rfxbuilder->field_delim,sfile);
	    }
	  }
	  break;
	default:
	  break;
	}
	if (i<rfxbuilder->num_params-1)
	  lives_fputs("\n",sfile);
      }
      lives_fputs("\n</params>\n\n",sfile);
      lives_fputs("<param_window>\n",sfile);
      for (i=0;i<rfxbuilder->num_paramw_hints;i++) {
	lives_fputs(rfxbuilder->paramw_hints[i],sfile);
	if (strlen (rfxbuilder->paramw_hints[i])>strlen (rfxbuilder->field_delim)&&
	    strcmp (rfxbuilder->paramw_hints[i]+strlen (rfxbuilder->paramw_hints[i])
		    -strlen (rfxbuilder->field_delim),rfxbuilder->field_delim)) 
	  lives_fputs(rfxbuilder->field_delim,sfile);
	lives_fputs("\n",sfile);
      }
      lives_fputs("</param_window>\n\n",sfile);
      lives_fputs("<properties>\n",sfile);
      if (rfxbuilder->type==RFX_BUILD_TYPE_TOOL) props=rfxbuilder->props|RFX_PROPS_MAY_RESIZE;
      else props=rfxbuilder->props;
  
      if (rfxbuilder->type!=RFX_BUILD_TYPE_EFFECT0&&(rfxbuilder->props&RFX_PROPS_BATCHG)) rfxbuilder->props^=RFX_PROPS_BATCHG;

      buf=g_strdup_printf ("0x%04X",props);
      lives_fputs (buf,sfile);
      g_free (buf);
      lives_fputs("\n</properties>\n\n",sfile);
      lives_fputs("<language_code>\n",sfile);
      array=g_strsplit(lives_combo_get_active_text (LIVES_COMBO (rfxbuilder->langc_combo))," ",-1);
      lives_fputs (array[0],sfile);
      g_strfreev (array);
      lives_fputs("\n</language_code>\n\n",sfile);
      lives_fputs("<pre>\n",sfile);
      lives_fputs (rfxbuilder->pre_code,sfile);
      if (strlen (rfxbuilder->pre_code)&&strcmp (rfxbuilder->pre_code+strlen (rfxbuilder->pre_code)-1,"\n")) 
	lives_fputs ("\n",sfile);
      lives_fputs("</pre>\n\n",sfile);
      lives_fputs("<loop>\n",sfile);
      lives_fputs (rfxbuilder->loop_code,sfile);
      if (strlen (rfxbuilder->loop_code)&&strcmp (rfxbuilder->loop_code+strlen (rfxbuilder->loop_code)-1,"\n")) 
	lives_fputs ("\n",sfile);
      lives_fputs("</loop>\n\n",sfile);
      lives_fputs("<post>\n",sfile);
      lives_fputs (rfxbuilder->post_code,sfile);
      if (strlen (rfxbuilder->post_code)&&strcmp (rfxbuilder->post_code+strlen (rfxbuilder->post_code)-1,"\n")) 
	lives_fputs ("\n",sfile);
      lives_fputs("</post>\n\n",sfile);
      lives_fputs("<onchange>\n",sfile);
      for (i=0;i<rfxbuilder->num_triggers;i++) {
	int j;
	gint numtok=get_token_count (rfxbuilder->triggers[i].code,'\n');

	buf=rfxbuilder->triggers[i].when?g_strdup_printf ("%d",rfxbuilder->triggers[i].when-1):g_strdup ("init");
	array=g_strsplit(rfxbuilder->triggers[i].code,"\n",-1);
	for (j=0;j<numtok;j++) {
	  lives_fputs (buf,sfile);
	  lives_fputs(rfxbuilder->field_delim,sfile);
	  if (array[j]!=NULL) lives_fputs(array[j],sfile);
	  if (j<numtok-1)
	    lives_fputs("\n",sfile);
	}
	lives_fputs("\n",sfile);
	g_free (buf);
	g_strfreev (array);
      }
      lives_fputs("</onchange>\n\n",sfile);
      fclose (sfile);

      if (mainw->write_failed) {
	mainw->write_failed=FALSE;
	retval=do_write_failed_error_s_with_retry(script_file,NULL,GTK_WINDOW(rfxbuilder->dialog));
	if (retval==LIVES_CANCEL) d_print_file_error_failed();
      }

    }
  } while (retval==LIVES_RETRY);

  g_free(name);
  g_free(script_file);

  if (retval!=LIVES_CANCEL) {
    d_print_done();
    return TRUE;
  }
  return FALSE;
}


gboolean script_to_rfxbuilder (rfx_build_window_t *rfxbuilder, const gchar *script_file) {
  GList *list;
  gchar **array;
  gint num_channels;
  gchar *tmp;
  gint tnum,found;
  gint filled_triggers=0;
  gchar *line;
  gint len;
  gchar *version;

  int i,j;

  rfxbuilder->type=RFX_BUILD_TYPE_EFFECT1;
  if (!(list=get_script_section ("define",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <define> section found in script.\n")));
    return FALSE;
  }
  tmp=(gchar *)g_list_nth_data(list,0);
  if (strlen(tmp)<2) {
    g_list_free_strings(list);
    g_list_free(list);
    g_snprintf(mainw->msg,512,"%s",(_("Bad script version.\n")));
    return FALSE;
  }

  version=g_strdup(tmp+1);
  if (strcmp(version,RFX_VERSION)) {
    g_list_free_strings(list);
    g_list_free(list);
    g_free(version);
    g_snprintf(mainw->msg,512,"%s",(_("Bad script version.\n")));
    return FALSE;
  }
  g_free(version);

  memset(tmp+1,0,1);
  g_free(rfxbuilder->field_delim);
  rfxbuilder->field_delim=g_strdup(tmp);
  g_list_free_strings(list);
  g_list_free(list);

  if (!(list=get_script_section ("name",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <name> section found in script.\n")));
    return FALSE;
  }
  gtk_entry_set_text (GTK_ENTRY (rfxbuilder->name_entry),(gchar *)g_list_nth_data (list,0));
  g_list_free_strings (list);
  g_list_free (list);

  if (!(list=get_script_section ("version",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <version> section found in script.\n")));
    return FALSE;
  }
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_version),
			     (gdouble)atoi ((gchar *)g_list_nth_data (list,0)));
  g_list_free_strings (list);
  g_list_free (list);

  if (!(list=get_script_section ("author",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <author> section found in script.\n")));
    return FALSE;
  }
  array=g_strsplit ((gchar *)g_list_nth_data (list,0),rfxbuilder->field_delim,2);
  gtk_entry_set_text (GTK_ENTRY (rfxbuilder->author_entry),array[0]);
  if (get_token_count ((gchar *)g_list_nth_data (list,0),(int)rfxbuilder->field_delim[0])>1) {
    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->url_entry),array[1]);
  }
  g_strfreev (array);
  g_list_free_strings (list);
  g_list_free (list);


  if (!(list=get_script_section ("description",script_file,TRUE))) {
    g_snprintf(mainw->msg,512,"%s",(_("No <description> section found in script.\n")));
    return FALSE;
  }
  if (get_token_count ((gchar *)g_list_nth_data (list,0),(int)rfxbuilder->field_delim[0])<4) {
    g_snprintf(mainw->msg,512,(_("Bad description. (%s)\n")),(gchar *)g_list_nth_data(list,0));
    g_list_free_strings (list);
    g_list_free (list);
    return FALSE;
  }
  array=g_strsplit ((gchar *)g_list_nth_data (list,0),rfxbuilder->field_delim,-1);
  g_list_free_strings (list);
  g_list_free (list);
  gtk_entry_set_text (GTK_ENTRY (rfxbuilder->menu_text_entry),array[0]);
  gtk_entry_set_text (GTK_ENTRY (rfxbuilder->action_desc_entry),array[1]);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (rfxbuilder->spinbutton_min_frames),(gdouble)atoi (array[2]));
  num_channels=atoi (array[3]);

  if (num_channels==2) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT2;
  }
  if (num_channels==0) {
    rfxbuilder->type=RFX_BUILD_TYPE_EFFECT0;
  }
  if (atoi (array[2])==-1) rfxbuilder->type=RFX_BUILD_TYPE_UTILITY;
  g_strfreev (array);

  rfxbuilder->script_name=g_strdup (script_file);

  if ((list=get_script_section ("requires",script_file,TRUE))) {
    rfxbuilder->num_reqs=g_list_length (list);
    for (i=0;i<g_list_length (list);i++) {
      rfxbuilder->reqs[i]=(gchar *)g_list_nth_data (list,i);
    }
    g_list_free (list);
  }

  rfxbuilder->props=0;

  if ((list=get_script_section ("properties",script_file,TRUE))) {
    if (!strncmp ((gchar *)g_list_nth_data (list,0),"0x",2)||!strncmp ((gchar *)g_list_nth_data (list,0),"0x",2)) {
      rfxbuilder->props=hextodec ((const gchar *)g_list_nth_data (list,0)+2);
    }
    else rfxbuilder->props=atoi ((gchar *)g_list_nth_data (list,0));
    g_list_free_strings (list);
    g_list_free (list);
  }
  if (rfxbuilder->props<0) rfxbuilder->props=0;

  if (rfxbuilder->props&RFX_PROPS_MAY_RESIZE) rfxbuilder->type=RFX_BUILD_TYPE_TOOL;

  if ((list=get_script_section ("params",script_file,TRUE))) {
    gchar *type;

    rfxbuilder->num_params=g_list_length (list);
    rfxbuilder->params=(lives_param_t *)g_malloc (rfxbuilder->num_params*sizeof(lives_param_t));

    for (i=0;i<g_list_length (list);i++) {

      // TODO - error check
      line=(gchar *)g_list_nth_data(list,i);

      len=get_token_count (line,(int)rfxbuilder->field_delim[0]);
      array=g_strsplit(line,rfxbuilder->field_delim,-1);
      g_free (line);
      rfxbuilder->params[i].name=g_strdup (array[0]);
      rfxbuilder->params[i].label=g_strdup (array[1]);
      rfxbuilder->params[i].onchange=FALSE;
      rfxbuilder->params[i].dp=0;
      rfxbuilder->params[i].group=0;

      ////////////////
      rfxbuilder->params[i].desc=NULL;
      rfxbuilder->params[i].use_mnemonic=TRUE;
      rfxbuilder->params[i].step_size=1.;
      rfxbuilder->params[i].interp_func=rfxbuilder->params[i].display_func=NULL;
      rfxbuilder->params[i].hidden=FALSE;
      rfxbuilder->params[i].transition=FALSE;
      rfxbuilder->params[i].wrap=FALSE;
      ////////////////

      type=g_strdup (array[2]);
      if (!strncmp (type,"num",3)) {
	rfxbuilder->params[i].dp=atoi (type+3);
	rfxbuilder->params[i].type=LIVES_PARAM_NUM;
	if (rfxbuilder->params[i].dp) {
	  rfxbuilder->params[i].def=g_malloc (sizint);
	  set_double_param (rfxbuilder->params[i].def,g_strtod (array[3],NULL));
	}
	else {
	  rfxbuilder->params[i].def=g_malloc (sizint);
	  set_int_param (rfxbuilder->params[i].def,atoi (array[3]));
	}
	rfxbuilder->params[i].min=g_strtod (array[4],NULL);
	rfxbuilder->params[i].max=g_strtod (array[5],NULL);
	if (len>6) {
	  rfxbuilder->params[i].step_size=g_strtod(array[6],NULL);
	  if (rfxbuilder->params[i].step_size==0.) rfxbuilder->params[i].step_size=1.;
	  else if (rfxbuilder->params[i].step_size<0.) {
	    rfxbuilder->params[i].step_size=-rfxbuilder->params[i].step_size;
	    rfxbuilder->params[i].wrap=TRUE;
	  }
	}
      }
      else if (!strcmp (type,"colRGB24")) {
	rfxbuilder->params[i].type=LIVES_PARAM_COLRGB24;
	rfxbuilder->params[i].def=g_malloc (3*sizint);
	set_colRGB24_param (rfxbuilder->params[i].def,(gshort)atoi (array[3]),
			    (gshort)atoi (array[4]),(gshort)atoi (array[5]));
      }
      else if (!strcmp (type,"string")) {
	rfxbuilder->params[i].type=LIVES_PARAM_STRING;
	rfxbuilder->params[i].def=subst ((tmp=L2U8(array[3])),"\\n","\n");
	g_free(tmp);
	if (len>4) rfxbuilder->params[i].max=(gdouble)atoi(array[4]);
	else rfxbuilder->params[i].max=1024; // TODO
      }
      else if (!strcmp (type,"string_list")) {
	rfxbuilder->params[i].type=LIVES_PARAM_STRING_LIST;
	rfxbuilder->params[i].def=g_malloc (sizint);
	set_int_param (rfxbuilder->params[i].def,atoi (array[3]));
	if (len>3) {
	  rfxbuilder->params[i].list=array_to_string_list (array,3,len);
	}
	else {
	  rfxbuilder->params[i].list=NULL;
	  set_int_param (rfxbuilder->params[i].def,0);
	}
      }
      else {
	// default is bool
	rfxbuilder->params[i].type=LIVES_PARAM_BOOL;
	rfxbuilder->params[i].def=g_malloc (sizint);
	set_bool_param (rfxbuilder->params[i].def,atoi (array[3]));
	if (len>4) rfxbuilder->params[i].group=atoi(array[4]);
      }
      g_free (type);
      g_strfreev (array);
    }
    g_list_free (list);
  }

  if ((list=get_script_section ("param_window",script_file,TRUE))) {
    rfxbuilder->num_paramw_hints=g_list_length (list);
    for (i=0;i<g_list_length (list);i++) {
      rfxbuilder->paramw_hints[i]=(gchar *)g_list_nth_data (list,i);
    }
    g_list_free (list);
  }

  if ((list=get_script_section ("onchange",script_file,TRUE))) {
    for (i=0;i<g_list_length (list);i++) {
      array=g_strsplit ((gchar *)g_list_nth_data (list,i),rfxbuilder->field_delim,-1);
      if (!strcmp (array[0],"init")) {
	if (!rfxbuilder->has_init_trigger) {
	  rfxbuilder->has_init_trigger=TRUE;
	  rfxbuilder->num_triggers++;
	}
      }
      else if ((tnum=atoi (array[0])+1)<=rfxbuilder->num_params&&tnum>0) {
	if (!rfxbuilder->params[tnum-1].onchange) {
	  rfxbuilder->params[tnum-1].onchange=TRUE;
	  rfxbuilder->num_triggers++;
	}
      }
      else {
	//invalid trigger
	gchar *msg=g_strdup_printf (_("\n\nInvalid trigger (%s)\nfound in script.\n\n"),array[0]);
	do_error_dialog (msg);
	g_free (msg);
      }
      g_strfreev (array);
    }
    //end pass 1
    rfxbuilder->triggers=(rfx_trigger_t *)g_malloc (rfxbuilder->num_triggers*sizeof(rfx_trigger_t));

    for (i=0;i<rfxbuilder->num_triggers;i++) {
      rfxbuilder->triggers[i].when=-1;
      rfxbuilder->triggers[i].code=g_strdup ("");
    }

    filled_triggers=0;
    for (i=0;i<g_list_length (list);i++) {
      array=g_strsplit ((gchar *)g_list_nth_data (list,i),rfxbuilder->field_delim,-1);
      if (!strcmp (array[0],"init")) {
	// find init trigger and concatenate code
	found=filled_triggers;
	for (j=0;j<filled_triggers&&found==filled_triggers;j++) if (rfxbuilder->triggers[j].when==0) found=j;
	if (found==filled_triggers) filled_triggers++;
	if (!strlen (rfxbuilder->triggers[found].code)) {
	  tmp=g_strconcat (rfxbuilder->triggers[found].code,array[1],NULL);
	}
	else {
	  tmp=g_strconcat (rfxbuilder->triggers[found].code,"\n",array[1],NULL);
	}
	g_free (rfxbuilder->triggers[found].code);
	rfxbuilder->triggers[found].when=0;
	rfxbuilder->triggers[found].code=g_strdup (tmp);
	g_free (tmp);
      }
      else if ((tnum=atoi (array[0])+1)<=rfxbuilder->num_params&&tnum>0) {
	// find tnum trigger and concatenate code
	found=filled_triggers;

	for (j=0;j<filled_triggers&&found==filled_triggers;j++) if (rfxbuilder->triggers[j].when==tnum) found=j;
	if (found==filled_triggers) filled_triggers++;

	if (!strlen (rfxbuilder->triggers[found].code)) {
	  tmp=g_strconcat (rfxbuilder->triggers[found].code,array[1],NULL);
	}
	else {
	  tmp=g_strconcat (rfxbuilder->triggers[found].code,"\n",array[1],NULL);
	}
	g_free (rfxbuilder->triggers[found].code);
	rfxbuilder->triggers[found].when=tnum;
	rfxbuilder->triggers[found].code=g_strdup (tmp);
	g_free (tmp);
      }
      g_strfreev (array);
    }
    g_list_free_strings (list);
    g_list_free (list);
  }

  if ((list=get_script_section ("pre",script_file,FALSE))) {
    for (i=0;i<g_list_length (list);i++) {
      tmp=g_strconcat (rfxbuilder->pre_code,g_list_nth_data (list,i),NULL);
      g_free (rfxbuilder->pre_code);
      rfxbuilder->pre_code=tmp;
    }
    g_list_free_strings (list);
    g_list_free (list);
  }

  if ((list=get_script_section ("loop",script_file,FALSE))) {
    for (i=0;i<g_list_length (list);i++) {
      tmp=g_strconcat (rfxbuilder->loop_code,g_list_nth_data (list,i),NULL);
      g_free (rfxbuilder->loop_code);
      rfxbuilder->loop_code=tmp;
    }
    g_list_free_strings (list);
    g_list_free (list);
  }

  if ((list=get_script_section ("post",script_file,FALSE))) {
    for (i=0;i<g_list_length (list);i++) {
      tmp=g_strconcat (rfxbuilder->post_code,g_list_nth_data (list,i),NULL);
      g_free (rfxbuilder->post_code);
      rfxbuilder->post_code=tmp;
    }
    g_list_free_strings (list);
    g_list_free (list);
  }

  return TRUE;
}




GList *get_script_section (const gchar *section, const gchar *file, gboolean strip) {
  FILE *script_file;
  gchar buff[65536];
  GList *list=NULL;
  gchar *line;
  gchar *whole=g_strdup (""),*whole2;
  size_t linelen;

#ifndef IS_MINGW
  gchar *outfile=g_strdup_printf ("%s/rfxsec.%d",g_get_tmp_dir(),getpid());
  gchar *com=g_strdup_printf ("\"%s\" -get \"%s\" \"%s\" > \"%s\"",RFX_BUILDER,section,file,outfile);
#else
  gchar *outfile=g_strdup_printf ("%s\\rfxsec.%d",g_get_tmp_dir(),getpid());
  gchar *com=g_strdup_printf ("perl \"%s\\%s\" -get \"%s\" \"%s\" > \"%s\"",prefs->prefix_dir,RFX_BUILDER,
			      section,file,outfile);
#endif

  mainw->com_failed=FALSE;

  lives_system (com,FALSE);
  g_free (com);

  if (mainw->com_failed) return FALSE;

  if ((script_file=fopen (outfile,"r"))) {
    while (fgets(buff,65536,script_file)) {
      if (buff!=NULL) {
	if (strip) line=(g_strchomp (g_strchug(buff)));
	else line=buff;
	if ((linelen=strlen (line))) {
	  whole2=g_strconcat (whole,line,NULL);
	  if (whole2!=whole) g_free (whole);
	  whole=whole2;
	  if (linelen<(size_t)65535) {
	    list=g_list_append (list, g_strdup (whole));
	    g_free (whole);
	    whole=g_strdup ("");
	  }
	}
      }
    }
    g_free (whole);
  }
  else {
    g_free (outfile);
    return NULL;
  }
  fclose (script_file);
  unlink (outfile);
  g_free (outfile);
  return list;
}





void
on_rebuild_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *com;
  if (!check_builder_programs()) return;

  d_print (_("Rebuilding all RFX scripts...builtin...")); 
  do_threaded_dialog(_("Rebuilding scripts"),FALSE);

  com=g_strdup_printf("%s build_rfx_plugins builtinx \"%s%s%s\" \"%s%s%s\" \"%s/bin\"",prefs->backend_sync,
		      prefs->prefix_dir,
		      PLUGIN_SCRIPTS_DIR,PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS,
		      prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_RENDERED_EFFECTS_BUILTIN,prefs->prefix_dir);
  lives_system(com,TRUE);
  g_free(com);
  d_print (_ ("custom...")); 
  com=g_strdup_printf("%s build_rfx_plugins custom",prefs->backend_sync);
  lives_system(com,FALSE);
  g_free(com);
  d_print (_("test...")); 
  com=g_strdup_printf("%s build_rfx_plugins test",prefs->backend_sync);
  lives_system(com,FALSE);
  g_free(com);


  d_print(_("rebuilding dynamic menu entries..."));
  while (g_main_context_iteration(NULL,FALSE));
  threaded_dialog_spin();
  add_rfx_effects();
  threaded_dialog_spin();
  d_print_done(); 
  threaded_dialog_spin();
  end_threaded_dialog();

  gtk_widget_queue_draw(mainw->LiVES);
  while (g_main_context_iteration(NULL,FALSE));
}




gboolean check_builder_programs (void) {
#ifdef IS_MINGW
  return TRUE;
#endif

  // check our plugin builder routines are executable
  gchar loc[32];
  gchar *msg;

  get_location(RFX_BUILDER,loc,32);
  if (!strlen(loc)) {
    msg=g_strdup_printf(_("\n\nLiVES was unable to find the program %s.\nPlease check this program is in your path and executable.\n"),RFX_BUILDER);
    do_blocking_error_dialog(msg);
    g_free(msg);
    return FALSE;
  }
  get_location(RFX_BUILDER_MULTI,loc,32);
  if (!strlen(loc)) {
    msg=g_strdup_printf(_("\n\nLiVES was unable to find the program %s.\nPlease check this program is in your path and executable.\n"),RFX_BUILDER_MULTI);
    do_blocking_error_dialog(msg);
    g_free(msg);
    return FALSE;
  }
  return TRUE;
}



void
on_delete_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_rfx_status_t status=(lives_rfx_status_t)GPOINTER_TO_INT (user_data);
  gint ret;
  gchar *rfx_script_file,*rfx_script_dir;
  gchar *script_name=prompt_for_script_name (NULL,status);
  gchar *msg;

  if (script_name==NULL) return;  // user cancelled

  if (strlen (script_name)) {
    switch (status) {
    case RFX_STATUS_TEST:
      rfx_script_dir=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
					PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,NULL);
      rfx_script_file=g_build_filename (rfx_script_dir,script_name,NULL);
      break;
    case RFX_STATUS_CUSTOM:
      rfx_script_dir=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
					PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,script_name,NULL);
      rfx_script_file=g_build_filename (rfx_script_dir,script_name,NULL);
      break;
    default:
      // we will not delete builtins
      g_free (script_name);
      return;
    }
    g_free (script_name);

    // double check with user
    msg=g_strdup_printf (_ ("\n\nReally delete RFX script\n%s ?\n\n"),rfx_script_file);
    if (!do_warning_dialog (msg)) {
      g_free (msg);
      g_free (rfx_script_file);
      g_free (rfx_script_dir);
      return;
    }
    g_free (msg);

    msg=g_strdup_printf (_ ("Deleting rfx script %s..."),rfx_script_file);
    d_print (msg);
    g_free (msg);
    if (!(ret=unlink (rfx_script_file))) {
      rmdir(rfx_script_dir);
      d_print_done();
      on_rebuild_rfx_activate (NULL,NULL);
    }
    else {
      d_print_failed();
       msg=g_strdup_printf(_ ("\n\nFailed to delete the script\n%s\nError code was %d\n"),rfx_script_file,ret);
      do_error_dialog (msg);
      g_free (msg);
    }
    g_free (rfx_script_file);
    g_free (rfx_script_dir);
  }
}


void
on_promote_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *rfx_script_from=NULL;
  gchar *rfx_script_to=NULL;
  gchar *rfx_dir_from=NULL;
  gchar *rfx_dir_to=NULL;
  gchar *script_name=prompt_for_script_name (NULL,RFX_STATUS_TEST);
  gchar *msg;
  gint ret=0;
  gboolean failed=TRUE;

  if (script_name==NULL) return;  // user cancelled

  if (strlen (script_name)) {
    rfx_dir_from=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				   PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,NULL);

    rfx_script_from=g_build_filename (rfx_dir_from,script_name,NULL);


    rfx_dir_to=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				 PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,NULL);


    rfx_script_to=g_build_filename (rfx_dir_to,script_name,NULL);

    if (g_file_test (rfx_script_to, G_FILE_TEST_EXISTS)) {
      gchar *msg=g_strdup_printf (_ ("\nCustom script file:\n%s\nalready exists.\nPlease delete it first, or rename the test script.\n"),script_name);
      do_blocking_error_dialog (msg);
      g_free (msg);
      g_free (rfx_dir_from);
      g_free (rfx_script_from);
      g_free (rfx_dir_to);
      g_free (rfx_script_to);
      g_free (script_name);
      return;
    }

    msg=g_strdup_printf (_ ("Promoting rfx test plugin %s to custom..."),script_name);
    d_print (msg);
    g_free (script_name);
    g_free (msg);

    g_mkdir_with_parents(rfx_dir_to,S_IRWXU);

    if (!(ret=rename (rfx_script_from,rfx_script_to))) {
      d_print_done();
      on_rebuild_rfx_activate (NULL,NULL);
      failed=FALSE;
    }
  }

  if (failed) {
    rmdir(rfx_dir_to);
    d_print_failed();
    msg=g_strdup_printf(_ ("\n\nFailed to move the plugin script from\n%s to\n%s\nReturn code was %d (%s)\n"),
			rfx_script_from,rfx_script_to,errno,strerror(errno));
    do_error_dialog (msg);
    g_free (msg);
  }
  else rmdir(rfx_dir_from);

  if (rfx_script_from!=NULL) {
    g_free (rfx_dir_from);
    g_free (rfx_script_from);
    g_free (rfx_dir_to);
    g_free (rfx_script_to);
  }
}



void
on_import_rfx_activate (GtkMenuItem *menuitem, gpointer status) {
  GtkWidget *fileselection = create_fileselection (_ ("Import Script from..."),0,NULL);
  g_signal_connect (GTK_FILE_SELECTION(fileselection)->ok_button, "clicked",G_CALLBACK (on_import_rfx_ok),status);
  gtk_widget_show (fileselection);
}



void
on_export_rfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_rfx_status_t status=(lives_rfx_status_t)GPOINTER_TO_INT (user_data);
  gchar *script_name=prompt_for_script_name (NULL,status);
  GtkWidget *fileselection;
  gchar *tmp;
  
  if (script_name==NULL) return;  // user cancelled

  fileselection = create_fileselection (_ ("Export Script to..."),0,script_name);
  
  g_signal_connect (GTK_FILE_SELECTION(fileselection)->ok_button, "clicked", 
		    G_CALLBACK (on_export_rfx_ok),(gpointer)script_name);
  gtk_file_selection_complete(GTK_FILE_SELECTION(fileselection), 
			      (tmp=g_filename_from_utf8 (script_name,-1,NULL,NULL,NULL)));
  g_free(tmp);
  gtk_widget_show (fileselection);
}


void on_export_rfx_ok (GtkButton *button, gchar *script_name) {
  gchar *filename=g_filename_to_utf8 (gtk_file_selection_get_filename
				      (GTK_FILE_SELECTION(gtk_widget_get_toplevel(GTK_WIDGET(button)))),
				      -1,NULL,NULL,NULL);
  gchar *rfx_script_from;
  gchar *com,*msg,*tmp,*tmp2;
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));

  rfx_script_from=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
				    PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,script_name,NULL);

  msg=g_strdup_printf(_ ("Copying %s to %s..."),rfx_script_from,filename);
  d_print(msg);
  g_free(msg);
#ifndef IS_MINGW
  com=g_strdup_printf("/bin/cp \"%s\" \"%s\"",(tmp=g_filename_from_utf8 (rfx_script_from,-1,NULL,NULL,NULL)),
		      (tmp2=g_filename_from_utf8 (filename,-1,NULL,NULL,NULL)));
#else
  com=g_strdup_printf("cp.exe \"%s\" \"%s\"",(tmp=g_filename_from_utf8 (rfx_script_from,-1,NULL,NULL,NULL)),
		      (tmp2=g_filename_from_utf8 (filename,-1,NULL,NULL,NULL)));
#endif
  if (system(com)) d_print_failed();
  else d_print_done();
  g_free(tmp);
  g_free(tmp2);
  g_free(com);
  g_free(rfx_script_from);
  g_free(filename);
  g_free (script_name);
}


void on_import_rfx_ok (GtkButton *button, gpointer user_data) {
  gshort status=(gshort)GPOINTER_TO_INT (user_data);
  gchar *filename=g_filename_to_utf8 
    (gtk_file_selection_get_filename(GTK_FILE_SELECTION
				     (gtk_widget_get_toplevel(GTK_WIDGET(button)))),-1,NULL,NULL,NULL);
  gchar *rfx_script_to,*rfx_dir_to;
  gchar *com,*msg,*tmp,*tmp2,*tmpx;
  gchar basename[PATH_MAX];

  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));

  g_snprintf (basename,PATH_MAX,"%s",filename);
  get_basename (basename);

  mainw->com_failed=FALSE;

  switch (status) {
  case RFX_STATUS_TEST :
    rfx_dir_to=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,NULL);

    g_mkdir_with_parents((tmp=g_filename_from_utf8(rfx_dir_to,-1,NULL,NULL,NULL)),S_IRWXU);

    g_free(tmp);
    rfx_script_to=g_build_filename(rfx_dir_to,basename,NULL);
    g_free (rfx_dir_to);
    break;
  case RFX_STATUS_CUSTOM :
    rfx_dir_to=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,NULL);

    g_mkdir_with_parents((tmp=g_filename_from_utf8(rfx_dir_to,-1,NULL,NULL,NULL)),S_IRWXU);

    g_free(tmp);
    rfx_script_to=g_build_filename (rfx_dir_to,basename,NULL);
    g_free (rfx_dir_to);
    break;
  default :
    rfx_script_to=g_build_filename (prefs->prefix_dir,PLUGIN_SCRIPTS_DIR,
				    PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS,basename,NULL);
    break;
  }

  if (mainw->com_failed) {
    g_free (rfx_script_to);
    g_free (filename);
    return;
  }

  if (g_file_test (rfx_script_to, G_FILE_TEST_EXISTS)) {
    // needs switch...eventually
    do_blocking_error_dialog ((tmpx=g_strdup_printf 
			       (_("\nCustom script file:\n%s\nalready exists.\nPlease delete it first, or rename the import script.\n"),basename)));
    g_free(tmpx);
    g_free (rfx_script_to);
    g_free (filename);
    return;
  }


  msg=g_strdup_printf(_ ("Copying %s to %s..."),filename,rfx_script_to);
  d_print(msg);
  g_free(msg);
#ifndef IS_MINGW
  com=g_strdup_printf("/bin/cp \"%s\" \"%s\"",(tmp=g_filename_from_utf8(filename,-1,NULL,NULL,NULL)),
		      (tmp2=g_filename_from_utf8(rfx_script_to,-1,NULL,NULL,NULL)));
#else
  com=g_strdup_printf("cp.exe \"%s\" \"%s\"",(tmp=g_filename_from_utf8(filename,-1,NULL,NULL,NULL)),
		      (tmp2=g_filename_from_utf8(rfx_script_to,-1,NULL,NULL,NULL)));
#endif
  g_free(tmp);
  g_free(tmp2);
  if (system(com)) d_print_failed();
  else {
    d_print_done();
    on_rebuild_rfx_activate (NULL,NULL);
  }
  g_free(com);
  g_free(rfx_script_to);
  g_free(filename);
}



gchar *prompt_for_script_name(const gchar *sname, lives_rfx_status_t status) {
  // show dialog to get script name of rfx plugin dependant on type
  // set type to RFX_STATUS_ANY to let user pick type as well
  // return value should be g_free'd after use
  // beware: if the user cancels, return value is NULL

  // sname is suggested name, set it to NULL to prompt for a name from
  // status list

  // in copy mode, there are extra entries and the selected script will be copied
  // to test

  // in rename mode a test script will be copied to another test script

  gchar *name=NULL;
  gchar *from_name;
  gchar *from_status;
  gchar *rfx_script_from;
  gchar *rfx_script_to;
  rfx_build_window_t *rfxbuilder;

  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *script_combo=NULL;
  GtkWidget *name_entry=NULL;
  GtkWidget *script_combo_entry=NULL;
  GtkWidget *status_combo=NULL;
  GtkWidget *status_combo_entry=NULL;
  GtkWidget *dialog;
  GtkWidget *action_area;
  GtkWidget *cancelbutton;

  GList *status_list=NULL;

  gboolean copy_mode=FALSE;
  gboolean rename_mode=FALSE;
  gboolean OK;

  if (status==RFX_STATUS_COPY) {
    copy_mode=TRUE;
    status_list = g_list_append (status_list, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN]));
    status_list = g_list_append (status_list, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM]));
    status_list = g_list_append (status_list, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_TEST]));
  }
  dialog = gtk_dialog_new ();

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
  }
  gtk_widget_modify_bg (dialog, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 200);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);

  vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  add_fill_to_box(LIVES_BOX(vbox));

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  add_fill_to_box(LIVES_BOX(vbox));
  
  if (copy_mode) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Copy RFX Script"));

    status_combo = lives_standard_combo_new (_ ("_From type:    "),TRUE,status_list,LIVES_BOX(hbox),NULL);
    gtk_widget_show_all (hbox);

    status_combo_entry = lives_combo_get_entry(LIVES_COMBO(status_combo));

    g_list_free_strings (status_list);
    g_list_free (status_list);

    label = gtk_label_new (_ ("   Script:    "));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
  }
  else {
    if (status==RFX_STATUS_RENAME) {
      gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Rename test RFX Script"));
      label = gtk_label_new (_ ("From script: "));
      rename_mode=TRUE;
      status=RFX_STATUS_TEST;
    }
    else {
      gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - RFX Script name"));
      label = gtk_label_new (_ ("Script name: "));
    }
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
  }
  
  
  if (sname==NULL||copy_mode||rename_mode) {
    script_combo = lives_combo_new ();
    gtk_widget_show (script_combo);

    name_entry = script_combo_entry = lives_combo_get_entry(LIVES_COMBO(script_combo));
    gtk_box_pack_start (GTK_BOX (hbox), script_combo, TRUE, TRUE, 0);
  }
  if (sname!=NULL||copy_mode||rename_mode) {
    // name_entry becomes a normal gtk_entry
    name_entry=gtk_entry_new();

    if (copy_mode) {
      g_signal_connect (GTK_OBJECT(status_combo),"changed",G_CALLBACK (on_script_status_changed),
			(gpointer)script_combo);
      label = gtk_label_new (_ ("New name: "));
    }
    if (rename_mode) {
      label = gtk_label_new (_ ("New script name: "));
    }
    if (copy_mode||rename_mode) {
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_widget_show (hbox);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
      }
    }
    else {
      gtk_entry_set_text (GTK_ENTRY (name_entry),sname);
    }
    gtk_widget_show (name_entry);
    gtk_box_pack_start (GTK_BOX (hbox), name_entry, TRUE, TRUE, 0);
  }
  gtk_widget_grab_focus (name_entry);
  gtk_entry_set_activates_default (GTK_ENTRY (name_entry), TRUE);

  action_area = GTK_DIALOG (dialog)->action_area;
  gtk_widget_show (action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  copy_script_okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (copy_script_okbutton);

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), copy_script_okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (copy_script_okbutton, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (copy_script_okbutton);

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
		    G_CALLBACK (gtk_true),
		    NULL);

  on_script_status_changed(LIVES_COMBO(status_combo),(gpointer)script_combo);

  do {
    OK=TRUE;
    if (gtk_dialog_run(GTK_DIALOG (dialog))==GTK_RESPONSE_OK) {
      if (name!=NULL) g_free (name);
      name=g_strdup(gtk_entry_get_text (GTK_ENTRY (name_entry)));
      if (copy_mode) {
	if (find_rfx_plugin_by_name (name,RFX_STATUS_TEST)>-1||
	    find_rfx_plugin_by_name (name,RFX_STATUS_CUSTOM)>-1||
	    find_rfx_plugin_by_name (name,RFX_STATUS_BUILTIN)>-1) {
	  do_blocking_error_dialog (_ ("\n\nThere is already a plugin with this name.\nName must be unique.\n"));
	  OK=FALSE;
	}
	//copy selected script to test
	else {
	  from_name=g_strdup(gtk_entry_get_text (GTK_ENTRY (script_combo_entry)));
	  from_status=g_strdup(gtk_entry_get_text (GTK_ENTRY (status_combo_entry)));
	  if (!strcmp (from_status,mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN])) status=RFX_STATUS_BUILTIN;
	  else {
	    if (!strcmp (from_status,mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM])) status=RFX_STATUS_CUSTOM;
	    else status=RFX_STATUS_TEST;
	  }

	  if ((rfxbuilder=make_rfx_build_window (from_name,status))==NULL) {
	    // invalid name
	    OK=FALSE;
	  }

	  g_free (from_name);
	  g_free (from_status);

	  if (OK) {
	    gtk_entry_set_text (GTK_ENTRY (rfxbuilder->name_entry),name);
	    rfxbuilder->mode=RFX_BUILDER_MODE_COPY;
	    gtk_widget_show (rfxbuilder->dialog);
	  }
	}
      }
      if (rename_mode) {
	GList *nmlist=NULL;
	if (name !=NULL&&g_list_find ((nmlist=get_script_list (status)),name)!=NULL) {
	  do_blocking_error_dialog (_ ("\n\nThere is already a test script with this name.\nScript name must be unique.\n"));
	  OK=FALSE;
	}
	else {
	  gint ret;
	  gchar *tmp;

	  from_name=g_strdup(gtk_entry_get_text (GTK_ENTRY (script_combo_entry)));
	  rfx_script_from=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
					    PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,from_name,NULL);
	  rfx_script_to=g_build_filename (capable->home_dir,LIVES_CONFIG_DIR,
					  PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,name,NULL);
	  d_print ((tmp=g_strdup_printf (_ ("Renaming RFX test script %s to %s..."),from_name,name)));
	  g_free(tmp);
	  g_free (from_name);

	  if ((ret=rename (rfx_script_from,rfx_script_to))) {
	    d_print_failed();
	    do_error_dialog ((tmp=g_strdup_printf(_ ("\n\nFailed to move the plugin script from\n%s to\n%s\nReturn code was %d\n"),rfx_script_from,rfx_script_to,ret)));
	    g_free(tmp);
	  }
	  else {
	    d_print_done();
	  }
	  g_free (rfx_script_from);
	  g_free (rfx_script_to);
	}
	if (nmlist!=NULL) {
	  g_list_free_strings(nmlist);
	  g_list_free(nmlist);
	}
      }
    }
  } while (!OK);

  gtk_widget_destroy (dialog);
  return name;
}


void on_script_status_changed (GtkComboBox *status_combo, gpointer user_data) {
  gchar *text=lives_combo_get_active_text(status_combo);
  GList *list=NULL;
  LiVESWidget *script_combo=(LiVESWidget *)user_data;

  if (script_combo==NULL||!LIVES_IS_COMBO (script_combo)) return;

  if (!(strcmp (text,mainw->string_constants[LIVES_STRING_CONSTANT_BUILTIN]))) {
    lives_combo_populate (LIVES_COMBO (script_combo), (list=get_script_list (RFX_STATUS_BUILTIN)));
  }
  else {
    if (!(strcmp (text,mainw->string_constants[LIVES_STRING_CONSTANT_CUSTOM]))) {
      lives_combo_populate (LIVES_COMBO (script_combo), (list=get_script_list (RFX_STATUS_CUSTOM)));
    }
    else {
      if (!(strcmp (text,mainw->string_constants[LIVES_STRING_CONSTANT_TEST]))) {
	lives_combo_populate (LIVES_COMBO (script_combo), (list=get_script_list (RFX_STATUS_TEST)));
      }
    }
  }
  g_free (text);
  if (list!=NULL) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(script_combo),0);
    gtk_widget_set_sensitive(copy_script_okbutton,TRUE);
    g_list_free_strings(list);
    g_list_free(list);
  }
  else {
    lives_combo_set_active_string(LIVES_COMBO(script_combo),"");
    gtk_widget_set_sensitive(copy_script_okbutton,FALSE);
  }
}


GList *get_script_list (gshort status) {
  GList *script_list=NULL;

  switch (status) {
  case RFX_STATUS_TEST :
    script_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS,TRUE,NULL,"script");
    break;
  case RFX_STATUS_CUSTOM :
    script_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS,TRUE,NULL,"script");
    break;
  case RFX_STATUS_BUILTIN :
  case RFX_STATUS_COPY :
    script_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS,TRUE,NULL,"script");
    break;
  }
  return script_list;
}




static void remove_from_parent(GtkWidget *widget) {
  gtk_container_remove(GTK_CONTAINER(widget->parent),widget);
}


void add_rfx_effects(void) {
  // scan render plugin directories, create a rfx array, and add each to the appropriate menu area
  GList *rfx_builtin_list=NULL;
  GList *rfx_custom_list=NULL;
  GList *rfx_test_list=NULL;


  lives_rfx_t *rfx=NULL;
  lives_rfx_t *rendered_fx;

  gchar txt[64]; // menu text

  GtkWidget *rfx_image;
  GtkWidget *menuitem;

  int i,plugin_idx,rfx_slot_count=1;

  int rc_child=0;

  gint tool_posn=RFX_TOOL_MENU_POSN;
  gint rfx_builtin_list_length=0,rfx_custom_list_length=0,rfx_test_list_length=0,rfx_list_length=0;

#ifndef NO_RFX
  gboolean allow_nonex;
#endif

  mainw->has_custom_tools=FALSE;
  mainw->has_custom_gens=FALSE;
  mainw->has_custom_utilities=FALSE;

  // exterminate...all...menuentries....
  // TODO - account for case where we only have apply_realtime (i.e add 1 to builtin count)
  if (mainw->num_rendered_effects_builtin) {
    for (i=0;i<=mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom
	   +mainw->num_rendered_effects_test;i++) {
      if (mainw->rendered_fx!=NULL) {
	if (mainw->rendered_fx[i].menuitem!=NULL) {
	  remove_from_parent(mainw->rendered_fx[i].menuitem);
	  threaded_dialog_spin();
	}
      }
    }
    threaded_dialog_spin();
    if (mainw->rte_separator!=NULL) {
      remove_from_parent (mainw->custom_effects_separator);
      remove_from_parent (mainw->custom_effects_menu);
      remove_from_parent (mainw->custom_effects_submenu);
      remove_from_parent (mainw->custom_gens_menu);
      remove_from_parent (mainw->custom_gens_submenu);
      remove_from_parent (mainw->gens_menu);
      remove_from_parent (mainw->custom_utilities_separator);
      remove_from_parent (mainw->custom_utilities_menu);
      remove_from_parent (mainw->custom_utilities_submenu);
      remove_from_parent (mainw->custom_tools_menu);
      remove_from_parent (mainw->utilities_menu);
      remove_from_parent (mainw->run_test_rfx_menu);
    }
    gtk_widget_queue_draw(mainw->effects_menu);
    while (g_main_context_iteration(NULL,FALSE));
    threaded_dialog_spin();

    if (mainw->rendered_fx!=NULL) rfx_free_all();
  }

  mainw->num_rendered_effects_builtin=mainw->num_rendered_effects_custom=mainw->num_rendered_effects_test=0;


  threaded_dialog_spin();

  make_custom_submenus();

  mainw->run_test_rfx_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->run_test_rfx_submenu), mainw->run_test_rfx_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->run_test_rfx_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_widget_show(mainw->run_test_rfx_menu);

  mainw->custom_effects_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->custom_effects_submenu), mainw->custom_effects_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->custom_effects_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->custom_tools_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->custom_tools_submenu), mainw->custom_tools_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->custom_tools_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

#ifdef IS_MINGW
  //#define NO_RFX
#endif


#ifndef NO_RFX

  // scan rendered effect directories
#ifndef IS_MINGW
  allow_nonex=FALSE;
#else
  allow_nonex=TRUE;
#endif

  rfx_custom_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_CUSTOM,allow_nonex,NULL,NULL);
  rfx_custom_list_length=g_list_length(rfx_custom_list);

  rfx_test_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_TEST,allow_nonex,NULL,NULL);
  rfx_test_list_length=g_list_length(rfx_test_list);
    
  if ((rfx_builtin_list=get_plugin_list (PLUGIN_RENDERED_EFFECTS_BUILTIN,allow_nonex,NULL,NULL))==NULL) {
    do_rendered_fx_dialog();
  }
  else {
    rfx_builtin_list_length=g_list_length(rfx_builtin_list);
  }
#endif

  rfx_list_length=rfx_builtin_list_length+rfx_custom_list_length+rfx_test_list_length;

  threaded_dialog_spin();
    
  rendered_fx=(lives_rfx_t *)g_malloc ((rfx_list_length+1)*sizeof(lives_rfx_t));
  
  // use rfx[0] as "Apply realtime fx"
  rendered_fx[0].name=g_strdup("realtime_fx");
  rendered_fx[0].menu_text=g_strdup (_("_Apply Real Time Effects to Selection"));
  rendered_fx[0].action_desc=g_strdup ("Applying Current Real Time Effects to");

  rendered_fx[0].props=0;
  rendered_fx[0].num_params=0;
  rendered_fx[0].num_in_channels=1;
  rendered_fx[0].menuitem=NULL;
  rendered_fx[0].params=NULL;
  rendered_fx[0].extra=NULL;
  rendered_fx[0].status=RFX_STATUS_WEED;
  rendered_fx[0].is_template=FALSE;
  rendered_fx[0].min_frames=1;
  rendered_fx[0].source=NULL;
  rendered_fx[0].source_type=LIVES_RFX_SOURCE_RFX;

  if (rfx_list_length) {
    gchar *plugin_name;
    GList *define=NULL;
    GList *description=NULL;
    GList *props=NULL;
    GList *rfx_list=rfx_builtin_list;
    gchar *type=g_strdup(PLUGIN_RENDERED_EFFECTS_BUILTIN);
    lives_rfx_status_t status=RFX_STATUS_BUILTIN;
    gint offset=0;
    gchar *def=NULL;
    gchar *tmp;

    for (plugin_idx=0;plugin_idx<rfx_list_length;plugin_idx++) {
      threaded_dialog_spin();
      if (mainw->splash_window==NULL) {
	while (g_main_context_iteration(NULL,FALSE));
      }
      if (plugin_idx==rfx_builtin_list_length) {
	g_free(type);
	type=g_strdup_printf(PLUGIN_RENDERED_EFFECTS_CUSTOM);
	status=RFX_STATUS_CUSTOM;
	rfx_list=rfx_custom_list;
	offset=rfx_builtin_list_length;
      }
      if (plugin_idx==rfx_builtin_list_length+rfx_custom_list_length) {
	g_free(type);
	type=g_strdup(PLUGIN_RENDERED_EFFECTS_TEST);
	status=RFX_STATUS_TEST;
	rfx_list=rfx_test_list;
	offset+=rfx_custom_list_length;
      }
      
      plugin_name=g_strdup((gchar *)g_list_nth_data(rfx_list,plugin_idx-offset));

      if (mainw->splash_window!=NULL) {
	splash_msg((tmp=g_strdup_printf(_("Loading rendered effect %s..."),plugin_name)),.2);
	g_free(tmp);
      }

#ifdef DEBUG_RENDER_FX
      g_print("Checking plugin %s\n",plugin_name);
#endif

      if ((define=plugin_request_by_line(type,plugin_name,"get_define"))==NULL) {
#ifdef DEBUG_RENDER_FX
      g_print("No get_define in %s\n",plugin_name);
#endif

      continue;
      }
      def=g_strdup((gchar *)g_list_nth_data(define,0));
      g_list_free_strings(define);
      g_list_free(define);

      if (strlen(def)<2) {
#ifdef DEBUG_RENDER_FX
	g_print("Invalid get_define in %s\n",plugin_name);
#endif
	g_free(def);
	continue;
      }
      if (strcmp(def+1,RFX_VERSION)) {
#ifdef DEBUG_RENDER_FX
	g_print("Invalid version %s instead of %s in %s\n",def+1,RFX_VERSION,plugin_name);
#endif
	g_free(def);
	continue;
      }
      memset(def+1,0,1);
      
      if ((description=plugin_request_common (type,plugin_name,"get_description",def,TRUE))!=NULL&&
	  (props=plugin_request_common (type,plugin_name,"get_capabilities",def,FALSE))!=NULL&&
	  g_list_length(description)>3) {
	rfx=&rendered_fx[rfx_slot_count++];
	rfx->name=g_strdup(plugin_name);
	memcpy(rfx->delim,def,2);
	rfx->menu_text=g_strdup ((gchar *)g_list_nth_data (description,0));
	rfx->action_desc=g_strdup ((gchar *)g_list_nth_data (description,1));
	if (!(rfx->min_frames=atoi ((gchar *)g_list_nth_data (description,2)))) rfx->min_frames=1;
	rfx->num_in_channels=atoi((gchar *)g_list_nth_data (description,3));
	rfx->status=status;
	rfx->props=atoi ((gchar *)g_list_nth_data(props,0));
	rfx->num_params=0;
	rfx->is_template=FALSE;
	rfx->params=NULL;
	rfx->source=NULL;
	rfx->source_type=LIVES_RFX_SOURCE_RFX;
	rfx->extra=NULL;
	rfx->is_template=FALSE;
	if (!check_rfx_for_lives (rfx)) rfx_slot_count--;
      }
      g_free(plugin_name);
      if (props!=NULL) {
	g_list_free_strings (props);
	g_list_free (props);
	props=NULL;
      }
      if (description!=NULL) {
	g_list_free_strings (description);
	g_list_free (description);
	description=NULL;
      }
      g_free(def);
    }

    if (rfx_builtin_list!=NULL) {
      g_list_free_strings (rfx_builtin_list);
      g_list_free (rfx_builtin_list);
    }
    if (rfx_custom_list!=NULL) {
      g_list_free_strings (rfx_custom_list);
      g_list_free (rfx_custom_list);
    }
    if (rfx_test_list!=NULL) {
      g_list_free_strings (rfx_test_list);
      g_list_free (rfx_test_list);
    }
    g_free(type);
  }

  rfx_slot_count--;
    
  threaded_dialog_spin();
  // sort menu text by alpha order (apart from [0])
  sort_rfx_array (rendered_fx,rfx_slot_count);
  g_free (rendered_fx);

  if (mainw->rte_separator==NULL) {
    mainw->rte_separator=gtk_menu_item_new();
    gtk_widget_set_sensitive (mainw->rte_separator, FALSE);
    gtk_widget_show (mainw->rte_separator);
    
    gtk_container_add (GTK_CONTAINER (mainw->effects_menu), mainw->rte_separator);
  }

  menuitem = gtk_menu_item_new_with_mnemonic (mainw->rendered_fx[0].menu_text);
  gtk_widget_show (menuitem);
  // prepend before mainw->rte_separator
  gtk_menu_shell_prepend (GTK_MENU_SHELL (mainw->effects_menu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);
  gtk_widget_set_tooltip_text( menuitem,_("See: VJ - show VJ keys. Set the realtime effects, and then apply them here."));
  
  gtk_widget_add_accelerator (menuitem, "activate", mainw->accel_group,
			      GDK_e, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (on_realfx_activate),
		    &mainw->rendered_fx[0]);

  mainw->rendered_fx[0].menuitem=menuitem;
  mainw->rendered_fx[0].num_in_channels=1;
  
  gtk_container_add (GTK_CONTAINER (mainw->effects_menu), mainw->custom_effects_submenu);
  
  mainw->custom_effects_separator = gtk_menu_item_new ();
  gtk_widget_set_sensitive (mainw->custom_effects_separator, FALSE);
  
  gtk_container_add (GTK_CONTAINER (mainw->effects_menu), mainw->custom_effects_separator);
  threaded_dialog_spin();
    
  // now we need to add to the effects menu and set a callback
  for (rfx=&mainw->rendered_fx[(plugin_idx=1)];plugin_idx<=rfx_slot_count;rfx=&mainw->rendered_fx[++plugin_idx]) {
    threaded_dialog_spin();
    if (mainw->splash_window==NULL) {
      while (g_main_context_iteration(NULL,FALSE));
    }
    render_fx_get_params (rfx,rfx->name,rfx->status);
    threaded_dialog_spin();
    rfx->source=NULL;
    rfx->extra=NULL;
    rfx->menuitem=NULL;

      switch (rfx->status) {
      case RFX_STATUS_BUILTIN:
	mainw->num_rendered_effects_builtin++;
	break;
      case RFX_STATUS_CUSTOM:
	mainw->num_rendered_effects_custom++;
	break;
      case RFX_STATUS_TEST:
	mainw->num_rendered_effects_test++;
	break;
      default:
	break;
      }
      
    if (!(rfx->props&RFX_PROPS_MAY_RESIZE)&&rfx->min_frames>=0&&rfx->num_in_channels==1) {
      // add resizing effects to tools menu later
      g_snprintf (txt,61,"_%s",_(rfx->menu_text));
      if (rfx->num_params) g_strappend (txt,64,"...");
      menuitem = gtk_image_menu_item_new_with_mnemonic(txt);
      gtk_widget_show(menuitem);
      
      switch (rfx->status) {
      case RFX_STATUS_BUILTIN:
	gtk_container_add (GTK_CONTAINER (mainw->effects_menu), menuitem);
	break;
      case RFX_STATUS_CUSTOM:
	gtk_container_add (GTK_CONTAINER (mainw->custom_effects_menu), menuitem);
	rc_child++;
	break;
      case RFX_STATUS_TEST:
	gtk_container_add (GTK_CONTAINER (mainw->run_test_rfx_menu), menuitem);
	break;
      default:
	break;
      }
      
      if (rfx->props&RFX_PROPS_SLOW) {
	rfx_image = gtk_image_new_from_stock ("gtk-no", GTK_ICON_SIZE_MENU);
      }
      else {
	rfx_image = gtk_image_new_from_stock ("gtk-yes", GTK_ICON_SIZE_MENU);
      }
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), rfx_image);
      
      gtk_widget_show(rfx_image);
      if (rfx->params==NULL) {
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (on_render_fx_activate),
			  (gpointer)rfx);
      }
      else {
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (on_render_fx_pre_activate),
			  (gpointer)rfx);
      }
      if (rfx->min_frames>=0) gtk_widget_set_sensitive(menuitem,FALSE);
      rfx->menuitem=menuitem;
    }
  }


  threaded_dialog_spin();

  // custom effects
  if (rc_child>0) {
    gtk_widget_show (mainw->custom_effects_separator);
    gtk_widget_show (mainw->custom_effects_menu);
    gtk_widget_show (mainw->custom_effects_submenu);
  }
  else {
    gtk_widget_hide (mainw->custom_effects_separator);
    gtk_widget_hide (mainw->custom_effects_menu);
    gtk_widget_hide (mainw->custom_effects_submenu);
  }

  mainw->utilities_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->utilities_submenu), mainw->utilities_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->utilities_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  if (mainw->custom_tools_menu!=NULL) {
    remove_from_parent(mainw->custom_tools_menu);
  }

  mainw->custom_tools_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->custom_tools_submenu), mainw->custom_tools_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->custom_tools_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }


  mainw->gens_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->gens_submenu), mainw->gens_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->gens_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }


  mainw->custom_gens_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->custom_gens_submenu), mainw->custom_gens_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->custom_gens_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_container_add (GTK_CONTAINER (mainw->gens_menu), mainw->custom_gens_submenu);



  mainw->custom_utilities_separator=gtk_menu_item_new();
  gtk_widget_set_sensitive (mainw->custom_utilities_separator, FALSE);

  mainw->custom_utilities_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->custom_utilities_submenu), mainw->custom_utilities_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->custom_utilities_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_container_add (GTK_CONTAINER (mainw->custom_tools_menu), mainw->custom_utilities_separator);
  gtk_container_add (GTK_CONTAINER (mainw->custom_tools_menu), mainw->custom_utilities_submenu);


  threaded_dialog_spin();

  mainw->resize_menuitem=NULL;

  if (rfx_slot_count) {
    for (rfx=&mainw->rendered_fx[(plugin_idx=1)];plugin_idx<=rfx_slot_count;rfx=&mainw->rendered_fx[++plugin_idx]) {
      threaded_dialog_spin();
      if (mainw->splash_window==NULL) {
	while (g_main_context_iteration(NULL,FALSE));
      }
      if ((rfx->props&RFX_PROPS_MAY_RESIZE&&rfx->num_in_channels==1)||rfx->min_frames<0) {
	// add resizing effects to tools menu

	g_snprintf (txt,61,"_%s",_(rfx->menu_text));
	if (rfx->num_params) g_strappend (txt,64,"...");
	menuitem = gtk_menu_item_new_with_mnemonic(txt);
	gtk_widget_show(menuitem);

	if (!strcmp(rfx->name,"resize")) {
	  mainw->resize_menuitem=menuitem;
	}

	switch (rfx->status) {
	case RFX_STATUS_BUILTIN:
	  if (rfx->min_frames>=0) {
	    gtk_menu_shell_insert (GTK_MENU_SHELL (mainw->tools_menu), menuitem,tool_posn++);
	  }
	  else {
	    gtk_container_add (GTK_CONTAINER (mainw->utilities_menu), menuitem);
	    gtk_widget_show (mainw->utilities_menu);
	  }
	  break;
	case RFX_STATUS_CUSTOM:
	  if (rfx->min_frames>=0) {
	    gtk_container_add (GTK_CONTAINER (mainw->custom_tools_menu), menuitem);
	    mainw->has_custom_tools=TRUE;
	  }
	  else {
	    gtk_container_add (GTK_CONTAINER (mainw->custom_utilities_menu), menuitem);
	    mainw->has_custom_utilities=TRUE;
	  }
	  break;
	case RFX_STATUS_TEST:
	  gtk_container_add (GTK_CONTAINER (mainw->run_test_rfx_menu), menuitem);
	  break;
	default:
	  break;
	}

	if (menuitem!=mainw->resize_menuitem) {
	  if (rfx->params==NULL) {
	    g_signal_connect (GTK_OBJECT (menuitem), "activate",
			      G_CALLBACK (on_render_fx_activate),
			      (gpointer)rfx);
	  }
	  else {
	    g_signal_connect (GTK_OBJECT (menuitem), "activate",
			      G_CALLBACK (on_render_fx_pre_activate),
			      (gpointer)rfx);
	  }
	}
	else {
	  mainw->fx_candidates[FX_CANDIDATE_RESIZER].func=g_signal_connect (GTK_OBJECT (menuitem), "activate",
									    G_CALLBACK (on_render_fx_pre_activate),
									    (gpointer)rfx);
	}

	if (rfx->min_frames>=0) gtk_widget_set_sensitive(menuitem,FALSE);
	rfx->menuitem=menuitem;
      }

      else if (rfx->num_in_channels==0) {
	// non-realtime generator

	g_snprintf (txt,61,"_%s",_(rfx->menu_text));
	if (rfx->num_params) g_strappend (txt,64,"...");
	menuitem = gtk_menu_item_new_with_mnemonic(txt);
	gtk_widget_show(menuitem);

	switch (rfx->status) {
	case RFX_STATUS_BUILTIN:
	  gtk_container_add (GTK_CONTAINER (mainw->gens_menu), menuitem);
	  gtk_widget_show (mainw->gens_menu);
	  break;
	case RFX_STATUS_CUSTOM:
	  gtk_container_add (GTK_CONTAINER (mainw->custom_gens_menu), menuitem);
	  mainw->has_custom_gens=TRUE;
	  break;
	case RFX_STATUS_TEST:
	  gtk_container_add (GTK_CONTAINER (mainw->run_test_rfx_menu), menuitem);
	  break;
	default:
	  break;
	}

	if (rfx->params==NULL) {
	  g_signal_connect (GTK_OBJECT (menuitem), "activate",
			    G_CALLBACK (on_render_fx_activate),
			    (gpointer)rfx);
	}
	else {
	  g_signal_connect (GTK_OBJECT (menuitem), "activate",
			    G_CALLBACK (on_render_fx_pre_activate),
			    (gpointer)rfx);
	}
      }
    }
  }


  threaded_dialog_spin();

  if (mainw->has_custom_tools||mainw->has_custom_utilities) {
    gtk_widget_show(mainw->custom_tools_separator);
    gtk_widget_show(mainw->custom_tools_menu);
    gtk_widget_show(mainw->custom_tools_submenu);
  }
  else {
    gtk_widget_hide(mainw->custom_tools_separator);
    gtk_widget_hide(mainw->custom_tools_menu);
    gtk_widget_hide(mainw->custom_tools_submenu);
  }
  if (mainw->has_custom_utilities) {
    if (mainw->has_custom_tools) {
      gtk_widget_show(mainw->custom_utilities_separator);
    }
    else {
      gtk_widget_hide(mainw->custom_utilities_separator);
    }
    gtk_widget_show(mainw->custom_utilities_menu);
    gtk_widget_show(mainw->custom_utilities_submenu);
  }
  else {
    gtk_widget_hide(mainw->custom_utilities_menu);
    gtk_widget_hide(mainw->custom_utilities_submenu);
  }
  if (mainw->has_custom_gens) {
    gtk_widget_show(mainw->custom_gens_menu);
    gtk_widget_show(mainw->custom_gens_submenu);
  }
  else {
    gtk_widget_hide(mainw->custom_gens_menu);
    gtk_widget_hide(mainw->custom_gens_submenu);
  }
  if (mainw->current_file>0&&mainw->playing_file==-1) sensitize();

}

