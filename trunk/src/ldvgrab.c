// ldvgrab.c
// LiVES
// (c) G. Finch 2006 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// portions of this file (c) Dan Dennedy (dan@dennedy.org)

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/resource.h>

#define RX_CHANNEL 63
#define RAW_BUF_SIZE 4096

unsigned char g_rx_packet[RAW_BUF_SIZE]; /* the received packet data */
int g_rx_length; /* the size of a received packet */
int g_alldone = 0; /* flag to indicate when to quit */
int g_rx_channel = RX_CHANNEL;


//////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "support.h"
#include "ldvinterface.h"
#include "ldvcallbacks.h"

int raw_iso_handler(raw1394handle_t handle, int channel, size_t length, quadlet_t *data) {
  if (length < RAW_BUF_SIZE && channel == g_rx_channel) {
    g_rx_length = length;
    memcpy(g_rx_packet, data, length);
  }
  return 0;
}

/* libraw1394 executes this when there is a bus reset. We'll just keep it 
   simple and quit */
int reset_handler(raw1394handle_t handle, unsigned int generation) {
  raw1394_update_generation(handle, generation);
  on_camquit_clicked(NULL,dvgrabw->cam);
  return 0;
}


raw1394handle_t open_raw1394(void) {
  int numcards;
  struct raw1394_portinfo pinf[16];
  raw1394handle_t handle;
  struct pollfd raw1394_poll;
  
  if (!(handle = raw1394_new_handle())) {
    d_print(_("raw1394 - couldn't get handle"));
    do_error_dialog(_("\nThe ieee1394 driver is not loaded or /dev/raw1394 does not exist.\n"));
    return NULL;
  }

  if ((numcards = raw1394_get_port_info(handle, pinf, 16)) < 0) {
    do_error_dialog(_("\nraw1394 - couldn't get card info.\n"));
    return NULL;
  }

  /* port 0 is the first host adapter card */
  if (raw1394_set_port(handle, 0) < 0) {
    do_error_dialog(_("\nraw1394 - couldn't set port.\n"));
    return NULL;
  }
    
  /* tell libraw1394 the names of our callback functions */
  //raw1394_set_iso_handler(handle, g_rx_channel, raw_iso_handler);
  raw1394_set_bus_reset_handler(handle, reset_handler);
  
  /* poll for leftover events */
  raw1394_poll.fd = raw1394_get_fd(handle);
  raw1394_poll.events = POLLIN;

  while(1) {
    if ( poll( &raw1394_poll, 1, 10) < 1 ) break;
    raw1394_loop_iterate(handle);
  }
  
  /* Starting iso receive */
  /*  if (raw1394_start_iso_rcv(handle, g_rx_channel) < 0) {
    do_error_dialog(_("\nraw1394 - couldn't start iso receive.\n"));
    return NULL;
    }*/
  return handle;
}


void close_raw1394(raw1394handle_t handle) {
  //raw1394_stop_iso_rcv(handle, g_rx_channel);
  raw1394_destroy_handle(handle);
}


void camdest(s_cam *cam) {
  raw1394_destroy_handle(cam->handle);
  free(cam);
}

s_cam *camready (void) {
  gchar *msg;
  rom1394_directory rom_dir;
  int i,j;

  int n_ports;
  struct raw1394_portinfo pinf[ 16 ];

  s_cam *cam=(s_cam *)malloc(sizeof(s_cam));

  cam->device=-1;

#ifdef RAW1394_V_0_8
  cam->handle = raw1394_get_handle();
#else
  cam->handle = raw1394_new_handle();
#endif

  if (!cam->handle) {
    if (!errno) {
      do_error_dialog(_("\nraw1394 device not compatible!\n"));
    } else {
      d_print(_("Couldn't get 1394 handle"));
      do_error_dialog(_("\nIs ieee1394, driver, and raw1394 loaded?\n"));
    }
    return NULL;
  } 

  if ( ( n_ports = raw1394_get_port_info( cam->handle, pinf, 16 ) ) < 0 ) {
      msg=g_strdup_printf(_("raw1394 - failed to get port info: %s.\n"), strerror( errno ));
      raw1394_destroy_handle( cam->handle ); 
      return NULL;;
  }



  for ( j = 0; j < n_ports && cam->device == -1; j++ ) {

    if (raw1394_set_port(cam->handle, j) < 0) {
      msg=g_strdup_printf(_("\nraw1394 - couldn't set port %d !\n"),j);
      d_print(msg);
      g_free(msg);
      continue;
    }
    
    for (i=0; i < raw1394_get_nodecount(cam->handle); ++i) {

      if (rom1394_get_directory(cam->handle, i, &rom_dir) < 0) {
	msg=g_strdup_printf(_("error reading config rom directory for node %d\n"), i);
	d_print(msg);
	g_free(msg);
	continue;
      }

      if ( (rom1394_get_node_type(&rom_dir) == ROM1394_NODE_TYPE_AVC) &&
	   avc1394_check_subunit_type(cam->handle, i, AVC1394_SUBUNIT_TYPE_VCR)) {
	cam->device = i;
	break;
      }
    }
  }

  if (cam->device == -1) {
    do_error_dialog(_("\nLiVES could not find any firewire camera.\nPlease make sure your camera is switched on,\nand check that you have read/write permissions for the camera device\n(generally /dev/raw1394*).\n"));
    raw1394_destroy_handle(cam->handle);
    return NULL;
  }

  return cam;
}






