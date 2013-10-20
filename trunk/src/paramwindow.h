// paramwindow.h
// LiVES
// (c) G. Finch 2004 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_PARAMWINDOW_H
#define HAS_LIVES_PARAMWINDOW_H

typedef struct {
  int usr_number;
  GSList *rbgroup;
  int active_param;
} lives_widget_group_t;


#define RFX_TEXT_MAGIC 80 ///< length at which entry turns into textview
#define RFX_DEF_NUM_MAX 1000000. ///< default param max when not defined


void on_paramwindow_ok_clicked (GtkButton *, lives_rfx_t *);
void on_paramwindow_cancel_clicked (GtkButton *, lives_rfx_t *);
void on_paramwindow_cancel_clicked2 (GtkButton *, lives_rfx_t *);

void on_render_fx_pre_activate (GtkMenuItem *, lives_rfx_t *);
void on_render_fx_activate (GtkMenuItem *, lives_rfx_t *);

void on_fx_pre_activate (lives_rfx_t *, int didx, LiVESWidget *pbox);

boolean make_param_box(GtkVBox *, lives_rfx_t *);

boolean add_param_to_box (GtkBox *, lives_rfx_t *, int param_number, boolean add_slider);
void add_param_label_to_box (GtkBox *, boolean do_trans, const gchar *text);

GSList *add_usrgrp_to_livesgrp (GSList *u2l, GSList *rbgroup, int usr_number);
lives_widget_group_t *livesgrp_from_usrgrp (GSList *u2l, int usrgrp);

void after_boolean_param_toggled (GtkToggleButton *, lives_rfx_t *);
void after_param_value_changed (GtkSpinButton *, lives_rfx_t *);
void after_param_red_changed (GtkSpinButton *, lives_rfx_t *);
void after_param_green_changed (GtkSpinButton *, lives_rfx_t *);
void after_param_blue_changed (GtkSpinButton *, lives_rfx_t *);
void after_param_alpha_changed (GtkSpinButton *, lives_rfx_t *);
boolean after_param_text_focus_changed (GtkWidget *, GtkWidget *, lives_rfx_t *);
void after_param_text_changed (GtkWidget *, lives_rfx_t *);
void after_string_list_changed (GtkComboBox *, lives_rfx_t *);

void on_pwcolsel (GtkButton *, lives_rfx_t *);

char *param_marshall (lives_rfx_t *, boolean with_min_max);
char **param_marshall_to_argv (lives_rfx_t *);
void param_demarshall (lives_rfx_t *, GList *plist, boolean with_min_max, boolean update_widgets);
int set_param_from_list(GList *plist, lives_param_t *param, int pnum, boolean with_min_max, boolean upd);
GList *argv_to_marshalled_list (lives_rfx_t *, int argc, char **argv);

/// object should have g_set_object_data "param_number" set to parameter number
///
/// (0 based, -ve for init onchanges)
GList *do_onchange (GObject *object, lives_rfx_t *) WARN_UNUSED;
GList *do_onchange_init(lives_rfx_t *rfx) WARN_UNUSED;



void update_weed_color_value(weed_plant_t *inst, int pnum, int c1, int c2, int c3, int c4);

void update_visual_params(lives_rfx_t *rfx, boolean update_hidden);



#endif