//////////////////////////////////////////////////////////////////////////////////////////////

void camplay(s_cam *cam) {
  avc1394_vcr_play(cam->handle, cam->device);
}

void camstop (s_cam *cam) {
  g_alldone=1;
  avc1394_vcr_stop(cam->handle, cam->device);
}

void camrew (s_cam *cam) {
  avc1394_vcr_rewind(cam->handle, cam->device);
}

void camff (s_cam *cam) {
  avc1394_vcr_forward(cam->handle, cam->device);
}

void campause (s_cam *cam) {
  avc1394_vcr_pause(cam->handle, cam->device);
}

void cameject (s_cam *cam) {
  avc1394_vcr_eject(cam->handle, cam->device);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


gchar *find_free_camfile(gint format) {
  gchar *filename=g_strdup(gtk_entry_get_text(GTK_ENTRY(dvgrabw->filent)));
  int i;
  gchar *fname,*tmp=NULL,*tmp2,*tmp3;

  if (format==CAM_FORMAT_HDV) {
    for (i=1;i<10000;i++) {
      fname=g_strdup_printf("%s%04d.mpg",filename,i);
      if (!g_file_test((tmp=g_strdup_printf("%s/%s",(tmp2=g_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),(tmp3=g_filename_from_utf8(fname,-1,NULL,NULL,NULL)))),G_FILE_TEST_EXISTS)) break;
      g_free(tmp);
      g_free(tmp2);
      g_free(tmp3);
      tmp=NULL;
    }
  }
  else {
    for (i=1;i<1000;i++) {
      fname=g_strdup_printf("%s%03d.dv",filename,i);
      if (!g_file_test((tmp=g_strdup_printf("%s/%s",(tmp2=g_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),(tmp3=g_filename_from_utf8(fname,-1,NULL,NULL,NULL)))),G_FILE_TEST_EXISTS)) break;
      g_free(tmp);
      g_free(tmp2);
      g_free(tmp3);
      tmp=NULL;
    }
  }
  if (tmp!=NULL) g_free(tmp);
  g_free(filename);

  return fname;
}


gboolean rec(s_cam *cam) {
  // returns filename of file being written

  gchar *tmp2,*tmp3,*com;
  gchar *splits;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dvgrabw->split))) splits=g_strdup("-autosplit ");
  else splits=g_strdup("");

  if (cam->format==CAM_FORMAT_DV) {
    // dv format
    com=g_strdup_printf("dvgrab -format raw %s%s/%s >/dev/null 2>&1 &",splits,(tmp2=g_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),(tmp3=g_filename_from_utf8(dvgrabw->filename,-1,NULL,NULL,NULL)));
    dummyvar=system (com);
    g_free(com);
    g_free(tmp2);
    g_free(tmp3);
    g_free(splits);
    return TRUE;
  }

  // hdv format
  com=g_strdup_printf("dvgrab -format mpeg2 %s%s/%s >/dev/null 2>&1 &",splits,(tmp2=g_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),(tmp3=g_filename_from_utf8(dvgrabw->filename,-1,NULL,NULL,NULL)));
  dummyvar=system (com);
  g_free(com);
  g_free(tmp2);
  g_free(tmp3);
  g_free(splits);

  return TRUE;
}



void on_open_fw_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gint type=GPOINTER_TO_INT(user_data); // type 0==dv, type 1==hdv
  s_cam *cam;

  if (type==CAM_FORMAT_DV&&!capable->has_dvgrab) {
    do_dvgrab_error();
    return;
  }

  cam=camready();
  if (cam==NULL) return;

  /*  if (type==CAM_FORMAT_HDV) {
    cam->rec_handle=open_raw1394();
    if (cam->rec_handle==NULL) return;
  }
  else*/ 
  cam->rec_handle=NULL;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }


  dvgrabw = create_camwindow (cam,type);
  dvgrabw->cursor=NULL;
  cam->format=type;
  cam->grabbed_clips=FALSE;
  gtk_widget_show (dvgrabw->window);
  dvgrabw->cam=cam;
}
