// saveplay.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2011
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-host.h"
#include "weed/weed-palettes.h"
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-palettes.h"
#endif

#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "main.h"
#include "callbacks.h"
#include "support.h"
#include "resample.h"
#include "effects-weed.h"
#include "audio.h"
#include "htmsocket.h"
#include "cvirtual.h"


gboolean save_clip_values(gint which) {
  gint asigned;
  gint endian;
  gchar *lives_header;
  int retval;

  if (which==0||which==mainw->scrap_file) return TRUE;

  asigned=!(mainw->files[which]->signed_endian&AFORM_UNSIGNED);
  endian=mainw->files[which]->signed_endian&AFORM_BIG_ENDIAN;
  lives_header=g_build_filename(prefs->tmpdir,mainw->files[which]->handle,"header.lives",NULL);

  do {
    mainw->clip_header=fopen(lives_header,"w");
    
    if (mainw->clip_header==NULL) {
      retval=do_write_failed_error_s_with_retry(lives_header,g_strerror(errno),NULL);
      if (retval==LIVES_CANCEL) {
	g_free(lives_header);
	return FALSE;
      }
    }

    else {
      mainw->files[which]->header_version=LIVES_CLIP_HEADER_VERSION;

      do {
	retval=0;
	save_clip_value(which,CLIP_DETAILS_HEADER_VERSION,&mainw->files[which]->header_version);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_BPP,&mainw->files[which]->bpp);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_FPS,&mainw->files[which]->fps);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_PB_FPS,&mainw->files[which]->pb_fps);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_WIDTH,&mainw->files[which]->hsize);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_HEIGHT,&mainw->files[which]->vsize);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_INTERLACE,&mainw->files[which]->interlace);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_UNIQUE_ID,&mainw->files[which]->unique_id);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_ARATE,&mainw->files[which]->arps);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_PB_ARATE,&mainw->files[which]->arate);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_ACHANS,&mainw->files[which]->achans);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_ASIGNED,&asigned);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_AENDIAN,&endian);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_ASAMPS,&mainw->files[which]->asampsize);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_FRAMES,&mainw->files[which]->frames);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_TITLE,mainw->files[which]->title);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_AUTHOR,mainw->files[which]->author);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_COMMENT,mainw->files[which]->comment);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_PB_FRAMENO,&mainw->files[which]->frameno);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_CLIPNAME,&mainw->files[which]->name);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_FILENAME,&mainw->files[which]->file_name);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_KEYWORDS,mainw->files[which]->keywords);
      } while (FALSE);
      
      if (mainw->com_failed||mainw->write_failed) {
	fclose(mainw->clip_header);
	retval=do_write_failed_error_s_with_retry(lives_header,NULL,NULL);
      }

    }
  } while (retval==LIVES_RETRY);
  
  g_free(lives_header);

  fclose(mainw->clip_header);
  mainw->clip_header=NULL;

  if (retval==LIVES_CANCEL) return FALSE;

  return TRUE;
}


gboolean read_file_details(const gchar *file_name, gboolean is_audio) {
  // get preliminary details

  // is_audio set to TRUE prevents us from checking for images, and deleting the (existing) first frame
  // therefore it is IMPORTANT to set it when loading new audio for an existing clip !

  FILE *infofile;
  int alarm_handle;
  int retval;
  gboolean timeout;
  gchar *tmp,*com=g_strdup_printf("smogrify get_details \"%s\" \"%s\" \"%s\" %d %d",cfile->handle,
				  (tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)),
				  cfile->img_type==IMG_TYPE_JPEG?"jpg":"png",mainw->opening_loc,is_audio);
  g_free(tmp);

  mainw->com_failed=FALSE;
  unlink(cfile->info_file);
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    return FALSE;
  }

  if (mainw->opening_loc) 
    return do_progress_dialog(TRUE,TRUE,_ ("Examining file header"));


  threaded_dialog_spin();

  do {
    retval=0;
    timeout=FALSE;
    clear_mainw_msg();

#define LIVES_LONGER_TIMEOUT  (120 * U_SEC) // 2 minute timeout

    alarm_handle=lives_alarm_set(LIVES_LONGER_TIMEOUT);
    
    while (!((infofile=fopen(cfile->info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
      while (g_main_context_iteration (NULL,FALSE));
      threaded_dialog_spin();
      g_usleep(prefs->sleep_time);
    }
    
    lives_alarm_clear(alarm_handle);
    
    if (!timeout) {
      mainw->read_failed=FALSE;
      lives_fgets(mainw->msg,512,infofile);
      fclose(infofile);
    }

    if (timeout||mainw->read_failed) {
      retval=do_read_failed_error_s_with_retry(cfile->info_file,NULL,NULL);
    }
  } while (retval==LIVES_RETRY);

  threaded_dialog_spin();
  return TRUE;
}

const gchar *get_deinterlace_string(void) {
  if (mainw->open_deint) return "-vf pp=ci";
  else return "";
}


void 
deduce_file(const gchar *file_name, gdouble start, gint end) {
  // this is a utility function to deduce whether we are dealing with a file, 
  // a selection, a backup, or a location
  gchar short_file_name[PATH_MAX];
  mainw->img_concat_clip=-1;

  if (g_strrstr(file_name,"://")!=NULL&&strncmp (file_name,"dvd://",6)) {
    mainw->opening_loc=TRUE;
    open_file(file_name);
    mainw->opening_loc=FALSE;
  }
  else {
    g_snprintf(short_file_name,PATH_MAX,"%s",file_name);
    if (!(strcmp(file_name+strlen(file_name)-4,".lv1"))) {
      restore_file(file_name);
    }
    else {
      open_file_sel(file_name,start,end);
    }
  }
}


void open_file (const gchar *file_name) {
  // this function should be called to open a whole file
  open_file_sel(file_name,0.,0);
}



static gboolean rip_audio_cancelled(gint old_file, weed_plant_t *mt_pb_start_event, 
				    gboolean mt_has_audio_file) {

  if (mainw->cancelled==CANCEL_KEEP) {
    // user clicked "enough"
    mainw->cancelled=CANCEL_NONE;
    return TRUE;
  }

  end_threaded_dialog();

  d_print("\n");
  d_print_cancelled();
  close_current_file(old_file);
  
  mainw->noswitch=FALSE;

  if (mainw->multitrack!=NULL) {
    mainw->multitrack->pb_start_event=mt_pb_start_event;
    mainw->multitrack->has_audio_file=mt_has_audio_file;
  }

  if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
  mainw->file_open_params=NULL;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
  return FALSE;
}


#define AUDIO_FRAMES_TO_READ 100

void open_file_sel(const gchar *file_name, gdouble start, gint frames) {
  gchar *com;
  gint withsound=1;
  gint old_file=mainw->current_file;
  gint new_file=old_file;
  gchar *fname=g_strdup(file_name),*msgstr;
  gint achans,arate,arps,asampsize;
  gint current_file;
  gboolean mt_has_audio_file=TRUE;
  gchar msg[256],loc[256];
  gchar *tmp=NULL;
  gchar *isubfname=NULL;
  const lives_clip_data_t *cdata;

  weed_plant_t *mt_pb_start_event=NULL;

  if (old_file==-1||!cfile->opening) {
    new_file=mainw->first_free_file;

    if (!get_new_handle(new_file,fname)) {
      g_free(fname);
      return;
    }
    g_free(fname);
    
    lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);
    while (g_main_context_iteration(NULL,FALSE));

    if (frames==0) {
      com=g_strdup_printf(_ ("Opening %s"),file_name);
    }
    else {
      com=g_strdup_printf(_ ("Opening %s start time %.2f sec. frames %d"),file_name,start,frames);
    }
    d_print(""); // exhaust "switch" message

    d_print(com);
    g_free(com);
    
    if (!mainw->save_with_sound) {
      d_print(_ (" without sound"));
      withsound=0;
    }
    
    mainw->noswitch=TRUE;
    mainw->current_file=new_file;

    if (mainw->multitrack!=NULL) {
      // set up for opening preview
      mt_pb_start_event=mainw->multitrack->pb_start_event;
      mt_has_audio_file=mainw->multitrack->has_audio_file;
      mainw->multitrack->pb_start_event=NULL;
      mainw->multitrack->has_audio_file=TRUE;
    }

    if (!strcmp(prefs->image_ext,"png")) cfile->img_type=IMG_TYPE_PNG;

    if (prefs->instant_open) {
      cdata=get_decoder_cdata(cfile,prefs->disabled_decoders);
      if (cfile->ext_src!=NULL) {
	lives_decoder_t *dplug=(lives_decoder_t *)cfile->ext_src;
	cfile->opening=TRUE;
	cfile->clip_type=CLIP_TYPE_FILE;
	
	get_mime_type(cfile->type,40,cdata);
	
	if (!prefs->auto_nobord) {
	  cfile->hsize=cdata->frame_width*weed_palette_get_pixels_per_macropixel(cdata->current_palette);
	  cfile->vsize=cdata->frame_height;
	}
	else {
	  cfile->hsize=cdata->width*weed_palette_get_pixels_per_macropixel(cdata->current_palette);
	  cfile->vsize=cdata->height;
	}

	cfile->frames=cdata->nframes;
	
	if (frames>0&&cfile->frames>frames) cfile->frames=frames;
	
	cfile->start=1;
	cfile->end=cfile->frames;
	create_frame_index(mainw->current_file,TRUE,cfile->fps*(start==0?0:start-1),frames==0?cfile->frames:frames);

	cfile->arate=cfile->arps=cdata->arate;
	cfile->achans=cdata->achans;
	cfile->asampsize=cdata->asamps;
	
	cfile->signed_endian=get_signed_endian(cdata->asigned, G_BYTE_ORDER==G_LITTLE_ENDIAN);

	if (cfile->achans>0&&(dplug->decoder->rip_audio)!=NULL&&withsound==1) {
	  // call rip_audio() in the decoder plugin
	  // the plugin gets a chance to do any internal cleanup in rip_audio_cleanup()

	  int64_t stframe=cfile->fps*start+.5;
	  int64_t maxframe=stframe+(frames==0)?cfile->frames:frames;
	  int64_t nframes=AUDIO_FRAMES_TO_READ;
	  gchar *afile=g_strdup_printf("%s/%s/audiodump.pcm",prefs->tmpdir,cfile->handle);

	  msgstr=g_strdup_printf(_("Opening audio for %s"),file_name);

	  if (mainw->playing_file==-1) resize(1);

	  mainw->cancelled=CANCEL_NONE;

	  cfile->opening_only_audio=TRUE;
	  if (mainw->playing_file==-1) do_threaded_dialog(msgstr,TRUE);

	  do {
	    if (stframe+nframes>maxframe) nframes=maxframe-stframe;
	    if (nframes<=0) break;
	    (dplug->decoder->rip_audio)(cdata,afile,stframe,nframes,NULL);
	    threaded_dialog_spin();
	    stframe+=nframes;
	  } while (mainw->cancelled==CANCEL_NONE);

	  if (dplug->decoder->rip_audio_cleanup!=NULL) {
	    (dplug->decoder->rip_audio_cleanup)(cdata);
	  }

	  if (mainw->cancelled!=CANCEL_NONE) {
	    if (!rip_audio_cancelled(old_file,mt_pb_start_event,mt_has_audio_file)) {
	      g_free(afile);
	      return;
	    }
	  }

	  end_threaded_dialog();
	  g_free(msgstr);

	  cfile->opening_only_audio=FALSE;
	  g_free(afile);
	}
	else {
	  cfile->arate=0.;
	  cfile->achans=cfile->asampsize=0;
	}

	cfile->fps=cfile->pb_fps=cdata->fps;
	d_print("\n");
	
	if (cfile->achans==0&&capable->has_mplayer&&withsound==1) {

	  mainw->com_failed=FALSE;

	  // check if we have audio
	  read_file_details(file_name,FALSE);
	  unlink (cfile->info_file);
	  sync();
	  
	  if (mainw->com_failed) return;

	  if (strlen(mainw->msg)>0) add_file_info (cfile->handle,TRUE);

	  if (cfile->achans>0) {
	    // plugin returned no audio, try with mplayer
	    if (mainw->file_open_params==NULL) mainw->file_open_params=g_strdup("");
	    com=g_strdup_printf("smogrify open \"%s\" \"%s\" %d \"%s\" %.2f %d \"%s\"",cfile->handle,
				(tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)),-1,
				prefs->image_ext,start,frames,mainw->file_open_params);

	    unlink (cfile->info_file);
	    lives_system(com,FALSE);
	    g_free(com);
	    g_free(tmp);
	    tmp=NULL;
	    
	    // if we have a quick-opening file, display the first and last frames now
	    // for some codecs this can be helpful since we can locate the last frame while audio is loading
	    if (cfile->clip_type==CLIP_TYPE_FILE&&mainw->playing_file==-1) resize(1);

	    // TODO - check for EOF

	    msgstr=g_strdup_printf(_("Opening audio"),file_name);
	    if (!do_progress_dialog(TRUE,TRUE,msgstr)) {
	      // user cancelled or switched to another clip
	      
	      g_free(msgstr);
	      
	      mainw->opening_frames=-1;
	      
	      if (mainw->multitrack!=NULL) {
		mainw->multitrack->pb_start_event=mt_pb_start_event;
		mainw->multitrack->has_audio_file=mt_has_audio_file;
	      }

	      if (mainw->cancelled==CANCEL_NO_PROPOGATE) {
		lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
		mainw->cancelled=CANCEL_NONE;
		return;
	      }
	      
	      // cancelled
	      // clean up our temp files
	      com=g_strdup_printf("smogrify stopsubsub \"%s\" 2>/dev/null",cfile->handle);
	      lives_system(com,TRUE);
	      g_free(com);
	      if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
	      mainw->file_open_params=NULL;
	      close_current_file(old_file);
	      lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	      return;
	    }
	    if (mainw->error==0) add_file_info (cfile->handle,TRUE);
	    mainw->error=0;
	    g_free(msgstr);
	  }
	}
      }

      save_frame_index(mainw->current_file);

      if (tmp!=NULL) g_free(tmp);
      tmp=NULL;
    }

    
    if (cfile->ext_src!=NULL) {
      if (mainw->open_deint) {
	// override what the plugin says
	cfile->deinterlace=TRUE;
	cfile->interlace=LIVES_INTERLACE_TOP_FIRST; // guessing
	save_clip_value(mainw->current_file,CLIP_DETAILS_INTERLACE,&cfile->interlace);
	if (mainw->com_failed||mainw->write_failed) do_header_write_error(mainw->current_file);
      }
    }

    else {
      // get the file size, etc. (frames is just a guess here)
      if (!read_file_details(file_name,FALSE)) {
	// user cancelled
	close_current_file(old_file);
	mainw->noswitch=FALSE;
	if (mainw->multitrack!=NULL) {
	  mainw->multitrack->pb_start_event=mt_pb_start_event;
	  mainw->multitrack->has_audio_file=mt_has_audio_file;
	}
	if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
	mainw->file_open_params=NULL;
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return;
      }
      unlink (cfile->info_file);
      sync();
      
      // we must set this before calling add_file_info
      cfile->opening=TRUE;
      mainw->opening_frames=-1;

      if (!add_file_info (cfile->handle,FALSE)) {
	close_current_file(old_file);
	mainw->noswitch=FALSE;
	if (mainw->multitrack!=NULL) {
	  mainw->multitrack->pb_start_event=mt_pb_start_event;
	  mainw->multitrack->has_audio_file=mt_has_audio_file;
	}
	if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
	mainw->file_open_params=NULL;
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return;
      }

      if (frames>0&&cfile->frames>frames) cfile->end=cfile->undo_end=cfile->frames=frames;
      
      // be careful, here we switch from mainw->opening_loc to cfile->opening_loc
      if (mainw->opening_loc) {
	cfile->opening_loc=TRUE;
	mainw->opening_loc=FALSE;
      }
    
      if (cfile->f_size>prefs->warn_file_size*1000000.&&mainw->is_ready&&frames==0) {
	gchar *warn=g_strdup_printf(_ ("\nLiVES is not currently optimised for larger file sizes.\nYou are advised (for now) to start with a smaller file, or to use the 'Open File Selection' option.\n(Filesize=%.2fMB)\n\nAre you sure you wish to continue ?"),cfile->f_size/1000000.);
	if (!do_warning_dialog_with_check(warn,WARN_MASK_FSIZE)) {
	  g_free(warn);
	  close_current_file(old_file);
	  mainw->noswitch=FALSE;
	  if (mainw->multitrack!=NULL) {
	    mainw->multitrack->pb_start_event=mt_pb_start_event;
	    mainw->multitrack->has_audio_file=mt_has_audio_file;
	  }
	  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	  return;
	}
	g_free(warn);
	d_print(_ (" - please be patient."));
      }
      
      d_print("\n");
#if defined DEBUG
      g_print("open_file: dpd in\n");
#endif
    }

    // set undo_start and undo_end for preview
    cfile->undo_start=1;
    cfile->undo_end=cfile->frames;

    if (cfile->achans>0) {
      cfile->opening_audio=TRUE;
    }

    // these will get reset as we have no audio file yet, so preserve them
    achans=cfile->achans;
    arate=cfile->arate;
    arps=cfile->arps;
    asampsize=cfile->asampsize;
    cfile->old_frames=cfile->frames;
    cfile->frames=0;

    // we need this FALSE here, otherwise we will switch straight back here...
    cfile->opening=FALSE;

    // force a resize
    current_file=mainw->current_file;

    if (mainw->playing_file>-1) {
      do_quick_switch (current_file);
    }
    else {
      switch_to_file((mainw->current_file=(cfile->clip_type!=CLIP_TYPE_FILE)?old_file:current_file),current_file);
    }

    cfile->opening=TRUE;
    cfile->achans=achans;
    cfile->arate=arate;
    cfile->arps=arps;
    cfile->asampsize=asampsize;
    cfile->frames=cfile->old_frames;

    if (cfile->frames<=0) {
      cfile->undo_end=cfile->frames=123456789;
    }
    if (cfile->hsize*cfile->vsize==0) {
      cfile->frames=0;
    }

    if (mainw->multitrack==NULL) get_play_times();

    add_to_winmenu();
    set_main_title(cfile->file_name,0);

    mainw->effects_paused=FALSE;

    if (cfile->ext_src==NULL) {
      if (mainw->file_open_params==NULL) mainw->file_open_params=g_strdup("");

      tmp=g_strconcat(mainw->file_open_params,get_deinterlace_string(),NULL);
      g_free(mainw->file_open_params);
      mainw->file_open_params=tmp;
      
      com=g_strdup_printf("smogrify open \"%s\" \"%s\" %d \"%s\" %.2f %d \"%s\"",cfile->handle,
			  (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)),withsound,
			  prefs->image_ext,start,frames,mainw->file_open_params);
      unlink (cfile->info_file);
      lives_system(com,FALSE);
      g_free(com);
      g_free(tmp);
      mainw->noswitch=FALSE;
      
      if (mainw->toy_type==LIVES_TOY_TV) {
	// for LiVES TV we do an auto-preview
	mainw->play_start=cfile->start=cfile->undo_start;
	mainw->play_end=cfile->end=cfile->undo_end;
	mainw->preview=TRUE;
	do {
	  desensitize();
	  procw_desensitize();
	  on_playsel_activate (NULL, NULL);
	} while (mainw->cancelled==CANCEL_KEEP_LOOPING);
	mainw->preview=FALSE;
	on_toy_activate(NULL,GINT_TO_POINTER(LIVES_TOY_NONE));
	g_free (mainw->file_open_params);
	mainw->file_open_params=NULL;
	mainw->cancelled=CANCEL_NONE;
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return;
      }
    }


    // TODO - check for EOF



    //  loading:

    // 'entry point' when we switch back
    
    // spin until loading is complete
    // afterwards, mainw->msg will contain file details
    cfile->progress_start=cfile->progress_end=0;
    
    // (also check for cancel)
    msgstr=g_strdup_printf(_("Opening %s"),file_name);
    
    if (cfile->ext_src==NULL&&mainw->toy_type!=LIVES_TOY_TV) {
      if (!do_progress_dialog(TRUE,TRUE,msgstr)) {
	// user cancelled or switched to another clip
	
	g_free(msgstr);
	
	mainw->opening_frames=-1;
	mainw->effects_paused=FALSE;
	
	if (mainw->cancelled==CANCEL_NO_PROPOGATE) {
	  mainw->cancelled=CANCEL_NONE;
	  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	  return;
	}
	
	// cancelled
	// clean up our temp files
	com=g_strdup_printf("smogrify stopsubsub \"%s\" 2>/dev/null",cfile->handle);
	lives_system(com,TRUE);
	g_free(com);
	if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
	mainw->file_open_params=NULL;
	close_current_file(old_file);
	if (mainw->multitrack!=NULL) {
	  mainw->multitrack->pb_start_event=mt_pb_start_event;
	  mainw->multitrack->has_audio_file=mt_has_audio_file;
	}
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return;
      }
    }
    g_free(msgstr);
  }

  if (cfile->ext_src!=NULL&&cfile->achans>0) {
    gchar *afile=g_strdup_printf("%s/%s/audiodump.pcm",prefs->tmpdir,cfile->handle);
    gchar *ofile=g_strdup_printf("%s/%s/audio",prefs->tmpdir,cfile->handle);
    rename(afile,ofile);
    g_free(afile);
    g_free(ofile);
  }

  cfile->opening=cfile->opening_audio=cfile->opening_only_audio=FALSE;
  mainw->opening_frames=-1;
  mainw->effects_paused=FALSE;

#if defined DEBUG
  g_print("Out of dpd\n");
#endif

  if (mainw->multitrack!=NULL) {
    mainw->multitrack->pb_start_event=mt_pb_start_event;
    mainw->multitrack->has_audio_file=mt_has_audio_file;
  }

  // mainw->error is TRUE if we could not open the file
  if (mainw->error) {
    do_blocking_error_dialog(mainw->msg);
    d_print_failed();
    close_current_file(old_file);
    if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
    mainw->file_open_params=NULL;
    lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
    return;
  }

  if (cfile->opening_loc) {
    cfile->changed=TRUE;
    cfile->opening_loc=FALSE;
  }

  else {
    if (prefs->autoload_subs) {
      gchar filename[512];
      gchar *subfname;
      lives_subtitle_type_t subtype=SUBTITLE_TYPE_NONE;

      g_snprintf(filename,512,"%s",file_name);
      get_filename(filename,FALSE); // strip extension
      isubfname=g_strdup_printf("%s.srt",filename);
      if (g_file_test(isubfname,G_FILE_TEST_EXISTS)) {
	subfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.srt",NULL);
	subtype=SUBTITLE_TYPE_SRT;
      }
      else {
	g_free(isubfname);
	isubfname=g_strdup_printf("%s.sub",filename);
	if (g_file_test(isubfname,G_FILE_TEST_EXISTS)) {
	  subfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.sub",NULL);
	  subtype=SUBTITLE_TYPE_SUB;
	}
      }
      if (subtype!=SUBTITLE_TYPE_NONE) {
	com=g_strdup_printf("/bin/cp \"%s\" \"%s\"",isubfname,subfname);
	mainw->com_failed=FALSE;
	lives_system(com,FALSE);
	g_free(com);
	if (!mainw->com_failed) 
	  subtitles_init(cfile,subfname,subtype);
	g_free(subfname);
      }
      else {
	g_free(isubfname);
	isubfname=NULL;
      }
    }
  }


  // now file should be loaded...get full details
  cfile->is_loaded=TRUE;
  if (cfile->ext_src==NULL) add_file_info(cfile->handle,FALSE);
  else {
    add_file_info(NULL,FALSE);
    cfile->f_size=sget_file_size((gchar *)file_name);
  }

  if (cfile->frames<=0) {
    if (cfile->afilesize==0l) {
      // we got neither video nor audio...
      g_snprintf (msg,256,"%s",_ 
		  ("\n\nLiVES was unable to extract either video or audio.\nPlease check the terminal window for more details.\n"));
      get_location ("mplayer",loc,256);
      if (!capable->has_mplayer) {
	g_strappend (msg,256,_ ("\n\nYou may need to install mplayer to open this file.\n"));
      }
      else if (strcmp (prefs->video_open_command,loc)) {
	g_strappend (msg,256,_ ("\n\nPlease check the setting of Video open command in\nTools|Preferences|Decoding\n"));
      }

      do_error_dialog(msg);
      d_print_failed();
      close_current_file(old_file);
      if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
      mainw->file_open_params=NULL;
      lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
      return;
    }
    cfile->frames=0;
  }

  current_file=mainw->current_file;

  if (isubfname!=NULL) {
    tmp=g_strdup_printf(_("Loaded subtitle file: %s\n"),isubfname);
    d_print(tmp);
    g_free(tmp);
    g_free(isubfname);
  }


#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif

  if (prefs->show_recent&&!mainw->is_generating) {
    add_to_recent(file_name,start,frames,mainw->file_open_params);
  }
  if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
  mainw->file_open_params=NULL;

  if (!strcmp(cfile->type,"Frames")||!strcmp(cfile->type,"jpeg")||!strcmp(cfile->type,"png")||!strcmp(cfile->type,"Audio")) {
    cfile->is_untitled=TRUE;
  }

  if (cfile->frames==1&&(!strcmp(cfile->type,"jpeg")||!strcmp(cfile->type,"png"))) {
    if (mainw->img_concat_clip==-1) mainw->img_concat_clip=mainw->current_file;
    else if (prefs->concat_images) {
      // insert this image into our image clip, close this file

      com=g_strdup_printf("smogrify insert \"%s\" \"%s\" %d 1 1 \"%s\" 0 %d %d %d",
			  mainw->files[mainw->img_concat_clip]->handle,
			  mainw->files[mainw->img_concat_clip]->img_type==IMG_TYPE_JPEG?"jpg":"png",
			  mainw->files[mainw->img_concat_clip]->frames,
			  cfile->handle,mainw->files[mainw->img_concat_clip]->frames,
			  mainw->files[mainw->img_concat_clip]->hsize,mainw->files[mainw->img_concat_clip]->vsize);

      lives_system(com,FALSE);
      g_free(com);
      close_current_file(mainw->img_concat_clip);
      cfile->frames++;
      cfile->end++;

      g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames==0?0:1,cfile->frames);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
      g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);
      
      
      g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_start),cfile->frames==0?0:1,cfile->frames);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
      g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);

      // TODO - check for EOF


      return;
    }
  }

  // set new style file details
  if (!save_clip_values(current_file)) {
    close_current_file(old_file);
    return;
  }

  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
  mainw->noswitch=FALSE;

  if (mainw->multitrack==NULL) {
    // update widgets
    switch_to_file((mainw->current_file=0),current_file);
  }
  else {
    lives_mt *multi=mainw->multitrack;
    mainw->multitrack=NULL; // allow getting of afilesize
    reget_afilesize (mainw->current_file);
    mainw->multitrack=multi;
    get_total_time(cfile);
    if (!mainw->is_generating) mainw->current_file=mainw->multitrack->render_file;
    mt_init_clips(mainw->multitrack,current_file,TRUE);
    while (g_main_context_iteration(NULL,FALSE));
    mt_clip_select(mainw->multitrack,TRUE);
  }
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
}



static void save_subs_to_file(file *sfile, gchar *fname) {
  gchar *msg,*ext;
  lives_subtitle_type_t otype,itype;

  if (sfile->subt==NULL) return;

  itype=sfile->subt->type;

  ext=get_extension(fname);

  if (!strcmp(ext,"sub")) otype=SUBTITLE_TYPE_SUB;
  else if (!strcmp(ext,"srt")) otype=SUBTITLE_TYPE_SRT;
  else otype=itype;

  g_free(ext);

  // TODO - use sfile->subt->save_fn
  switch (otype) {
    case SUBTITLE_TYPE_SUB:
      save_sub_subtitles(sfile,(double)(sfile->start-1)/sfile->fps,(double)sfile->end/sfile->fps,(double)(sfile->start-1)/sfile->fps,fname);
      break;

    case SUBTITLE_TYPE_SRT:
      save_srt_subtitles(sfile,(double)(sfile->start-1)/sfile->fps,(double)sfile->end/sfile->fps,(double)(sfile->start-1)/sfile->fps,fname);
      break;
      
    default:
    return;
  }

  msg=g_strdup_printf(_("Subtitles were saved as %s\n"),mainw->subt_save_file);
  d_print(msg);
  g_free(msg);
}






gboolean get_handle_from_info_file(gint index) {
  // called from get_new_handle to get the 'real' file handle
  // because until we know the handle we can't use the normal info file yet

  // return FALSE if we time out

  FILE *infofile;
  int alarm_handle;
  int retval;
  gboolean timeout;

  do {
    retval=0;
    timeout=FALSE;
    clear_mainw_msg();

#define LIVES_MEDIUM_TIMEOUT  (60 * U_SEC) // 60 sec timeout

    alarm_handle=lives_alarm_set(LIVES_MEDIUM_TIMEOUT);

    while (!((infofile=fopen(mainw->first_info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
      g_usleep(prefs->sleep_time);
    }

    lives_alarm_clear(alarm_handle);
  
    if (!timeout) {
      mainw->read_failed=FALSE;
      lives_fgets(mainw->msg,256,infofile);
      fclose(infofile);
    }

    if (timeout || mainw->read_failed) {
      retval=do_read_failed_error_s_with_retry(mainw->first_info_file,NULL,NULL);
    }
  } while (retval==LIVES_RETRY);
  
  if (retval==LIVES_CANCEL) {
    mainw->read_failed=FALSE;
    return FALSE;
  }

  unlink(mainw->first_info_file);

  if (mainw->files[index]==NULL) {
    mainw->files[index]=(file *)(g_malloc(sizeof(file)));
    mainw->files[index]->clip_type=CLIP_TYPE_DISK; // the default
  }
  g_snprintf(mainw->files[index]->handle,256,"%s",mainw->msg);

  return TRUE;
}




void save_frame (GtkMenuItem *menuitem, gpointer user_data) {
  int frame;
  // save a single frame from a clip
  gchar *filt[2];
  gchar *ttl;
  char *filename;

  filt[0]=g_strdup_printf ("*.%s",cfile->img_type==IMG_TYPE_JPEG?"jpg":"png");
  filt[1]=NULL;

  frame=GPOINTER_TO_INT(user_data);

  if (frame>0)
    ttl=g_strdup_printf (_("LiVES: Save Frame %d as..."),frame);

  else
    ttl=g_strdup (_("LiVES: Save Frame as..."));


  filename=choose_file(strlen(mainw->image_dir)?mainw->image_dir:NULL,NULL,filt,GTK_FILE_CHOOSER_ACTION_SAVE,ttl,NULL);

  g_free (filt[0]);
  g_free (ttl);

  if (filename==NULL||!strlen(filename)) return;


  if (!save_frame_inner(mainw->current_file,GPOINTER_TO_INT (frame),filename,-1,-1,FALSE)) return;

  g_snprintf(mainw->image_dir,PATH_MAX,"%s",filename);
  get_dirname(mainw->image_dir);
  if (prefs->save_directories) {
    gchar *tmp;
    set_pref ("image_dir",(tmp=g_filename_from_utf8(mainw->image_dir,-1,NULL,NULL,NULL)));
    g_free(tmp);
  }

}






void save_file (int clip, int start, int end, const char *filename) {
  // save clip from frame start to frame end
  file *sfile=mainw->files[clip],*nfile=NULL;

  gdouble aud_start=0.,aud_end=0.;

  const char *n_file_name;
  gchar *fps_string;
  gchar *extra_params=NULL;
  gchar *redir=g_strdup("1>&2 2>/dev/null");
  gchar *new_stderr_name=NULL;
  gchar *mesg,*bit,*tmp;
  gchar *com;
  gchar *full_file_name=NULL;
  gchar *enc_exec_name;

  int new_stderr=-1;
  int retval;
  gint startframe=1;
  gint current_file=mainw->current_file;
  gint asigned=!(sfile->signed_endian&AFORM_UNSIGNED);
  gint arate;
  gint new_file=-1;

  GError *gerr=NULL;

  struct stat filestat;
  time_t file_mtime=0;

  gulong fsize;

  GtkWidget *hbox;

  gboolean safe_symlinks=prefs->safe_symlinks;
  gboolean not_cancelled;
  gboolean output_exists=FALSE;
  gboolean save_all=FALSE;
  gboolean resb;

  if (start==1&&end==sfile->frames) save_all=TRUE;

  // new handling for save selection:
  // symlink images 1 - n to the encoded frames
  // symlinks are now created in /tmp (for dynebolic)
  // then encode the symlinked frames

  if (filename==NULL) {
    // prompt for encoder type/output format
    if (prefs->show_rdet) {
      gint response;
      rdet=create_render_details(1); // WARNING !! - rdet is global in events.h
      response=gtk_dialog_run(GTK_DIALOG(rdet->dialog));
      gtk_widget_hide (rdet->dialog);
      
      if (response==GTK_RESPONSE_CANCEL) {
	gtk_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
	return;
      }
    }
  }

  // get file extension
  check_encoder_restrictions (TRUE,FALSE,save_all);

  hbox = gtk_hbox_new (FALSE, 0);
  mainw->fx1_bool=TRUE;
  add_suffix_check(GTK_BOX(hbox),prefs->encoder.of_def_ext);
  gtk_widget_show_all(hbox);

  if (filename==NULL) {
    gchar *ttl=g_strdup (_("LiVES: Save Clip as..."));
    do {
      n_file_name=choose_file(mainw->vid_save_dir,NULL,NULL,GTK_FILE_CHOOSER_ACTION_SAVE,ttl,hbox);
      if (n_file_name==NULL) return;
    } while (!strlen(n_file_name));
    g_snprintf(mainw->vid_save_dir,PATH_MAX,"%s",n_file_name);
    get_dirname(mainw->vid_save_dir);
    if (prefs->save_directories) {
      set_pref ("vid_save_dir",(tmp=g_filename_from_utf8(mainw->vid_save_dir,-1,NULL,NULL,NULL)));
      g_free(tmp);
    }
    g_free(ttl);
  }
  else n_file_name=filename;

  //append default extension (if necessary)
  if (!strlen (prefs->encoder.of_def_ext)) {
    // encoder adds its own extension
    if (strrchr(n_file_name,'.')!=NULL) {
      memset (strrchr (n_file_name,'.'),0,1);
    }
  }
  else {
    if (mainw->fx1_bool&&(strlen(n_file_name)<=strlen(prefs->encoder.of_def_ext)||
			  strncmp(n_file_name+strlen(n_file_name)-strlen(prefs->encoder.of_def_ext)-1,".",1)||
			  strcmp(n_file_name+strlen(n_file_name)-strlen(prefs->encoder.of_def_ext),
				 prefs->encoder.of_def_ext))) {
      full_file_name=g_strconcat(n_file_name,".",prefs->encoder.of_def_ext,NULL);
    }
  }

  if (full_file_name==NULL) {
    full_file_name=g_strdup (n_file_name);
  }

  if (filename==NULL) {
    if (!check_file(full_file_name,FALSE)) {
      g_free(full_file_name);
      if (rdet!=NULL) {
	gtk_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
      }
      return;
    }
    sfile->orig_file_name=FALSE;
    if (!strlen (sfile->comment)) {
      g_snprintf (sfile->comment,251,"Created with LiVES");
    }
    if (!do_comments_dialog(sfile,full_file_name)) {
	g_free(full_file_name);
	if (rdet!=NULL) {
	  gtk_widget_destroy (rdet->dialog);
	  g_free(rdet->encoder_name);
	  g_free(rdet);
	  rdet=NULL;
	  if (resaudw!=NULL) g_free(resaudw);
	  resaudw=NULL;
	}
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	return;
    }
  }
  else if (!mainw->osc_auto&&sfile->orig_file_name) {
    gchar *warn=g_strdup(_("Saving your video could lead to a loss of quality !\nYou are strongly advised to 'Save As' to a new file.\n\nDo you still wish to continue ?"));
    if (!do_warning_dialog_with_check(warn,WARN_MASK_SAVE_QUALITY)) {
	g_free(warn);
	g_free(full_file_name);
	if (rdet!=NULL) {
	  gtk_widget_destroy (rdet->dialog);
	  g_free(rdet->encoder_name);
	  g_free(rdet);
	  if (resaudw!=NULL) g_free(resaudw);
	  resaudw=NULL;
	  rdet=NULL;
	}
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	return;
    }
    g_free(warn);
  }



  if (sfile->arate*sfile->achans) {
    aud_start=calc_time_from_frame (clip,start*sfile->arps/sfile->arate);
    aud_end=calc_time_from_frame (clip,(end+1)*sfile->arps/sfile->arate);
  }

  if (!save_all&&!safe_symlinks) {
    // we are saving a selection - make symlinks from a temporary clip

    if ((new_file=mainw->first_free_file)==-1) {
      too_many_files();
      if (rdet!=NULL) {
	gtk_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
      }
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    // create new clip
    if (!get_new_handle(new_file,g_strdup (_("selection")))) {
      if (rdet!=NULL) {
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
      }
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    if (sfile->clip_type==CLIP_TYPE_FILE) {
      mainw->cancelled=CANCEL_NONE;
      cfile->progress_start=1;
      cfile->progress_end=count_virtual_frames(sfile->frame_index,start,end);
      do_threaded_dialog(_("Pulling frames from clip"),TRUE);
      resb=virtual_to_images(clip,start,end,TRUE);
      end_threaded_dialog();
      
      if (mainw->cancelled!=CANCEL_NONE||!resb) {
	mainw->cancelled=CANCEL_USER;
	if (rdet!=NULL) {
	  gtk_widget_destroy (rdet->dialog);
	  g_free(rdet->encoder_name);
	  g_free(rdet);
	  rdet=NULL;
	  if (resaudw!=NULL) g_free(resaudw);
	  resaudw=NULL;
	}
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	if (!resb) d_print_file_error_failed();
	return;
      }
    }

    mainw->effects_paused=FALSE;

    nfile=mainw->files[new_file];
    nfile->hsize=sfile->hsize;
    nfile->vsize=sfile->vsize;
    cfile->progress_start=nfile->start=1;
    cfile->progress_end=nfile->frames=nfile->end=end-start+1;
    nfile->fps=sfile->fps;
    nfile->arps=sfile->arps;
    nfile->arate=sfile->arate;
    nfile->achans=sfile->achans;
    nfile->asampsize=sfile->asampsize;
    nfile->signed_endian=sfile->signed_endian;
    nfile->img_type=sfile->img_type;

    com=g_strdup_printf ("smogrify link_frames \"%s\" %d %d %.8f %.8f %d %d %d %d %d \"%s\"",nfile->handle,
			 start,end,aud_start,aud_end,nfile->arate,nfile->achans,nfile->asampsize,
			 !(nfile->signed_endian&AFORM_UNSIGNED),!(nfile->signed_endian&AFORM_BIG_ENDIAN),sfile->handle);

    unlink(nfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    // TODO - eliminate this
    mainw->current_file=new_file;

    if (mainw->com_failed) {
      lives_system (g_strdup_printf("smogrify close \"%s\"",cfile->handle),TRUE);
      g_free (cfile);
      cfile=NULL;
      if (mainw->first_free_file==-1||mainw->first_free_file>new_file) 
	mainw->first_free_file=new_file;
      switch_to_file(mainw->current_file,current_file);
      d_print_cancelled();
      if (rdet!=NULL) {
	gtk_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
      }
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    // TODO - check for EOF

    cfile->nopreview=TRUE;
    if (!(do_progress_dialog(TRUE,TRUE,_("Linking selection")))) {
      lives_system (g_strdup_printf("smogrify close \"%s\"",cfile->handle),TRUE);
      g_free (cfile);
      cfile=NULL;
      if (mainw->first_free_file==-1||mainw->first_free_file>new_file) 
	mainw->first_free_file=new_file;
      switch_to_file(mainw->current_file,current_file);
      d_print_cancelled();
      if (rdet!=NULL) {
	gtk_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
      }
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    aud_start=calc_time_from_frame (new_file,1)*nfile->arps/nfile->arate;
    aud_end=calc_time_from_frame (new_file,nfile->frames+1)*nfile->arps/nfile->arate;
    cfile->nopreview=FALSE;

  }
  else mainw->current_file=clip; // for encoder restns

  if (rdet!=NULL) rdet->is_encoding=TRUE;


  if (!check_encoder_restrictions(FALSE,FALSE,save_all)) {
    if (!save_all&&!safe_symlinks) {
      lives_system ((com=g_strdup_printf("smogrify close \"%s\"",nfile->handle)),TRUE);
      g_free(com);
      g_free (nfile);
      mainw->files[new_file]=NULL;
      if (mainw->first_free_file==-1||new_file) mainw->first_free_file=new_file;
    }
    else if (!save_all&&safe_symlinks) {
      com=g_strdup_printf("smogrify clear_symlinks \"%s\"",nfile->handle);
      lives_system (com,TRUE);
      g_free (com);
    }
    switch_to_file(mainw->current_file,current_file);
    d_print_cancelled();
    if (rdet!=NULL) {
      gtk_widget_destroy (rdet->dialog);
      g_free(rdet->encoder_name);
      g_free(rdet);
      rdet=NULL;
      if (resaudw!=NULL) g_free(resaudw);
      resaudw=NULL;
    }
    if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
    mainw->subt_save_file=NULL;
    return;
  }

  if (rdet!=NULL) {
    gtk_widget_destroy (rdet->dialog);
    g_free(rdet->encoder_name);
    g_free(rdet);
    rdet=NULL;
    if (resaudw!=NULL) g_free(resaudw);
    resaudw=NULL;
  }


  if (!save_all&&safe_symlinks) {
    // we are saving a selection - make symlinks in /tmp

    startframe=-1;

    if (sfile->clip_type==CLIP_TYPE_FILE) {
      mainw->cancelled=CANCEL_NONE;
      cfile->progress_start=1;
      cfile->progress_end=count_virtual_frames(sfile->frame_index,start,end);

      do_threaded_dialog(_("Pulling frames from clip"),TRUE);
      resb=virtual_to_images(clip,start,end,TRUE);
      end_threaded_dialog();
      
      if (mainw->cancelled!=CANCEL_NONE||!resb) {
	mainw->cancelled=CANCEL_USER;
	if (rdet!=NULL) {
	  gtk_widget_destroy (rdet->dialog);
	  g_free(rdet->encoder_name);
	  g_free(rdet);
	  rdet=NULL;
	  if (resaudw!=NULL) g_free(resaudw);
	  resaudw=NULL;
	}
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	if (!resb) d_print_file_error_failed();
	return;
      }
    }

    com=g_strdup_printf ("smogrify link_frames \"%s\" %d %d %.8f %.8f %d %d %d %d %d",sfile->handle,
			 start,end,aud_start,aud_end,sfile->arate,sfile->achans,sfile->asampsize,
			 !(sfile->signed_endian&AFORM_UNSIGNED),!(sfile->signed_endian&AFORM_BIG_ENDIAN));

    unlink(sfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    mainw->current_file=clip;

    if (mainw->com_failed) {
      com=g_strdup_printf("smogrify clear_symlinks \"%s\"",cfile->handle);
      lives_system (com,TRUE);
      g_free (com);
      cfile->nopreview=FALSE;
      switch_to_file(mainw->current_file,current_file);
      d_print_cancelled();
      if (rdet!=NULL) {
	gtk_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
      }
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    // TODO - check for EOF

    cfile->nopreview=TRUE;
    if (!(do_progress_dialog(TRUE,TRUE,_("Linking selection")))) {
      com=g_strdup_printf("smogrify clear_symlinks \"%s\"",cfile->handle);
      lives_system (com,TRUE);
      g_free (com);
      cfile->nopreview=FALSE;
      switch_to_file(mainw->current_file,current_file);
      d_print_cancelled();
      if (rdet!=NULL) {
	gtk_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
      }
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    aud_start=calc_time_from_frame (clip,1)*sfile->arps/sfile->arate;
    aud_end=calc_time_from_frame (clip,end-start+1)*sfile->arps/sfile->arate;
    cfile->nopreview=FALSE;
  }


  if (save_all) {
    if (sfile->clip_type==CLIP_TYPE_FILE) {
      mainw->cancelled=CANCEL_NONE;
      cfile->progress_start=1;
      cfile->progress_end=count_virtual_frames(sfile->frame_index,1,sfile->frames);
      do_threaded_dialog(_("Pulling frames from clip"),TRUE);
      resb=virtual_to_images(clip,1,sfile->frames,TRUE);
      end_threaded_dialog();
      
      if (mainw->cancelled!=CANCEL_NONE||!resb) {
	mainw->cancelled=CANCEL_USER;
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	if (!resb) d_print_file_error_failed();
	switch_to_file(mainw->current_file,current_file);
	return;
      }
    }
  }

  arate=sfile->arate;

  if (!mainw->save_with_sound||prefs->encoder.of_allowed_acodecs==0) {
    bit=g_strdup(_ (" (with no sound)\n"));
    arate=0;
  }
  else {
    bit=g_strdup("\n");
  }

  if (!save_all) {
    mesg=g_strdup_printf(_ ("Saving frames %d to %d%s as \"%s\" : encoder = %s : format = %s..."),
			 start,end,bit,full_file_name,prefs->encoder.name,prefs->encoder.of_desc);
  } // end selection
  else {
    mesg=g_strdup_printf(_ ("Saving frames 1 to %d%s as \"%s\" : encoder %s : format = %s..."),
			 sfile->frames,bit,full_file_name,prefs->encoder.name,prefs->encoder.of_desc);
  }
  g_free (bit);
  
  if (!sfile->ratio_fps) {
    fps_string=g_strdup_printf ("%.3f",sfile->fps);
  }
  else {
    fps_string=g_strdup_printf ("%.8f",sfile->fps);
  }


  // get extra parameters for saving
  if (prefs->encoder.capabilities&HAS_RFX) {
    if (prefs->encoder.capabilities&ENCODER_NON_NATIVE) {
      com=g_strdup_printf("smogrify save get_rfx %s \"%s%s%s/%s\" %s \"%s\" %d %d %d %d %d %d %.4f %.4f",
			  sfile->handle,prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS,prefs->encoder.name,
			  fps_string,(tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),
			  1,sfile->frames,arate,sfile->achans,sfile->asampsize,asigned,aud_start,aud_end);
      g_free(tmp);
    }
    else {
      com=g_strdup_printf("%s%s%s/%s save get_rfx %s \"\" %s \"%s\" %d %d %d %d %d %d %.4f %.4f",prefs->lib_dir,
			  PLUGIN_EXEC_DIR,PLUGIN_ENCODERS,prefs->encoder.name,sfile->handle,
			  fps_string,(tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),
			  1,sfile->frames,arate,sfile->achans,sfile->asampsize,asigned,aud_start,aud_end);
      g_free(tmp);
    }
    extra_params=plugin_run_param_window(com,NULL,NULL);
    g_free(com);

    if (extra_params==NULL) {
      if (!save_all&&safe_symlinks) {
	com=g_strdup_printf("smogrify clear_symlinks \"%s\"",nfile->handle);
	lives_system (com,TRUE);
	g_free (com);
      }
      if (!save_all&&!safe_symlinks) {
	lives_system ((com=g_strdup_printf("smogrify close \"%s\"",nfile->handle)),TRUE);
	g_free(com);
	g_free(nfile);
	mainw->files[new_file]=NULL;
	if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file)
	  mainw->first_free_file=new_file;
      }
      gtk_widget_destroy(fx_dialog[1]);
      fx_dialog[1]=NULL;
      g_free(mesg);
      g_free(fps_string);
      switch_to_file(mainw->current_file,current_file);
      d_print_cancelled();
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }
  }


  mainw->no_switch_dprint=TRUE;
  d_print (mesg);
  mainw->no_switch_dprint=FALSE;
  g_free (mesg);


  if (prefs->show_gui) {
    // open a file for stderr

    new_stderr_name=g_strdup_printf("%s%s/.debug_out",prefs->tmpdir,cfile->handle);
    g_free(redir);

    do {
      retval=0;
      new_stderr=open(new_stderr_name,O_CREAT|O_RDWR|O_TRUNC|O_SYNC,S_IRUSR|S_IWUSR);
      if (new_stderr<0) {
	retval=do_write_failed_error_s_with_retry(new_stderr_name,g_strerror(errno),NULL);
	if (retval==LIVES_CANCEL) redir=g_strdup("1>&2");
      }
      else {
	redir=g_strdup_printf("1>&2 2>%s",new_stderr_name);
	
	mainw->iochan=g_io_channel_unix_new(new_stderr);
	g_io_channel_set_encoding (mainw->iochan, NULL, NULL);
	g_io_channel_set_buffer_size(mainw->iochan,0);
	g_io_channel_set_flags(mainw->iochan,G_IO_FLAG_NONBLOCK,&gerr);
	if (gerr!=NULL) g_error_free(gerr);
	gerr=NULL;
	
	mainw->optextview=create_output_textview();
      }
    } while (retval==LIVES_RETRY);
  }
  else {
    g_free(redir);
    redir=g_strdup("1>&2");
  }

  if (g_file_test ((tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)), G_FILE_TEST_EXISTS)) {
    stat(tmp,&filestat);
    file_mtime=filestat.st_mtime;
  }
  g_free(tmp);

  // if startframe is -ve, we will use the links created for safe_symlinks - in /tmp
  // for non-safe symlinks, cfile will be our new links file
  // for save_all, cfile will be sfile

  enc_exec_name=g_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS,prefs->encoder.name,NULL);

  if (prefs->encoder.capabilities&ENCODER_NON_NATIVE) {
    com=g_strdup_printf("smogrify save \"%s\" \"%s\" \"%s\" \"%s\" %d %d %d %d %d %d %.4f %.4f %s %s",
			cfile->handle,
			enc_exec_name,fps_string,(tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),
			startframe,cfile->frames,arate,cfile->achans,cfile->asampsize,
			asigned,aud_start,aud_end,(extra_params==NULL)?"":extra_params,redir);
    g_free(tmp);
  }
  else {
    // for native plugins we go via the plugin
    com=g_strdup_printf("\"%s\" save \"%s\" \"\" %s \"%s\" %d %d %d %d %d %d %.4f %.4f %s %s",
			enc_exec_name,cfile->handle,
			fps_string,(tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),
			startframe,cfile->frames,arate,cfile->achans,cfile->asampsize,
			asigned,aud_start,aud_end,(extra_params==NULL?"":extra_params),redir);
    g_free(tmp);
  }
  g_free (fps_string);

  if (extra_params!=NULL) g_free(extra_params);
  extra_params=NULL;

  mainw->effects_paused=FALSE;
  cfile->nokeep=TRUE;
  
  unlink(cfile->info_file);
  mainw->write_failed=FALSE;
  save_file_comments(current_file);
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed||mainw->write_failed) {
    not_cancelled=FALSE;
    mainw->error=TRUE;
  }


  if (!mainw->error) {
    not_cancelled=do_progress_dialog(TRUE,TRUE,_ ("Saving [can take a long time]"));
    mesg=g_strdup (mainw->msg);
    
    if (mainw->iochan!=NULL) {
      // flush last of stdout/stderr from plugin
      
      fsync(new_stderr);
      
      pump_io_chan(mainw->iochan);
      
      g_io_channel_shutdown(mainw->iochan,FALSE,&gerr);
      g_io_channel_unref(mainw->iochan);
      
      if (gerr!=NULL) g_error_free(gerr);
      close(new_stderr);
      unlink(new_stderr_name);
      g_free(new_stderr_name);
      g_free(redir);
    }
    
    mainw->effects_paused=FALSE;
    cfile->nokeep=FALSE;
    
    // TODO *** - concat parameters EOF

    if (prefs->encoder.capabilities&ENCODER_NON_NATIVE) {
      com=g_strdup_printf("smogrify plugin_clear \"%s\" %d %d \"%s%s\" \"%s\" \"%s\"",cfile->handle,1,
			  cfile->frames,prefs->lib_dir,
			  PLUGIN_EXEC_DIR,PLUGIN_ENCODERS,prefs->encoder.name);

      LIVES_DEBUG("com is");
      LIVES_DEBUG(com);
    }
    else {
      com=g_strdup_printf("\"%s\" plugin_clear \"%s\" %d %d \"\" %s \"\"", enc_exec_name,cfile->handle,
			  1, cfile->frames, PLUGIN_ENCODERS);
    }
    
    lives_system(com,FALSE);
    g_free(com);
  }

  g_free(enc_exec_name);

  if (not_cancelled) {
    if (mainw->error) {
      mainw->no_switch_dprint=TRUE;
      d_print_failed();
      mainw->no_switch_dprint=FALSE;
      g_free(full_file_name);
      if (!save_all&&!safe_symlinks) {
	lives_system ((com=g_strdup_printf("smogrify close \"%s\"",cfile->handle)),TRUE);
	g_free(com);
	g_free (cfile);
	cfile=NULL;
	if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file)
	  mainw->first_free_file=mainw->current_file;
      }
      else if (!save_all&&safe_symlinks) {
	com=g_strdup_printf("smogrify clear_symlinks \"%s\"",cfile->handle);
	lives_system (com,TRUE);
	g_free (com);
      }

      switch_to_file(mainw->current_file,current_file);
      do_blocking_error_dialog(mesg);
      g_free (mesg);

      if (mainw->iochan!=NULL) {
	mainw->iochan=NULL;
	g_object_unref(mainw->optextview);
      }

      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      sensitize();
      return;
    }
    g_free (mesg);


    if (g_file_test ((tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)), G_FILE_TEST_EXISTS)) {
      g_free(tmp);
      stat((tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),&filestat);
      if (filestat.st_size>0) output_exists=TRUE;
    }
    if (!output_exists||file_mtime==filestat.st_mtime) {
      g_free(tmp);

      mainw->no_switch_dprint=TRUE;
      d_print_failed();
      mainw->no_switch_dprint=FALSE;
      g_free(full_file_name);
      if (!save_all&&!safe_symlinks) {
	lives_system ((com=g_strdup_printf("smogrify close \"%s\"",cfile->handle)),TRUE);
	g_free(com);
	g_free (cfile);
	cfile=NULL;
	if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) 
	  mainw->first_free_file=mainw->current_file;
      }
      else if (!save_all&&safe_symlinks) {
	com=g_strdup_printf("smogrify clear_symlinks \"%s\"",cfile->handle);
	lives_system (com,TRUE);
	g_free (com);
      }

      switch_to_file(mainw->current_file,current_file);
      do_blocking_error_dialog(_ ("\n\nEncoder error - output file was not created !\n"));

      if (mainw->iochan!=NULL) {
	mainw->iochan=NULL;
	g_object_unref(mainw->optextview);
      }

      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      sensitize();
      return;
    }
    g_free(tmp);

    if (save_all) {

      if (prefs->enc_letterbox) {
	// replace letterboxed frames with maxspect frames
	int iwidth=sfile->ohsize;
	int iheight=sfile->ovsize;
	gboolean bad_header=FALSE;

	com=g_strdup_printf("smogrify mv_mgk \"%s\" %d %d \"%s\" 1",sfile->handle,1,sfile->frames,
			    sfile->img_type==IMG_TYPE_JPEG?"jpg":"png");

	unlink(sfile->info_file);
	lives_system(com,FALSE);

	do_progress_dialog(TRUE,FALSE,_ ("Clearing letterbox"));

    // TODO - check for EOF


	calc_maxspect(sfile->hsize,sfile->vsize,&iwidth,&iheight);

	sfile->hsize=iwidth;
	sfile->vsize=iheight;

	save_clip_value(clip,CLIP_DETAILS_WIDTH,&sfile->hsize);
	if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
	save_clip_value(clip,CLIP_DETAILS_HEIGHT,&sfile->vsize);
	if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
	if (bad_header) do_header_write_error(mainw->current_file);

      }



      g_snprintf(sfile->save_file_name,PATH_MAX,"%s",full_file_name);
      sfile->changed=FALSE;

      // save was successful
      sfile->f_size=sget_file_size (full_file_name);

      if (sfile->is_untitled) {
	sfile->is_untitled=FALSE;
      }
      if (!sfile->was_renamed) {
	set_menu_text(sfile->menuentry,full_file_name,FALSE);
	g_snprintf(sfile->name,256,"%s",full_file_name);
      }
      set_main_title(cfile->name,0);
      add_to_recent (full_file_name,0.,0,NULL);
    }
    else {
      if (!safe_symlinks) {
	lives_system ((com=g_strdup_printf("smogrify close \"%s\"",nfile->handle)),TRUE);
	g_free(com);
	g_free (nfile);
	mainw->files[new_file]=NULL;
	if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) 
	  mainw->first_free_file=new_file;
      }
      else {
	com=g_strdup_printf("smogrify clear_symlinks \"%s\"",cfile->handle);
	lives_system (com,TRUE);
	g_free (com);
      }
    }
  }

  switch_to_file(mainw->current_file,current_file);

  if (mainw->iochan!=NULL) {
    gchar *logfile=g_strdup_printf("%sencoder_log_%d_%d.txt",prefs->tmpdir,getuid(),getgid());
    int logfd;

    // save the logfile in tempdir
    if ((logfd=creat(logfile,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))!=-1) {
      gchar *btext=text_view_get_text(mainw->optextview);
      lives_write(logfd,btext,strlen(btext),TRUE);  // not really important if it fails
      g_free(btext);
      close (logfd);
    }
    g_free(logfile);
    g_object_unref(mainw->optextview);
    mainw->iochan=NULL;
  }

  if (not_cancelled) {
    mainw->no_switch_dprint=TRUE;
    d_print_done();

    // get size of file and show it

    fsize=sget_file_size(full_file_name);
    mesg=g_strdup_printf(_("File size was %.2fMB\n"),(gdouble)fsize/1000000.);
    d_print(mesg);
    g_free(mesg);

    if (mainw->subt_save_file!=NULL) {
      save_subs_to_file(sfile,mainw->subt_save_file);
      g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
    }

    mainw->no_switch_dprint=FALSE;
#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_SUCCESS,
		     (mesg=g_strdup_printf("encode %d \"%s\"",clip,
					   (tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)))));
    g_free(tmp);
    g_free(mesg);
#endif

  }
  g_free(full_file_name);


}



void play_file (void) {
  // play the current clip from 'mainw->play_start' to 'mainw->play_end'
  gint arate;
  gchar *com;
  gchar *com2=g_strdup (" ");
  gchar *com4=g_strdup (" ");
  gchar *com3=g_strdup (" ");
  gchar *stopcom=NULL;
  gchar *msg;
  gchar *stfile;

  unsigned int wid;
  gint asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
  gint aendian=!(cfile->signed_endian&AFORM_BIG_ENDIAN);

  gint current_file=mainw->current_file;
  gint audio_end=0;

  gint loop=0;
  gboolean mute;

  GClosure *freeze_closure;
  gshort audio_player=prefs->audio_player;

  weed_plant_t *pb_start_event=NULL;
  gboolean exact_preview=FALSE;
  gboolean has_audio_buffers=FALSE;

#ifdef RT_AUDIO
  gchar *tmpfilename=NULL;
#endif

  if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE) mainw->aud_file_to_kill=mainw->current_file;
  else mainw->aud_file_to_kill=-1;


#ifdef ENABLE_JACK
  if (!mainw->preview&&!mainw->foreign) jack_pb_start();
#endif

  if (mainw->multitrack==NULL) mainw->must_resize=FALSE;
  mainw->ext_playback=FALSE;
  mainw->deltaticks=0;

  mainw->rec_aclip=-1;

  if (mainw->pre_src_file==-2) mainw->pre_src_file=mainw->current_file;

  // enable the freeze button
  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_BackSpace, GDK_CONTROL_MASK, 0, (freeze_closure=g_cclosure_new (G_CALLBACK (freeze_callback),NULL,NULL)));

  if (mainw->multitrack!=NULL) {
    mainw->event_list=mainw->multitrack->event_list;
    pb_start_event=mainw->multitrack->pb_start_event;
    exact_preview=mainw->multitrack->exact_preview;
  }

  if (mainw->record) {
    if (mainw->preview) {
      mainw->record=FALSE;
      d_print (_ ("recording aborted by preview.\n"));
    }
    else if (mainw->current_file==0) {
      mainw->record=FALSE;
      d_print (_ ("recording aborted by clipboard playback.\n"));
    }
    else {
      d_print(_ ("Recording performance..."));
      mainw->clip_switched=FALSE;
      // TODO
      if (mainw->current_file>0&&(cfile->undo_action==UNDO_RESAMPLE||cfile->undo_action==UNDO_RENDER)) {
	gtk_widget_set_sensitive (mainw->undo,FALSE);
	gtk_widget_set_sensitive (mainw->redo,FALSE);
	cfile->undoable=cfile->redoable=FALSE;
      }
    }
  }
  // set performance at right place
  else if (mainw->event_list!=NULL) cfile->next_event=get_first_event(mainw->event_list);

  // note, here our start is in frames, in save_file it is in seconds !
  // TODO - check if we can change it to seconds here too

  mainw->audio_start=mainw->audio_end=0;

  if (mainw->event_list!=NULL) {
    // play performance data
    if (event_list_get_end_secs (mainw->event_list)>cfile->frames/cfile->fps&&!mainw->playing_sel) {
      mainw->audio_end=(event_list_get_end_secs (mainw->event_list)*cfile->fps+1.)*cfile->arate/cfile->arps;
    }
  }
  
  if (mainw->audio_end==0) {
    mainw->audio_start=(calc_time_from_frame(mainw->current_file,mainw->play_start)*cfile->fps+1.)*cfile->arate/cfile->arps;
    mainw->audio_end=(calc_time_from_frame(mainw->current_file,mainw->play_end)*cfile->fps+1.)*cfile->arate/cfile->arps;
    if (!mainw->playing_sel) {
      mainw->audio_end=0;
    }
  }

  find_when_to_stop();

  if (!cfile->opening_audio&&!mainw->loop) {
    // if we are opening audio or looping we just play to the end of audio,
    // otherwise...
    audio_end=mainw->audio_end;
  }

  if (prefs->stop_screensaver) {
    g_free (com2);
    com2=g_strdup ("xset s off 2>/dev/null; xset -dpms 2>/dev/null; gconftool-2 --set --type bool /apps/gnome-screensaver/idle_activation_enabled false 2>/dev/null;");
  }
  if (prefs->pause_xmms&&cfile->achans>0&&!mainw->mute&&capable->has_xmms) {
    g_free (com3);
    com3=g_strdup ("xmms -u;");
  }
  if (!mainw->foreign&&prefs->midisynch&&!mainw->preview) {
    g_free (com4);
    com4=g_strdup  ("midistart");
  }
  com=g_strconcat (com2,com3,com4,NULL);
  if (strlen (com)) {
    // allow this to fail - not all sub-commands may be present
    lives_system (com,TRUE);
  }
  g_free (com); g_free (com2); g_free (com3); g_free (com4);
  com4=g_strdup (" ");
  com3=NULL;
  com2=NULL;
  com=NULL;

  if (mainw->multitrack==NULL) {
    if (!mainw->preview) {
      gtk_frame_set_label(GTK_FRAME(mainw->playframe),_ ("Play"));
    }
    else {
      gtk_frame_set_label(GTK_FRAME(mainw->playframe),_ ("Preview"));
    }
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(gtk_frame_get_label_widget(GTK_FRAME(mainw->playframe)), 
			   GTK_STATE_NORMAL, &palette->normal_fore);
    }
    
    // blank the background if asked to
    if ((mainw->faded||(mainw->fs&&!mainw->sep_win))&&(cfile->frames>0||mainw->foreign)) {
      fade_background();
    }

    if ((!mainw->sep_win||(!mainw->faded&&(prefs->sepwin_type!=1)))&&(cfile->frames>0||mainw->foreign)) {
      // show the frame in the main window
      gtk_widget_show(mainw->playframe);
    }
  }

  if (mainw->multitrack==NULL) {
    // plug the plug into the playframe socket if we need to
    add_to_playframe();
  }
  
  arate=cfile->arate;

  mute=mainw->mute;

  if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE) {
    if (cfile->achans==0||mainw->is_rendering) mainw->mute=TRUE;
    if (mainw->mute&&!cfile->opening_only_audio) arate=arate?-arate:-1;
  }

  if (mainw->sep_win) {
    wid=0;
  }
  else {
    wid=(unsigned int)mainw->xwin;
  }
  
  cfile->frameno=mainw->play_start;
  cfile->pb_fps=cfile->fps;
  if (mainw->reverse_pb) {
    cfile->pb_fps=-cfile->pb_fps;
    cfile->frameno=mainw->play_end;
  }
  mainw->reverse_pb=FALSE;

  cfile->play_paused=FALSE;
  mainw->period=U_SEC/cfile->pb_fps;

  if (audio_player==AUD_PLAYER_JACK) audio_cache_init();

  if (mainw->blend_file!=-1&&mainw->files[mainw->blend_file]==NULL) mainw->blend_file=-1;

  if (mainw->num_tr_applied>0&&!mainw->preview&&mainw->blend_file>-1) {
    // reset frame counter for blend_file
    mainw->files[mainw->blend_file]->frameno=1;
    mainw->files[mainw->blend_file]->aseek_pos=0;
  }

  gtk_widget_set_sensitive(mainw->m_stopbutton,TRUE);
  mainw->playing_file=mainw->current_file;

  if (!mainw->preview||!cfile->opening) {
    desensitize();
  }

  if (mainw->record) {
    if (mainw->event_list!=NULL) event_list_free (mainw->event_list);
    mainw->event_list=add_filter_init_events(NULL,0);
  }

  if (mainw->double_size&&mainw->multitrack==NULL) {
    gtk_widget_hide(mainw->scrolledwindow);
  }

  gtk_widget_set_sensitive (mainw->stop, TRUE);

  if (mainw->multitrack==NULL) gtk_widget_set_sensitive (mainw->m_playbutton, FALSE);
  else if (!cfile->opening) {
    if (!mainw->is_processing) mt_swap_play_pause(mainw->multitrack,TRUE);
    else {
      gtk_widget_set_sensitive(mainw->multitrack->playall,FALSE);
      gtk_widget_set_sensitive(mainw->m_playbutton,FALSE);
    }
  }

  gtk_widget_set_sensitive (mainw->m_playselbutton, FALSE);
  gtk_widget_set_sensitive (mainw->m_rewindbutton, FALSE);
  gtk_widget_set_sensitive (mainw->m_mutebutton, (audio_player==AUD_PLAYER_JACK||audio_player==AUD_PLAYER_PULSE||
						  mainw->multitrack!=NULL));

  gtk_widget_set_sensitive (mainw->m_loopbutton, (!cfile->achans||mainw->mute||mainw->multitrack!=NULL||
						  mainw->loop_cont||audio_player==AUD_PLAYER_JACK||
						  audio_player==AUD_PLAYER_PULSE)&&mainw->current_file>0);
  gtk_widget_set_sensitive (mainw->loop_continue, (!cfile->achans||mainw->mute||mainw->loop_cont||
						   audio_player==AUD_PLAYER_JACK||audio_player==AUD_PLAYER_PULSE)
			    &&mainw->current_file>0);

  if (cfile->frames==0&&mainw->multitrack==NULL) {
    if (mainw->preview_box!=NULL&&mainw->preview_box->parent!=NULL) {
      gtk_container_remove (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
    }
  }
  else {
    if (mainw->sep_win) {
      // create a separate window for the internal player if requested
      if (prefs->sepwin_type==0) {
	// needed
	block_expose();
	make_play_window();
	unblock_expose();
      }
      else {
	if (mainw->multitrack==NULL) {
	  if (mainw->preview_box!=NULL&&mainw->preview_box->parent!=NULL) {
	    gtk_container_remove (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
	  }
	}

	if (mainw->multitrack==NULL||mainw->fs) {
	  resize_play_window();
	}

	// needed
	if (mainw->multitrack==NULL) {
	  block_expose();
	  mainw->noswitch=TRUE;
	  while (g_main_context_iteration (NULL,FALSE));
	  mainw->noswitch=FALSE;
	  unblock_expose();
	}
	else {
	  // this doesn't get called if we don't call resize_play_window()
	  gtk_window_present (GTK_WINDOW (mainw->play_window));
	  gdk_window_raise(mainw->play_window->window);
	}
      }
    }

    if (mainw->play_window!=NULL) {
      hide_cursor (mainw->play_window->window);
      gtk_widget_set_app_paintable(mainw->play_window,TRUE);
      if (mainw->vpp!=NULL&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)&&mainw->fs) 
	gtk_window_set_title (GTK_WINDOW (mainw->play_window),_("LiVES: - Streaming"));
      else gtk_window_set_title (GTK_WINDOW (mainw->play_window),_("LiVES: - Play Window"));
      if (!mainw->pw_exp_is_blocked) g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=TRUE;
    }
  
    if (!mainw->foreign&&!mainw->sep_win) {
      hide_cursor(mainw->playarea->window);
    }
    
    // pwidth and pheight are playback width and height
    if (!mainw->sep_win&&!mainw->foreign) {
      do {
	mainw->pwidth=mainw->playframe->allocation.width-H_RESIZE_ADJUST;
	mainw->pheight=mainw->playframe->allocation.height-V_RESIZE_ADJUST;
	if (mainw->pwidth*mainw->pheight==0) {
	  gtk_widget_queue_draw (mainw->playframe);
	  mainw->noswitch=TRUE;
	  while (g_main_context_iteration (NULL, FALSE));
	  mainw->noswitch=FALSE;
	}
      } while (mainw->pwidth*mainw->pheight==0);
      // double size
      if (mainw->double_size) {
	frame_size_update();
      }
    }

    if (mainw->vpp!=NULL&&mainw->vpp->fheight>-1&&mainw->vpp->fwidth>-1) {
      // fixed o/p size for stream
      if (!(mainw->vpp->fwidth*mainw->vpp->fheight)) {
	mainw->vpp->fwidth=cfile->hsize;
	mainw->vpp->fheight=cfile->vsize;
      }
      mainw->pwidth=mainw->vpp->fwidth;
      mainw->pheight=mainw->vpp->fheight;
    }
    
    if (mainw->fs&&!mainw->sep_win&&cfile->frames>0) {
      fullscreen_internal();
    }

  }

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->pb_fps);
    
  mainw->last_blend_file=-1;

  // show the framebar
  if (mainw->multitrack==NULL&&(prefs->show_framecount&&(!mainw->fs||(prefs->gui_monitor!=prefs->play_monitor&&mainw->sep_win!=0)||(mainw->vpp!=NULL&&mainw->sep_win&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)))&&((!mainw->preview&&(cfile->frames>0||mainw->foreign))||cfile->opening))) {
    gtk_widget_show(mainw->framebar);
  }

  cfile->play_paused=FALSE;
  mainw->actual_frame=0;

  mainw->currticks=0;

  if (!mainw->foreign) {
    // start up our audio player (jack or pulse)

    if (audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
      if (mainw->jackd!=NULL&&mainw->aud_rec_fd==-1) {
	mainw->jackd->is_paused=FALSE;
	mainw->jackd->mute=mainw->mute;
	if (mainw->loop_cont&&!mainw->preview) {
	  if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS&&mainw->multitrack==NULL) mainw->jackd->loop=AUDIO_LOOP_PINGPONG;
	  else mainw->jackd->loop=AUDIO_LOOP_FORWARD;
	}
	else mainw->jackd->loop=AUDIO_LOOP_NONE;
	if (cfile->achans>0&&(!mainw->preview||(mainw->preview&&mainw->is_processing))&&(cfile->laudio_time>0.||cfile->opening||(mainw->multitrack!=NULL&&mainw->multitrack->is_rendering&&g_file_test((tmpfilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,cfile->handle)),G_FILE_TEST_EXISTS)))) {
	  gboolean timeout;
	  int alarm_handle;

	  if (tmpfilename!=NULL) g_free(tmpfilename);
	  mainw->jackd->num_input_channels=cfile->achans;
	  mainw->jackd->bytes_per_channel=cfile->asampsize/8;
	  mainw->jackd->sample_in_rate=cfile->arate;
	  mainw->jackd->usigned=!asigned;
	  mainw->jackd->seek_end=cfile->afilesize;
	  
	  if ((aendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(!aendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) mainw->jackd->reverse_endian=TRUE;
	  else mainw->jackd->reverse_endian=FALSE;

	  alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
	  while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
	    sched_yield(); // wait for seek
	  }
	  if (timeout) jack_try_reconnect();
	  lives_alarm_clear(alarm_handle);

	  if ((mainw->multitrack==NULL||mainw->multitrack->is_rendering)&&(mainw->event_list==NULL||mainw->record||(mainw->preview&&mainw->is_processing))) {
	    // tell jack server to open audio file and start playing it
	    jack_message.command=ASERVER_CMD_FILE_OPEN;
	    jack_message.data=g_strdup_printf("%d",mainw->current_file);

	    // TODO ** - use chain messages
	    jack_message.next=NULL;
	    mainw->jackd->msgq=&jack_message;

	    if (!jack_audio_seek_frame(mainw->jackd,mainw->play_start)) {
	      if (jack_try_reconnect()) jack_audio_seek_frame(mainw->jackd,mainw->play_start);
	    }

	    mainw->jackd->in_use=TRUE;
	    mainw->rec_aclip=mainw->current_file;
	    mainw->rec_avel=cfile->pb_fps/cfile->fps;
	    mainw->rec_aseek=(gdouble)cfile->aseek_pos/(gdouble)(cfile->arate*cfile->achans*(cfile->asampsize/8));
	  }
	}
      }
#endif
    }
    else if (audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed!=NULL&&mainw->aud_rec_fd==-1) {
	mainw->pulsed->is_paused=FALSE;
	mainw->pulsed->mute=mainw->mute;
	if (mainw->loop_cont&&!mainw->preview) {
	  if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS&&mainw->multitrack==NULL) mainw->pulsed->loop=AUDIO_LOOP_PINGPONG;
	  else mainw->pulsed->loop=AUDIO_LOOP_FORWARD;
	}
	else mainw->pulsed->loop=AUDIO_LOOP_NONE;
	if (cfile->achans>0&&(!mainw->preview||(mainw->preview&&mainw->is_processing))&&(cfile->laudio_time>0.||cfile->opening||(mainw->multitrack!=NULL&&mainw->multitrack->is_rendering&&g_file_test((tmpfilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,cfile->handle)),G_FILE_TEST_EXISTS)))) {

	  gboolean timeout;
	  int alarm_handle;

	  if (tmpfilename!=NULL) g_free(tmpfilename);
	  mainw->pulsed->in_achans=cfile->achans;
	  mainw->pulsed->in_asamps=cfile->asampsize;
	  mainw->pulsed->in_arate=cfile->arate;
	  mainw->pulsed->usigned=!asigned;
	  mainw->pulsed->seek_end=cfile->afilesize;
	  if (cfile->opening) mainw->pulsed->is_opening=TRUE;
	  else mainw->pulsed->is_opening=FALSE;
	  
	  if ((aendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(!aendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) mainw->pulsed->reverse_endian=TRUE;
	  else mainw->pulsed->reverse_endian=FALSE;

	  alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
	  while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
	    sched_yield(); // wait for seek
	  }
	  
	  if (timeout) pulse_try_reconnect();
	  
	  lives_alarm_clear(alarm_handle);

	  if ((mainw->multitrack==NULL||mainw->multitrack->is_rendering||cfile->opening)&&(mainw->event_list==NULL||mainw->record||(mainw->preview&&mainw->is_processing))) {
	    // tell pulse server to open audio file and start playing it
	    pulse_message.command=ASERVER_CMD_FILE_OPEN;
	    pulse_message.data=g_strdup_printf("%d",mainw->current_file);
	    pulse_message.next=NULL;
	    mainw->pulsed->msgq=&pulse_message;

	    if (!pulse_audio_seek_frame(mainw->pulsed,mainw->play_start)) {
	      if (pulse_try_reconnect()) pulse_audio_seek_frame(mainw->pulsed, mainw->play_start);
	    }

	    mainw->pulsed->in_use=TRUE;
	    mainw->rec_aclip=mainw->current_file;
	    mainw->rec_avel=cfile->pb_fps/cfile->fps;
	    mainw->rec_aseek=(gdouble)cfile->aseek_pos/(gdouble)(cfile->arate*cfile->achans*(cfile->asampsize/8));
	  }
	}
      }
#endif
    }
    else if (cfile->achans>0) {
      // sox or mplayer audio - run as background process

      if (mainw->loop_cont) {
	// tell audio to loop forever
	loop=-1;
      }

      stfile=g_build_filename(prefs->tmpdir,cfile->handle,".stoploop",NULL);
      unlink (stfile);
    
      if (cfile->achans>0||(!cfile->is_loaded&&!mainw->is_generating)) {
	if (loop) {
	  g_free (com4);
	  com4=g_strdup_printf ("/bin/touch \"%s\" 2>/dev/null;",stfile);
	}
	
	if (cfile->achans>0) {
	  com2=g_strdup_printf("smogrify stop_audio %s",cfile->handle);
	}
    
	stopcom=g_strconcat (com4,com2,NULL);
      }

      g_free(stfile);
      if (!mainw->preview&&!mainw->is_rendering) weed_reinit_all();

      stfile=g_build_filename(prefs->tmpdir,cfile->handle,".status.play",NULL);
      g_snprintf(cfile->info_file,PATH_MAX,"%s",stfile);
      g_free(stfile);
      if (cfile->clip_type==CLIP_TYPE_DISK) unlink(cfile->info_file);

      // PLAY

      if (cfile->clip_type==CLIP_TYPE_DISK&&cfile->opening) {
	  com=g_strdup_printf("smogrify play_opening_preview \"%s\" %.3f %d %d %d %d %d %u %d %d %d %d %d %d",
			      cfile->handle,cfile->fps,mainw->audio_start,audio_end,mainw->fs,0,wid,
			      mainw->pwidth,mainw->pheight,arate,cfile->achans,cfile->asampsize,asigned,aendian);
      }
      else {
	// this is only used now for sox or mplayer audio player
	com=g_strdup_printf("smogrify play \"%s\" %.3f %d %d %d %d %u %d %d %d %d %d %d %d",cfile->handle,
			    cfile->fps,mainw->audio_start,audio_end,mainw->fs,loop,wid,mainw->pwidth,mainw->pheight,
			    arate,cfile->achans,cfile->asampsize,asigned,aendian);
      }
      if (mainw->multitrack==NULL&&com!=NULL) lives_system(com,FALSE);
    }
  }

  g_free (com4);

  if (mainw->foreign||weed_playback_gen_start()) {

#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_PLAYBACK_STARTED,"");
    if (mainw->osc_auto) lives_osc_notify_success(NULL);
#endif
    
    // add a timer for the keyboard and other realtime events (osc, midi, joystick, etc)
    mainw->kb_timer=gtk_timeout_add (KEY_RPT_INTERVAL,&plugin_poll_keyboard,NULL);

#ifdef ENABLE_JACK
    if (mainw->event_list!=NULL&&!mainw->record&&audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&
	!(mainw->preview&&mainw->is_processing&&
	  !(mainw->multitrack!=NULL&&mainw->preview&&mainw->multitrack->is_rendering))) {
      // if playing an event list, we switch to audio memory buffer mode
      if (mainw->multitrack!=NULL) init_jack_audio_buffers(cfile->achans,cfile->arate,exact_preview);
      else init_jack_audio_buffers(DEFAULT_AUDIO_CHANS,DEFAULT_AUDIO_RATE,FALSE);
      has_audio_buffers=TRUE;
    }
#endif    
#ifdef HAVE_PULSE_AUDIO
    if (mainw->event_list!=NULL&&!mainw->record&&audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&
	!(mainw->preview&&mainw->is_processing&&
	  !(mainw->multitrack!=NULL&&mainw->preview&&mainw->multitrack->is_rendering))) {
      // if playing an event list, we switch to audio memory buffer mode
      if (mainw->multitrack!=NULL) init_pulse_audio_buffers(cfile->achans,cfile->arate,exact_preview);
      else init_pulse_audio_buffers(DEFAULT_AUDIO_CHANS,DEFAULT_AUDIO_RATE,FALSE);
      has_audio_buffers=TRUE;
    }
#endif    

    mainw->abufs_to_fill=0;

    //play until stopped or a stream finishes
    do {
      mainw->cancelled=CANCEL_NONE;

      if (mainw->event_list!=NULL&&!mainw->record) {
	if (pb_start_event==NULL) pb_start_event=get_first_frame_event(mainw->event_list);

	if (has_audio_buffers) {

#ifdef ENABLE_JACK
	  if (audio_player==AUD_PLAYER_JACK) {
	    int i;
	    mainw->write_abuf=0;
	    
	    // fill our audio buffers now
	    // this will also get our effects state

	    // reset because audio sync may have set it
	    if (mainw->multitrack!=NULL) mainw->jackd->abufs[0]->arate=cfile->arate;
	    fill_abuffer_from(mainw->jackd->abufs[0],mainw->event_list,pb_start_event,exact_preview);
	    for (i=1;i<prefs->num_rtaudiobufs;i++) {
	      // reset because audio sync may have set it
	      if (mainw->multitrack!=NULL) mainw->jackd->abufs[i]->arate=cfile->arate;
	      fill_abuffer_from(mainw->jackd->abufs[i],mainw->event_list,NULL,FALSE);
	    }
	    
	    pthread_mutex_lock(&mainw->abuf_mutex);
	    mainw->jackd->read_abuf=0;
	    mainw->abufs_to_fill=0;
	    pthread_mutex_unlock(&mainw->abuf_mutex);
	    mainw->jackd->in_use=TRUE;
	  }
#endif
#ifdef HAVE_PULSE_AUDIO
	  if (audio_player==AUD_PLAYER_PULSE) {
	    int i;
	    mainw->write_abuf=0;
	    
	    // fill our audio buffers now
	    // this will also get our effects state
	    fill_abuffer_from(mainw->pulsed->abufs[0],mainw->event_list,pb_start_event,exact_preview);
	    for (i=1;i<prefs->num_rtaudiobufs;i++) {
	      fill_abuffer_from(mainw->pulsed->abufs[i],mainw->event_list,NULL,FALSE);
	    }
	    
	    pthread_mutex_lock(&mainw->abuf_mutex);
	    mainw->pulsed->read_abuf=0;
	    mainw->abufs_to_fill=0;
	    pthread_mutex_unlock(&mainw->abuf_mutex);
	    mainw->pulsed->in_use=TRUE;
	    
	  }
#endif
	  // let transport roll
	  mainw->video_seek_ready=TRUE;
	}

      }

      if (mainw->multitrack==NULL||mainw->multitrack->pb_start_event==NULL) {
	do_progress_dialog(FALSE,FALSE,NULL);

	// reset audio buffers
#ifdef ENABLE_JACK
	if (audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
	  // must do this before deinit fx
	  pthread_mutex_lock(&mainw->abuf_mutex);
	  mainw->jackd->read_abuf=-1;
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	if (audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
	  // must do this before deinit fx
	  pthread_mutex_lock(&mainw->abuf_mutex);
	  mainw->pulsed->read_abuf=-1;
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	}
#endif

      }
      else {
	// play from middle of mt timeline
	cfile->next_event=mainw->multitrack->pb_start_event;

	if (!has_audio_buffers) {
	  // no audio buffering
	  // get just effects state
	  get_audio_and_effects_state_at(mainw->multitrack->event_list,mainw->multitrack->pb_start_event,FALSE,mainw->multitrack->exact_preview);
	}

	do_progress_dialog(FALSE,FALSE,NULL);

	// reset audio read buffers
#ifdef ENABLE_JACK
	if (audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
	  // must do this before deinit fx
	  pthread_mutex_lock(&mainw->abuf_mutex);
	  mainw->jackd->read_abuf=-1;
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	if (audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
	  // must do this before deinit fx
	  pthread_mutex_lock(&mainw->abuf_mutex);
	  mainw->pulsed->read_abuf=-1;
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	}
#endif

	// realtime effects off
	deinit_render_effects();

	cfile->next_event=NULL;

	// multitrack loop - go back to loop start position unless external transport moved us
	if (mainw->scratch==SCRATCH_NONE) {
	  mainw->multitrack->pb_start_event=mainw->multitrack->pb_loop_event;
	}
      }
      if (mainw->multitrack!=NULL) pb_start_event=mainw->multitrack->pb_start_event;
    } while (mainw->multitrack!=NULL&&(mainw->loop_cont||mainw->scratch!=SCRATCH_NONE)&&(mainw->cancelled==CANCEL_NONE||mainw->cancelled==CANCEL_EVENT_LIST_END));
    mainw->osc_block=TRUE;
    gtk_timeout_remove (mainw->kb_timer);
    mainw->rte_textparm=NULL;
    mainw->playing_file=-1;
  }
    
  // play completed

  mainw->video_seek_ready=FALSE;

#ifdef ENABLE_JACK
  if (audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {

    if (mainw->foreign&&mainw->jackd_read!=NULL) jack_rec_audio_end();

    if (!mainw->preview&&!mainw->foreign) jack_pb_stop();

    // tell jack client to close audio file
    if (mainw->jackd->playing_file>0) {
      gboolean timeout;
      int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
	sched_yield(); // wait for seek
      }
      if (timeout) jack_try_reconnect();
      lives_alarm_clear(alarm_handle);

      jack_message.command=ASERVER_CMD_FILE_CLOSE;
      jack_message.data=NULL;
      jack_message.next=NULL;
      mainw->jackd->msgq=&jack_message;

    }
    if (mainw->record&&(prefs->rec_opts&REC_AUDIO)) {
      weed_plant_t *event=get_last_frame_event(mainw->event_list);
      insert_audio_event_at(mainw->event_list,event,-1,1,0.,0.); // audio switch off
    }

  }
  else {
#endif
#ifdef HAVE_PULSE_AUDIO
  if (audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {

    if (mainw->foreign&&mainw->pulsed_read!=NULL) pulse_rec_audio_end();

    // tell pulse client to close audio file
    if (mainw->pulsed->fd>0) {
      gboolean timeout;
      int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
	sched_yield(); // wait for seek
      }
      if (timeout) pulse_try_reconnect();
      lives_alarm_clear(alarm_handle);

      pulse_message.command=ASERVER_CMD_FILE_CLOSE;
      pulse_message.data=NULL;
      pulse_message.next=NULL;
      mainw->pulsed->msgq=&pulse_message;
    }
    if (mainw->record&&(prefs->rec_opts&REC_AUDIO)) {
      weed_plant_t *event=get_last_frame_event(mainw->event_list);
      insert_audio_event_at(mainw->event_list,event,-1,1,0.,0.); // audio switch off
    }

  }
  else {
#endif
    if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE&&stopcom!=NULL) {
      // kill sound(if still playing)
      lives_system(stopcom,FALSE);
      mainw->aud_file_to_kill=-1;
      g_free (stopcom);
    }
#ifdef ENABLE_JACK
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  }
#endif

  if (com!=NULL) g_free(com);
  mainw->actual_frame=0;

  if (audio_player==AUD_PLAYER_JACK) audio_cache_end();

#ifdef ENABLE_OSC
  lives_osc_notify(LIVES_OSC_NOTIFY_PLAYBACK_STOPPED,"");
#endif

  mainw->video_seek_ready=FALSE;

  // PLAY FINISHED...
  // allow this to fail - not all sub-commands may be present
  if (prefs->stop_screensaver) 
    lives_system("xset s on 2>/dev/null; xset +dpms 2>/dev/null; gconftool-2 --set --type bool /apps/gnome-screensaver/idle_activation_enabled true 2>/dev/null;",TRUE);
  if (capable->has_xmms&&prefs->pause_xmms&&cfile->achans>0&&!mainw->mute) lives_system("xmms -u",TRUE);
  if (!mainw->foreign&&prefs->midisynch) lives_system ("midistop",TRUE);

  if (mainw->ext_playback) {
    vid_playback_plugin_exit();
  }
  // we could have started by playing a generator, which could've been closed
  if (mainw->files[current_file]==NULL) current_file=mainw->current_file;

  if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE) {
    // wait for audio_ended...
    if (cfile->achans>0&&com2!=NULL) {
      wait_for_stop(com2);
      mainw->aud_file_to_kill=-1;
    }
    if (com2!=NULL) g_free(com2);
  }

  if (mainw->current_file>-1) {
    stfile=g_build_filename(prefs->tmpdir,cfile->handle,".status",NULL);
    g_snprintf(cfile->info_file,PATH_MAX,"%s",stfile);
    g_free(stfile);
  }

  if (mainw->foreign) {
    // recording from external window capture

    mainw->pwidth=mainw->playframe->allocation.width-H_RESIZE_ADJUST;
    mainw->pheight=mainw->playframe->allocation.height-V_RESIZE_ADJUST;

    cfile->hsize=mainw->pwidth;
    cfile->vsize=mainw->pheight;

    g_object_ref(GTK_SOCKET(mainw->playarea)->plug_window);
    gdk_window_reparent(GTK_SOCKET(mainw->playarea)->plug_window,NULL,0,0);

    XMapWindow (GDK_WINDOW_XDISPLAY (GTK_SOCKET(mainw->playarea)->plug_window),
		  GDK_WINDOW_XID (GTK_SOCKET(mainw->playarea)->plug_window));

    // TODO - figure out how to add back to toplevel windows...

    while (g_main_context_iteration(NULL,FALSE));

    return;
  }

  if (mainw->toy_type==LIVES_TOY_AUTOLIVES) {
    on_toy_activate(NULL, GINT_TO_POINTER(LIVES_TOY_NONE));
  }

  gtk_widget_hide(mainw->playarea);

  // unblank the background
  if ((mainw->faded||mainw->fs)&&mainw->multitrack==NULL) {
    unfade_background();
  }

  // resize out of double size
  if ((mainw->double_size&&!mainw->fs)&&mainw->multitrack==NULL) {
    resize(1);
    if (palette->style&STYLE_1) {
      gtk_widget_show(mainw->sep_image);
    }
    gtk_widget_show(mainw->scrolledwindow);
  }

  // switch out of full screen mode
  if (mainw->fs&&mainw->multitrack==NULL) {
    gtk_widget_show(mainw->frame1);
    gtk_widget_show(mainw->frame2);
    gtk_widget_show(mainw->eventbox3);
    gtk_widget_show(mainw->eventbox4);
    gtk_widget_show(mainw->sep_image);
    gtk_frame_set_label(GTK_FRAME(mainw->playframe),_ ("Preview"));
    gtk_container_set_border_width (GTK_CONTAINER (mainw->playframe), 10);
    resize(1);
    gtk_widget_show(mainw->t_bckground);
    gtk_widget_show(mainw->t_double);
  }

  if (mainw->eventbox->allocation.height+mainw->menubar->allocation.height>mainw->scr_height-2) {
    // the screen grew too much...remaximise it
    gtk_window_unmaximize (GTK_WINDOW(mainw->LiVES));
    mainw->noswitch=TRUE;
    while (g_main_context_iteration(NULL,FALSE));
    mainw->noswitch=FALSE;
    gtk_window_maximize (GTK_WINDOW(mainw->LiVES));
  }
  
  if (mainw->multitrack==NULL) {
    gtk_widget_hide(mainw->playframe);
    gtk_widget_show(mainw->frame1);
    gtk_widget_show(mainw->frame2);
    gtk_widget_show(mainw->eventbox3);
    gtk_widget_show(mainw->eventbox4);
    disable_record();
    
    gtk_container_set_border_width (GTK_CONTAINER (mainw->playframe), 10);
  }

  if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE) mainw->mute=mute;

  if (!mainw->preview||!cfile->opening) {
    sensitize();
  }
  if (mainw->current_file>-1&&cfile->opening) {
    gtk_widget_set_sensitive (mainw->mute_audio, cfile->achans>0);
    gtk_widget_set_sensitive (mainw->loop_continue, TRUE);
    gtk_widget_set_sensitive (mainw->loop_video, cfile->achans>0&&cfile->frames>0);
  }

  if (mainw->cancelled!=CANCEL_USER_PAUSED) {
    gtk_widget_set_sensitive (mainw->stop, FALSE);
    gtk_widget_set_sensitive (mainw->m_stopbutton, FALSE);
  }

  if (mainw->multitrack==NULL) {
    // update screen for internal players
    gtk_widget_hide(mainw->framebar);
    gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),"");
    gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image274),NULL);
  }

  // kill the separate play window
  if (mainw->play_window!=NULL) {
    gtk_window_unfullscreen(GTK_WINDOW(mainw->play_window));
    if (prefs->sepwin_type==0) {
      kill_play_window();
    }
    else {
      // or resize it back to single size
      if (!GTK_WIDGET_VISIBLE (mainw->play_window)) {

	block_expose();
	mainw->noswitch=TRUE;
	g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
	mainw->pw_exp_is_blocked=TRUE;
	while (g_main_context_iteration (NULL,FALSE));
	mainw->noswitch=FALSE;
	unblock_expose();
      }
      if (mainw->current_file>-1&&cfile->is_loaded&&cfile->frames>0&&!mainw->is_rendering&&(cfile->clip_type!=CLIP_TYPE_GENERATOR)) {
	if (mainw->preview_box==NULL) {
	  // create the preview in the sepwin
	  make_preview_box();
	}
	if (mainw->current_file!=current_file) {
	  // now we have to guess how to center the play window
	  mainw->opwx=mainw->opwy=-1;
	  mainw->preview_frame=0;
	}
      }

      if (mainw->play_window!=NULL) {
	if (mainw->multitrack==NULL) {
	  mainw->playing_file=-2;
	  resize_play_window();
	  mainw->playing_file=-1;

	  gtk_widget_queue_draw (mainw->LiVES);
	  mainw->noswitch=TRUE;

	  g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
	  mainw->pw_exp_is_blocked=TRUE;

	  while (g_main_context_iteration (NULL,FALSE));

	  load_preview_image(FALSE);

	  mainw->noswitch=FALSE;
	  if (mainw->playing_file==-1&&mainw->play_window!=NULL)
	    gtk_window_set_title(GTK_WINDOW(mainw->play_window),gtk_window_get_title(GTK_WINDOW(mainw->LiVES)));
	}
	if (mainw->play_window!=NULL) {
	  gtk_window_present (GTK_WINDOW (mainw->play_window));
	  gdk_window_raise(mainw->play_window->window);
	  unhide_cursor (mainw->play_window->window);
	}
      }
    }

    // free the last frame image
    if (mainw->frame_layer!=NULL) {
      weed_layer_free(mainw->frame_layer);
      mainw->frame_layer=NULL;
    }
  }
  
  if (mainw->current_file>-1) cfile->play_paused=FALSE;

  if (mainw->blend_file!=-1&&mainw->blend_file!=mainw->current_file&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR) {
    gint xcurrent_file=mainw->current_file;
    weed_bg_generator_end (mainw->files[mainw->blend_file]->ext_src);
    mainw->current_file=xcurrent_file;
  }

  mainw->filter_map=NULL;

  mainw->record_paused=mainw->record_starting=FALSE;
  
  // disable the freeze key
  gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), freeze_closure);
  
  if (mainw->multitrack==NULL) gtk_widget_show(mainw->scrolledwindow);

  if (mainw->current_file>-1) {
    if (mainw->toy_type==LIVES_TOY_MAD_FRAMES&&!cfile->opening) {
      load_start_image (cfile->start);
      load_end_image (cfile->end);
    }
  }
  if (prefs->show_player_stats) {
    if (mainw->fps_measure>0.) {
      msg=g_strdup_printf (_ ("Average FPS was %.4f\n"),mainw->fps_measure);
      d_print (msg);
      g_free (msg);
    }
  }
  if (mainw->size_warn) {
    do_error_dialog (_ ("\n\nSome frames in this clip are wrongly sized.\nYou should click on Tools--->Resize All\nand resize all frames to the current size.\n"));
    mainw->size_warn=FALSE;
  }
  mainw->is_processing=mainw->preview;

  // TODO - ????
  if (mainw->current_file>-1&&cfile->clip_type==CLIP_TYPE_DISK&&cfile->frames==0&&mainw->record_perf) {
    g_signal_handler_block(mainw->record_perf,mainw->record_perf_func);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->record_perf),FALSE);
    g_signal_handler_unblock(mainw->record_perf,mainw->record_perf_func);
  }

  // TODO - can this be done earlier ?
  if (mainw->cancelled==CANCEL_APP_QUIT) on_quit_activate (NULL,NULL);

  // end record performance

#ifdef ENABLE_JACK
  if (audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
    gboolean timeout;
    int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
    while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
      sched_yield(); // wait for seek
    }
    if (timeout) jack_try_reconnect();
    
    lives_alarm_clear(alarm_handle);

    mainw->jackd->in_use=FALSE;

    if (has_audio_buffers) {
      free_jack_audio_buffers();
      audio_free_fnames();
    }


  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
    gboolean timeout;
    int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
    while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
      sched_yield(); // wait for seek
    }
    
    if (timeout) pulse_try_reconnect();
    
    lives_alarm_clear(alarm_handle);
    
    mainw->pulsed->in_use=FALSE;

    if (has_audio_buffers) {
      free_pulse_audio_buffers();
      audio_free_fnames();
    }

  }
#endif

  if (mainw->bad_aud_file!=NULL) {
    // we got an error recording audio
    do_write_failed_error_s(mainw->bad_aud_file);
    g_free(mainw->bad_aud_file);
    mainw->bad_aud_file=NULL;
  }

  // need to do this here, in case we want to preview a generator which will close to -1
  if (mainw->record) deal_with_render_choice(TRUE);

  if (!mainw->preview&&cfile->clip_type==CLIP_TYPE_GENERATOR) {
    mainw->osc_block=TRUE;
    weed_generator_end (cfile->ext_src);
    mainw->osc_block=FALSE;
    if (mainw->multitrack==NULL) {
      if (mainw->files[current_file]!=NULL) switch_to_file (mainw->current_file,current_file);
      else if (mainw->pre_src_file!=-1) switch_to_file (mainw->current_file,mainw->pre_src_file);
    }
  }

  if (mainw->multitrack==NULL) mainw->osc_block=FALSE;

  reset_clip_menu();

}
  

gboolean
get_temp_handle(gint index, gboolean create) {
  // we can call this to get a temp handle for returning info from the backend
  // this function is also called from get_new_handle to create a permanent handle
  // for an opened file

  // if a temp handle is required, pass in index as mainw->first_free_file, and
  // call 'smogrify close cfile->handle' on it after use, then restore mainw->current_file

  // returns FALSE if we couldn't write to tempdir

  // WARNING: this function changes mainw->current_file, unless it returns FALSE (could not create cfile)


  gchar *com;
  gint ret;
  gboolean is_unique;
  gint current_file=mainw->current_file;

  if (index==-1) {
    too_many_files();
    return FALSE;
  }

  do {
    mainw->current_file=current_file;

    is_unique=TRUE;

    // TODO - check for EOF

    com=g_strdup_printf("smogrify new %d",getpid());
    ret=system(com);
    g_free(com);
    
    if (ret) {
      tempdir_warning();
      return FALSE;
    }
    
    // TODO - check for EOF

    //get handle from info file, we will also malloc a new "file" struct here
    if (!get_handle_from_info_file(index)) {
      // timed out
      if (mainw->files[index]!=NULL) g_free(mainw->files[index]);
      mainw->files[index]=NULL;
      return FALSE;
    }

    mainw->current_file=index;

    if (strlen(mainw->set_name)>0) {
      gchar *setclipdir=g_build_filename(prefs->tmpdir,mainw->set_name,"clips",cfile->handle,NULL);
      if (g_file_test(setclipdir,G_FILE_TEST_IS_DIR)) is_unique=FALSE;
      g_free(setclipdir);
    }

  } while (!is_unique);

  if (create) create_cfile();
  return TRUE;
}



void create_cfile(void) {
  gchar *stfile;

  // any cfile (clip) initialisation goes in here
  cfile->menuentry=NULL;
  cfile->start=cfile->end=0;
  cfile->old_frames=cfile->frames=0;
  g_snprintf(cfile->type,40,"%s",_ ("Unknown"));
  cfile->f_size=0l;
  cfile->achans=0;
  cfile->arate=0;
  cfile->arps=0;
  cfile->afilesize=0l;
  cfile->asampsize=0;
  cfile->undoable=FALSE;
  cfile->redoable=FALSE;
  cfile->changed=FALSE;
  cfile->hsize=cfile->vsize=cfile->ohsize=cfile->ovsize=0;
  cfile->fps=cfile->pb_fps=prefs->default_fps;
  cfile->events[0]=NULL;
  cfile->insert_start=cfile->insert_end=0;
  cfile->is_untitled=TRUE;
  cfile->was_renamed=FALSE;
  cfile->undo_action=UNDO_NONE;
  cfile->opening_audio=cfile->opening=cfile->opening_only_audio=FALSE;
  cfile->pointer_time=0.;
  cfile->restoring=cfile->opening_loc=cfile->nopreview=cfile->is_loaded=FALSE;
  cfile->video_time=cfile->total_time=cfile->laudio_time=cfile->raudio_time=0.;
  cfile->freeze_fps=0.;
  cfile->frameno=cfile->last_frameno=0;
  cfile->proc_ptr=NULL;
  cfile->progress_start=cfile->progress_end=0;
  cfile->play_paused=cfile->nokeep=FALSE;
  cfile->undo_start=cfile->undo_end=0;
  cfile->rowstride=0; // unknown
  cfile->ext_src=NULL;
  cfile->clip_type=CLIP_TYPE_DISK;
  cfile->ratio_fps=FALSE;
  cfile->aseek_pos=0;
  cfile->unique_id=(gint64)random();
  cfile->layout_map=NULL;
  cfile->frame_index=cfile->frame_index_back=NULL;
  cfile->fx_frame_pump=0;
  cfile->stored_layout_frame=0;
  cfile->stored_layout_audio=0.;
  cfile->stored_layout_fps=0.;
  cfile->stored_layout_idx=-1;
  cfile->interlace=LIVES_INTERLACE_NONE;
  cfile->subt=NULL;

  if (!strcmp(prefs->image_ext,"jpg")) cfile->img_type=IMG_TYPE_JPEG;
  else cfile->img_type=IMG_TYPE_PNG;

  cfile->bpp=(cfile->img_type==IMG_TYPE_JPEG)?24:32;
  cfile->deinterlace=FALSE;

  cfile->play_paused=FALSE;
  cfile->header_version=LIVES_CLIP_HEADER_VERSION;

  cfile->event_list=cfile->event_list_back=NULL;
  cfile->next_event=NULL;

  memset(cfile->name,0,1);
  memset(cfile->mime_type,0,1);
  memset(cfile->file_name,0,1);
  memset(cfile->save_file_name,0,1);

  memset (cfile->comment,0,1);
  memset (cfile->author,0,1);
  memset (cfile->title,0,1);
  memset (cfile->keywords,0,1);

  cfile->signed_endian=AFORM_UNKNOWN;
  g_snprintf(cfile->undo_text,32,"%s",_ ("_Undo"));
  g_snprintf(cfile->redo_text,32,"%s",_ ("_Redo"));

  stfile=g_build_filename(prefs->tmpdir,cfile->handle,".status",NULL);
  g_snprintf(cfile->info_file,PATH_MAX,"%s",stfile);
  g_free(stfile);
  
  // remember to set cfile->is_loaded=TRUE !!!!!!!!!!
}


gboolean
get_new_handle (gint index, const gchar *name) {
  // here is where we first initialize for the clipboard
  // and for paste_as_new, and restore
  // pass in name as NULL or "" and it will be set with an untitled number

  // this function *does not* change mainw->current_file, or add to the menu
  // or update mainw->clips_available
  gchar *xname;

  gint current_file=mainw->current_file;
  if (!get_temp_handle(index,TRUE)) return FALSE;

  // note : don't need to update first_free_file for the clipboard 
  if (index!=0) {
    get_next_free_file();
  }

  if (name==NULL||!strlen(name)) {
    cfile->is_untitled=TRUE;
    xname=g_strdup_printf(_ ("Untitled%d"),mainw->untitled_number++);
  }
  else xname=g_strdup(name);

  g_snprintf(cfile->file_name,PATH_MAX,"%s",xname);
  g_snprintf(cfile->name,256,"%s",xname);
  mainw->current_file=current_file;

  g_free(xname);
  return TRUE;
}



gboolean add_file_info(const gchar *check_handle, gboolean aud_only) {
  // file information has been retrieved, set struct cfile with details
  // contained in mainw->msg. We do this twice, once before opening the file, once again after.
  // The first time, frames and afilesize may not be correct.
  gint pieces;
  gchar *mesg,*mesg1;
  gchar **array;
  gchar *test_fps_string1;
  gchar *test_fps_string2;

  if (check_handle!=NULL) {
    if (mainw->msg==NULL||get_token_count(mainw->msg,'|')==1) return FALSE;

    array=g_strsplit(mainw->msg,"|",-1);
    
    // sanity check handle against status file
    // (this should never happen...)
    
    if (strcmp(check_handle,array[1])) {
      LIVES_ERROR("Handle!=statusfile !");
      mesg=g_strdup_printf(_("\nError getting file info for clip %s.\nBad things may happen with this clip.\n"),
			   check_handle);
      do_error_dialog(mesg);
      g_free(mesg);
      return FALSE;
    }
    
    if (!aud_only) {
      cfile->frames=atoi(array[2]);
      g_snprintf(cfile->type,40,"%s",array[3]);
      cfile->hsize=atoi(array[4]);
      cfile->vsize=atoi(array[5]);
      cfile->bpp=atoi(array[6]);
      cfile->pb_fps=cfile->fps=g_strtod(array[7],NULL);
      
      cfile->f_size=strtol(array[8],NULL,10);
    }

    cfile->arps=cfile->arate=atoi(array[9]);
    cfile->achans=atoi(array[10]);
    cfile->asampsize=atoi(array[11]);
    cfile->signed_endian=get_signed_endian(atoi (array[12]), atoi (array[13]));
    cfile->afilesize=strtol(array[14],NULL,10);
    
    pieces=get_token_count (mainw->msg,'|');
    
    if (pieces>14&&array[15]!=NULL) {
      g_snprintf (cfile->title,256,"%s",g_strchomp (g_strchug ((array[15]))));
    }
    if (pieces>15&&array[16]!=NULL) {
      g_snprintf (cfile->author,256,"%s",g_strchomp (g_strchug ((array[16]))));
    }
    if (pieces>16&&array[17]!=NULL) {
      g_snprintf (cfile->comment,256,"%s",g_strchomp (g_strchug ((array[17]))));
    }
    
    g_strfreev(array);
  }

  if (aud_only) return TRUE;

  test_fps_string1=g_strdup_printf ("%.3f00000",cfile->fps);
  test_fps_string2=g_strdup_printf ("%.8f",cfile->fps);

  if (strcmp (test_fps_string1,test_fps_string2)) {
    cfile->ratio_fps=TRUE;
  }
  else {
    cfile->ratio_fps=FALSE;
  }
  g_free (test_fps_string1);
  g_free (test_fps_string2);

  if (!mainw->save_with_sound) {
    cfile->arps=cfile->arate=cfile->achans=cfile->asampsize=0;
    cfile->afilesize=0l;
  }

  if (cfile->frames<=0) {
    if (cfile->afilesize==0l&&cfile->is_loaded) {
      // we got no video or audio...
      return FALSE;
    }
    cfile->start=cfile->end=cfile->undo_start=cfile->undo_end=0;
  }
  else {
  // start with all selected
    cfile->start=1;
    cfile->end=cfile->frames;
    cfile->undo_start=cfile->start;
    cfile->undo_end=cfile->end;
  }

  cfile->orig_file_name=TRUE;
  cfile->is_untitled=FALSE;

  // some files give us silly frame rates, even single frames...
  // fps of 1000. is used for some streams (i.e. play each frame as it is received)
  if (cfile->fps==0.||cfile->fps==1000.||(cfile->frames<2&&cfile->is_loaded)) {
      gdouble xduration=0.;

      if (cfile->ext_src!=NULL&&cfile->fps>0) {
	  xduration=cfile->frames/cfile->fps;
      }

    if (!(cfile->afilesize*cfile->asampsize*cfile->arate*cfile->achans)||cfile->frames<2) {
      if (cfile->frames!=1) {
      mesg=g_strdup_printf(_ ("\nPlayback speed not found or invalid ! Using default fps of %.3f fps. \nDefault can be set in Tools | Preferences | Misc.\n"),prefs->default_fps);
      d_print(mesg);
      g_free(mesg);
      }
      cfile->pb_fps=cfile->fps=prefs->default_fps;
    }
    else {
      cfile->laudio_time=cfile->raudio_time=cfile->afilesize/cfile->asampsize*8./cfile->arate/cfile->achans;
      cfile->pb_fps=cfile->fps=1.*(gint)(cfile->frames/cfile->laudio_time);
      if (cfile->fps>FPS_MAX||cfile->fps<1.) {
	cfile->pb_fps=cfile->fps=prefs->default_fps;
      }
      mesg=g_strdup_printf(_ ("Playback speed was adjusted to %.3f frames per second to fit audio.\n"),cfile->fps);
      d_print(mesg);
      g_free(mesg);
    }

    if (xduration>0.) {
	lives_clip_data_t *cdata=((lives_decoder_t *)cfile->ext_src)->cdata;
// we should not (!) do this, but broken codecs force us to !
	cdata->nframes=cfile->frames=xduration*cfile->fps;
	cdata->fps=cfile->fps;
    }

  }
  if (cfile->opening) return TRUE;

  if (cfile->bpp==256) {
    mesg1=g_strdup_printf(_ ("Frames=%d type=%s size=%dx%d *bpp=Greyscale* fps=%.3f\nAudio:"),cfile->frames,cfile->type,cfile->hsize,cfile->vsize,cfile->fps);
  }
  else {
    if (cfile->bpp!=32) cfile->bpp=24; // assume RGB24  *** TODO - check
    mesg1=g_strdup_printf(_ ("Frames=%d type=%s size=%dx%d bpp=%d fps=%.3f\nAudio:"),cfile->frames,cfile->type,cfile->hsize,cfile->vsize,cfile->bpp,cfile->fps);
  }

  if (cfile->achans==0) {
    mesg=g_strdup_printf (_ ("%s none\n"),mesg1);
  }
  else {
    mesg=g_strdup_printf(_ ("%s %d Hz %d channel(s) %d bps\n"),mesg1,cfile->arate,cfile->achans,cfile->asampsize);
  }
  d_print(mesg);
  g_free(mesg1);
  g_free(mesg);

  // get the comments
  if (strlen (cfile->comment)) {
    mesg=g_strdup_printf(_ (" - Comment: %s\n"),cfile->comment);
    d_print(mesg);
    g_free(mesg);
  }
  return TRUE;
}


gboolean save_file_comments (int fileno) {
  // save the comments etc for smogrify
  int retval;
  int comment_fd;
  gchar *comment_file=g_strdup_printf ("%s/%s/.comment",prefs->tmpdir,cfile->handle);
  file *sfile=mainw->files[fileno];

  unlink (comment_file);

  do {
    retval=0;
    comment_fd=creat(comment_file,S_IRUSR|S_IWUSR);
    if (comment_fd<0) {
      mainw->write_failed=TRUE;
      retval=do_write_failed_error_s_with_retry(comment_file,g_strerror(errno),NULL);
    }
    else {
      mainw->write_failed=FALSE;
      lives_write(comment_fd,sfile->title,strlen (sfile->title),TRUE);
      lives_write(comment_fd,"||%",3,TRUE);
      lives_write(comment_fd,sfile->author,strlen (sfile->author),TRUE);
      lives_write(comment_fd,"||%",3,TRUE);
      lives_write(comment_fd,sfile->comment,strlen (sfile->comment),TRUE);
      
      close (comment_fd);
      
      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(comment_file,NULL,NULL);
      }
    }
  } while (retval==LIVES_RETRY);

  g_free (comment_file);

  if (mainw->write_failed) return FALSE;

  return TRUE;
}





void
wait_for_stop (const gchar *stop_command) {
  FILE *infofile;

  // only used for audio player mplayer or audio player sox

# define SECOND_STOP_TIME 0.1
# define STOP_GIVE_UP_TIME 1.0

  gdouble time_waited=0.;
  gboolean sent_second_stop=FALSE;
  
  // send another stop if necessary
  mainw->noswitch=TRUE;
  while (!(infofile=fopen(cfile->info_file,"r"))) {
    while (g_main_context_iteration(NULL,FALSE));
    g_usleep(prefs->sleep_time);
    time_waited+=1000000./prefs->sleep_time;
    if (time_waited>SECOND_STOP_TIME&&!sent_second_stop) {
      lives_system(stop_command,TRUE);
      sent_second_stop=TRUE;
    }
    
    if (time_waited>STOP_GIVE_UP_TIME) {
      // give up waiting, but send a last try...
      lives_system(stop_command,TRUE);
      break;
    }
  }
  mainw->noswitch=FALSE;
  if (infofile) fclose (infofile);
}


gboolean save_frame_inner(gint clip, gint frame, const gchar *file_name, gint width, gint height, gboolean allow_over) {
  // save 1 frame as an image (uses imagemagick to convert)
  // width==-1, height==-1 to use "natural" values
  gint result;
  gchar *com,*tmp;
  gchar full_file_name[PATH_MAX];
  file *sfile=mainw->files[clip];

  if (strrchr(file_name,'.')==NULL) {
    g_snprintf(full_file_name,PATH_MAX,"%s.%s",file_name,sfile->img_type==IMG_TYPE_JPEG?"jpg":"png");
  }
  else {
    g_snprintf(full_file_name,PATH_MAX,"%s",file_name);
  }

  if (!check_file(full_file_name,!allow_over)) return FALSE;

  tmp=g_filename_from_utf8 (full_file_name,-1,NULL,NULL,NULL);

  if (mainw->multitrack==NULL) {
    com=g_strdup_printf(_ ("Saving frame %d as %s..."),frame,full_file_name);
    d_print(com);
    g_free(com);
    
    if (sfile->clip_type==CLIP_TYPE_FILE) {
      gboolean resb=virtual_to_images(clip,frame,frame,FALSE);
      if (!resb) {
	d_print_file_error_failed();
	return FALSE;
      }
    }

    com=g_strdup_printf("smogrify save_frame \"%s\" %d \"%s\" %d %d",sfile->handle,frame,tmp,width,height);
    result=system(com);
    g_free(com);
    g_free(tmp);
    
    // TODO - check for EOF

    if (result==256) {
      d_print_file_error_failed();
      do_file_perm_error(full_file_name);
      return FALSE;
    }
    if (result==0) {
      d_print_done();
      return TRUE;
    }
  }
  else {
    // multitrack mode
    GError *gerr=NULL;
    GdkPixbuf *pixbuf;
    int retval;

    mt_show_current_frame(mainw->multitrack,TRUE);
    convert_layer_palette(mainw->frame_layer,WEED_PALETTE_RGB24,0);
    resize_layer(mainw->frame_layer,sfile->hsize,sfile->vsize,GDK_INTERP_HYPER);
    pixbuf=layer_to_pixbuf(mainw->frame_layer);
    weed_plant_free(mainw->frame_layer);
    mainw->frame_layer=NULL;

    do {
      retval=0;
      if (sfile->img_type==IMG_TYPE_JPEG) lives_pixbuf_save(pixbuf, tmp, IMG_TYPE_JPEG, 100, &gerr);
      else if (sfile->img_type==IMG_TYPE_PNG) lives_pixbuf_save(pixbuf, tmp, IMG_TYPE_PNG, 100, &gerr);

      if (gerr!=NULL) {
	retval=do_write_failed_error_s_with_retry(full_file_name,gerr->message,NULL);
	g_error_free(gerr);
	gerr=NULL;
      }

    } while (retval==LIVES_RETRY);

    free(tmp);
    gdk_pixbuf_unref(pixbuf);
  }

  // some other error condition
  return FALSE;
}


void backup_file(int clip, int start, int end, const gchar *file_name) {
  gchar *com,*tmp;
  gchar title[256];
  gchar **array;
  gchar full_file_name[PATH_MAX];
  gint withsound=1;
  gboolean with_perf=FALSE,retval;
  gint current_file=mainw->current_file;

  file *sfile=mainw->files[clip];

  if (strrchr(file_name,'.')==NULL) {
    g_snprintf(full_file_name,PATH_MAX,"%s.lv1",file_name);
   }
  else {
    g_snprintf(full_file_name,PATH_MAX,"%s",file_name);
  }

  // check if file exists
  if (!check_file(full_file_name,TRUE)) return;

  // create header files
  retval=write_headers(sfile); // for pre LiVES 0.9.6
  retval=save_clip_values(clip); // new style (0.9.6+)

  if (!retval) return;

  //...and backup
  get_menu_text(sfile->menuentry,title);
  com=g_strdup_printf(_ ("Backing up %s to %s"),title,full_file_name);
  d_print(com);
  g_free(com);

  if (!mainw->save_with_sound) {
    d_print(_ (" without sound"));
    withsound=0;
  }

  d_print("...");
  cfile->progress_start=1;
  cfile->progress_end=sfile->frames;

  if (sfile->clip_type==CLIP_TYPE_FILE) {
    gboolean resb;
    mainw->cancelled=CANCEL_NONE;
    cfile->progress_start=1;
    cfile->progress_end=count_virtual_frames(sfile->frame_index,1,sfile->frames);
    do_threaded_dialog(_("Pulling frames from clip"),TRUE);
    resb=virtual_to_images(clip,1,sfile->frames,TRUE);
    end_threaded_dialog();

    if (mainw->cancelled!=CANCEL_NONE||!resb) {
      sensitize();
      mainw->cancelled=CANCEL_USER;
      cfile->nopreview=FALSE;
      if (!resb) d_print_file_error_failed();
      else d_print_cancelled();
      return;
    }
  }

  com=g_strdup_printf("smogrify backup \"%s\" %d %d %d \"%s\"",sfile->handle,withsound,start,end,(tmp=g_filename_from_utf8 (full_file_name,-1,NULL,NULL,NULL)));

  // TODO
  mainw->current_file=clip;

  unlink (cfile->info_file);
  cfile->nopreview=TRUE;
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(tmp);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    mainw->current_file=current_file;
    return;
  }

  // TODO - check for EOF

  if (!(do_progress_dialog(TRUE,TRUE,_ ("Backing up")))||mainw->error) {
    if (mainw->error) {
      d_print_failed();
    }

    // cancelled - clear up files
    cfile->nopreview=FALSE;
    g_free (com);

    // using restore details in the 'wrong' way here...it will also clear files
    com=g_strdup_printf("smogrify restore_details \"%s\"",cfile->handle);
    unlink (cfile->info_file);
    lives_system(com,FALSE);
    // auto-d
    g_free(com);

    //save_clip_values(mainw->current_file);
    mainw->current_file=current_file;
    return;
  }

  cfile->nopreview=FALSE;
  g_free(com);

  mainw->current_file=current_file;

  if (mainw->error) {
    do_error_dialog(mainw->msg);
    d_print_failed();
    return;
  }

  if (with_perf) {
    d_print(_ ("performance data was backed up..."));
  }

  array=g_strsplit(mainw->msg,"|",3);
  sfile->f_size=strtol(array[1],NULL,10);
  g_strfreev(array);

  g_snprintf(sfile->file_name,PATH_MAX,"%s",full_file_name);
  if (!sfile->was_renamed) {
    g_snprintf(sfile->name,256,"%s",full_file_name);
    set_main_title(cfile->name,0);
    set_menu_text(sfile->menuentry,full_file_name,FALSE);
  }
  add_to_recent (full_file_name,0.,0,NULL);

  sfile->changed=FALSE;
  // set is_untitled to stop users from saving with a .lv1 extension
  sfile->is_untitled=TRUE;
  d_print_done();
}


gboolean write_headers (file *file) {
  // this function is included only for backwards compatibility with ancient builds of LiVES
  //

  int retval;
  int header_fd;
  gchar *hdrfile;

  // save the file details
  hdrfile=g_build_filename(prefs->tmpdir,file->handle,"header",NULL);

  do {
    retval=0;
    header_fd=creat(hdrfile,S_IRUSR|S_IWUSR);
    if (header_fd<0) {
      retval=do_write_failed_error_s_with_retry(hdrfile,g_strerror(errno),NULL);
    }
    else {
      mainw->write_failed=FALSE;
      
      lives_write(header_fd,&cfile->bpp,sizint,TRUE);
      lives_write(header_fd,&cfile->fps,sizdbl,TRUE);
      lives_write(header_fd,&cfile->hsize,sizint,TRUE);
      lives_write(header_fd,&cfile->vsize,sizint,TRUE);
      lives_write(header_fd,&cfile->arps,sizint,TRUE);
      lives_write(header_fd,&cfile->signed_endian,sizint,TRUE);
      lives_write(header_fd,&cfile->arate,sizint,TRUE);
      lives_write(header_fd,&cfile->unique_id,8,TRUE);
      lives_write(header_fd,&cfile->achans,sizint,TRUE);
      lives_write(header_fd,&cfile->asampsize,sizint,TRUE);
      
      lives_write(header_fd,LiVES_VERSION,strlen(LiVES_VERSION),TRUE);
      close(header_fd);
      
      if (mainw->write_failed) retval=do_write_failed_error_s_with_retry(hdrfile,NULL,NULL);
      
    }
  } while (retval==LIVES_RETRY);


  g_free(hdrfile);

  if (!mainw->write_failed) {
    // more file details (since version 0.7.5)
    hdrfile=g_build_filename(prefs->tmpdir,file->handle,"header2",NULL);

    do {
      retval=0;
      header_fd=creat(hdrfile,S_IRUSR|S_IWUSR);
    
      if (header_fd<0) {
	retval=do_write_failed_error_s_with_retry(hdrfile,g_strerror(errno),NULL);
      }
      else {
	mainw->write_failed=FALSE;
	lives_write(header_fd,&file->frames,sizint,TRUE);
	lives_write(header_fd,&file->title,256,TRUE);
	lives_write(header_fd,&file->author,256,TRUE);
	lives_write(header_fd,&file->comment,256,TRUE);
	close(header_fd);
      }
      
      if (mainw->write_failed) retval=do_write_failed_error_s_with_retry(hdrfile,NULL,NULL);
    } while (retval==LIVES_RETRY);

    g_free(hdrfile);
  }

  if (mainw->write_failed) {
    mainw->write_failed=FALSE;
    return FALSE;
  }
  return TRUE;

}


gboolean read_headers(const gchar *file_name) {
  // file_name is only used to get the file size on the disk
  FILE *infofile;
  gchar **array;
  gchar buff[1024];
  gchar version[32];
  gchar *com,*tmp;
  gchar *old_hdrfile=g_build_filename(prefs->tmpdir,cfile->handle,"header",NULL);
  gchar *lives_header=g_build_filename(prefs->tmpdir,cfile->handle,"header.lives",NULL);

  gint header_size;
  gint version_hash;
  gint pieces;
  int header_fd;
  int alarm_handle;
  int retval2;

  lives_clip_details_t detail;

  gboolean timeout;
  gboolean retval,retvala;

  size_t sizhead=8*sizint+sizdbl+8;

  time_t old_time=0,new_time=0;
  struct stat mystat;

  // TODO - remove this some time before 2038...
  if (!stat(old_hdrfile,&mystat)) old_time=mystat.st_mtime;
  if (!stat(lives_header,&mystat)) new_time=mystat.st_mtime;
  ///////////////

  if (old_time<new_time) {
    do {
      retval2=0;

      detail=CLIP_DETAILS_FRAMES;
      if (get_clip_value(mainw->current_file,detail,&cfile->frames,0)) {
	gint asigned,aendian;
	gchar *tmp;
	int alarm_handle;

	// use new style header (LiVES 0.9.6+)
	g_free(old_hdrfile);

	// clean up and get file sizes
	com=g_strdup_printf("smogrify restore_details \"%s\" \"%s\" %d",cfile->handle,
			    (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)),!strcmp (file_name,"."));

	mainw->com_failed=FALSE;
	lives_system(com,FALSE);
	g_free(com);
	g_free(tmp);
     
	if (mainw->com_failed) {
	  mainw->com_failed=FALSE;
	  return FALSE;
	}


	// TODO - check for EOF

	do {
	  retval2=0;
	  timeout=FALSE;
	  memset(buff,0,1);

#define LIVES_RESTORE_TIMEOUT  (30 * U_SEC) // 30 sec

	  alarm_handle=lives_alarm_set(LIVES_RESTORE_TIMEOUT);

	  while (!((infofile=fopen(cfile->info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
	    g_usleep(prefs->sleep_time);
	  }
	  
	  lives_alarm_clear(alarm_handle);
	  
	  if (!timeout) {
	    mainw->read_failed=FALSE;
	    lives_fgets(buff,1024,infofile);
	    fclose(infofile);
	  }

	  if (timeout || mainw->read_failed) {
	    retval2=do_read_failed_error_s_with_retry(cfile->info_file,NULL,NULL);
	  }
	} while (retval2==LIVES_RETRY);

	if (retval2==LIVES_CANCEL) {
	  return FALSE;
	}

	pieces=get_token_count (buff,'|');

	if (pieces>3) {
	  array=g_strsplit(buff,"|",pieces);
	
	  cfile->f_size=strtol(array[1],NULL,10);
	  cfile->afilesize=strtol(array[2],NULL,10);
	  if (!strcmp(array[3],"jpg")) cfile->img_type=IMG_TYPE_JPEG;
	  else cfile->img_type=IMG_TYPE_PNG;
	  g_strfreev(array);
	}

	threaded_dialog_spin();

	do {
	  retval2=0;
	  if (!cache_file_contents(lives_header)) {
	    retval2=do_read_failed_error_s_with_retry(lives_header,NULL,NULL);
	  }
	} while (retval2==LIVES_RETRY);

	g_free(lives_header);

	threaded_dialog_spin();

	detail=CLIP_DETAILS_HEADER_VERSION;
	retval=get_clip_value(mainw->current_file,detail,&cfile->header_version,16);
	if (retval) {
	  detail=CLIP_DETAILS_BPP;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->bpp,0);
	}
	if (retval) {
	  detail=CLIP_DETAILS_FPS;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->fps,0);
	}
	if (retval) {
	  detail=CLIP_DETAILS_PB_FPS;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->pb_fps,0);
	  if (!retval) {
	    retval=TRUE;
	    cfile->pb_fps=cfile->fps;
	  }
	}
	if (retval) {
	  retval=get_clip_value(mainw->current_file,CLIP_DETAILS_PB_FRAMENO,&cfile->frameno,0);
	  if (!retval) {
	    retval=TRUE;
	    cfile->frameno=1;
	  }
	}
	if (retval) {
	  detail=CLIP_DETAILS_WIDTH;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->hsize,0);
	}
	if (retval) {
	  detail=CLIP_DETAILS_HEIGHT;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->vsize,0);
	}
	if (retval) {
	  detail=CLIP_DETAILS_FILENAME;
	  get_clip_value(mainw->current_file,detail,cfile->file_name,256);
	}

	if (retval) {
	  detail=CLIP_DETAILS_ACHANS;
	  retvala=get_clip_value(mainw->current_file,detail,&cfile->achans,0);
	  if (!retvala) cfile->achans=0;
	}

	if (cfile->achans==0) retvala=FALSE;
	else retvala=TRUE;

	if (retval&&retvala) {
	  detail=CLIP_DETAILS_ARATE;
	  retvala=get_clip_value(mainw->current_file,detail,&cfile->arps,0);
	}

	if (!retvala) cfile->arps=cfile->achans=cfile->arate=cfile->asampsize=0;
	if (cfile->arps==0) retvala=FALSE; 

	if (retvala&&retval) {
	  detail=CLIP_DETAILS_PB_ARATE;
	  retvala=get_clip_value(mainw->current_file,detail,&cfile->arate,0);
	  if (!retvala) {
	    retvala=TRUE;
	    cfile->arate=cfile->arps;
	  }
	}
	if (retvala&&retval) {
	  detail=CLIP_DETAILS_ASIGNED;
	  retval=get_clip_value(mainw->current_file,detail,&asigned,0);
	}
	if (retvala&&retval) {
	  detail=CLIP_DETAILS_AENDIAN;
	  retval=get_clip_value(mainw->current_file,detail,&aendian,0);
	}

	cfile->signed_endian=asigned+aendian;

	if (retvala&&retval) {
	  detail=CLIP_DETAILS_ASAMPS;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->asampsize,0);
	}
	
	get_clip_value(mainw->current_file,CLIP_DETAILS_TITLE,cfile->title,256);
	get_clip_value(mainw->current_file,CLIP_DETAILS_AUTHOR,cfile->author,256);
	get_clip_value(mainw->current_file,CLIP_DETAILS_COMMENT,cfile->comment,256);
	get_clip_value(mainw->current_file,CLIP_DETAILS_KEYWORDS,cfile->comment,1024);
	get_clip_value(mainw->current_file,CLIP_DETAILS_INTERLACE,&cfile->interlace,0);
	if (cfile->interlace!=LIVES_INTERLACE_NONE) cfile->deinterlace=TRUE; // user must have forced this

	if (!retval) {
	  if (mainw->cached_list!=NULL) {
	    retval2=do_header_missing_detail_error(mainw->current_file,detail);
	  }
	  else {
	    retval2=do_header_read_error_with_retry(mainw->current_file);
	  }
	}
	else return TRUE;
      }
      else {
	if (mainw->cached_list!=NULL) {
	  retval2=do_header_missing_detail_error(mainw->current_file,CLIP_DETAILS_FRAMES);
	}
	else {
	  retval2=do_header_read_error_with_retry(mainw->current_file);
	}
      }
    } while (retval2==LIVES_RETRY);
    return FALSE; // retval2==LIVES_CANCEL
  }

  // old style headers (pre 0.9.6)
  g_free(lives_header);

  do {
    retval=0;
    memset (version,0,32);
    memset (buff,0,1024);
    
    header_fd=open(old_hdrfile,O_RDONLY);

    if (header_fd<0) {
      retval=do_read_failed_error_s_with_retry(old_hdrfile,g_strerror(errno),NULL);
    }
    else {
      mainw->read_failed=FALSE;
      header_size=get_file_size(header_fd);
  
      if (header_size<sizhead) {
	g_free(old_hdrfile);
	close (header_fd);
	return FALSE;
      }
      else {
	lives_read(header_fd,&cfile->bpp,sizint,FALSE);
	lives_read(header_fd,&cfile->fps,sizdbl,FALSE);
	lives_read(header_fd,&cfile->hsize,sizint,FALSE);
	lives_read(header_fd,&cfile->vsize,sizint,FALSE);
	lives_read(header_fd,&cfile->arps,sizint,FALSE);
	lives_read(header_fd,&cfile->signed_endian,sizint,FALSE);
	lives_read(header_fd,&cfile->arate,sizint,FALSE);
	lives_read(header_fd,&cfile->unique_id,8,FALSE);
	lives_read(header_fd,&cfile->achans,sizint,FALSE);
	lives_read(header_fd,&cfile->asampsize,sizint,FALSE);
      
	if (header_size>sizhead) {
	  if (header_size-sizhead>31) {
	    lives_read(header_fd,&version,31,FALSE);
	    version[31]='\0';
	  }
	  else {
	    lives_read(header_fd,&version,header_size-sizhead,FALSE);
	    version[header_size-sizhead]='\0';
	  }
	}
      }
      close(header_fd);
    }

    if (mainw->read_failed) {
      retval=do_read_failed_error_s_with_retry(old_hdrfile,NULL,NULL);
      if (retval==LIVES_CANCEL) {
	g_free(old_hdrfile);
	return FALSE;
      }
    }
  } while (retval==LIVES_RETRY);

  g_free(old_hdrfile);

  // handle version changes
  version_hash=verhash(version);
  if (version_hash<7001) {
    cfile->arps=cfile->arate;
    cfile->signed_endian=mainw->endian;
  }
  
  com=g_strdup_printf("smogrify restore_details \"%s\" \"%s\" %d",cfile->handle,
		      (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)),!strcmp (file_name,"."));
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);
  g_free(tmp);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    return FALSE;
  }

  // TODO - check for EOF

#define LIVES_RESTORE_TIMEOUT  (30 * U_SEC) // 120 sec timeout

  do {
    retval2=0;
    timeout=FALSE;

    alarm_handle=lives_alarm_set(LIVES_RESTORE_TIMEOUT);

    while (!((infofile=fopen(cfile->info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
      g_usleep(prefs->sleep_time);
    }

    lives_alarm_clear(alarm_handle);
  
    if (!timeout) {
      mainw->read_failed=FALSE;
      lives_fgets(buff,1024,infofile);
      fclose(infofile);
    }

    if (timeout || mainw->read_failed) {
      retval2=do_read_failed_error_s_with_retry(cfile->info_file,NULL,NULL);
    }
  
  } while (retval2==LIVES_RETRY);

  if (retval2==LIVES_CANCEL) {
    mainw->read_failed=FALSE;
    return FALSE;
  }

  pieces=get_token_count (buff,'|');
  array=g_strsplit(buff,"|",pieces);
  cfile->f_size=strtol(array[1],NULL,10);
  cfile->afilesize=strtol(array[2],NULL,10);

  if (!strcmp(array[3],"jpg")) cfile->img_type=IMG_TYPE_JPEG;
  else cfile->img_type=IMG_TYPE_PNG;

  cfile->frames=atoi(array[4]);

  cfile->bpp=(cfile->img_type==IMG_TYPE_JPEG)?24:32;
  
  if (pieces>4&&array[5]!=NULL) {
    g_snprintf (cfile->title,256,"%s",g_strchomp (g_strchug ((array[4]))));
  }
  if (pieces>5&&array[6]!=NULL) {
    g_snprintf (cfile->author,256,"%s",g_strchomp (g_strchug ((array[5]))));
  }
  if (pieces>6&&array[7]!=NULL) {
    g_snprintf (cfile->comment,256,"%s",g_strchomp (g_strchug ((array[6]))));
  }
  
  g_strfreev(array);
  return TRUE;
}


void open_set_file (const gchar *set_name, gint clipnum) {
  gchar name[256];
  gboolean needs_update=FALSE;

  if (mainw->current_file<1) return;

  memset (name,0,256);

  if (mainw->cached_list!=NULL) {
    gboolean retval;
    // LiVES 0.9.6+

    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->pb_fps,0);
    if (!retval) {
      cfile->pb_fps=cfile->fps;
    }
    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_PB_FRAMENO,&cfile->frameno,0);
    if (!retval) {
      cfile->frameno=1;
    }
    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_CLIPNAME,name,256);
    if (!retval) {
      g_snprintf(name,256,_ ("Untitled%d"),mainw->untitled_number++);
      needs_update=TRUE;
    }
    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_UNIQUE_ID,&cfile->unique_id,0);
    if (!retval) {
      cfile->unique_id=random();
      needs_update=TRUE;
    }
    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_INTERLACE,&cfile->interlace,0);
    if (!retval) {
      cfile->interlace=LIVES_INTERLACE_NONE;
      needs_update=TRUE;
    }
    if (cfile->interlace!=LIVES_INTERLACE_NONE) cfile->deinterlace=TRUE;

  }
  else {
    // pre 0.9.6 <- ancient code
    size_t nlen;
    int set_fd;
    int pb_fps;
    int retval;
    gchar *setfile=g_strdup_printf("%s/%s/set.%s",prefs->tmpdir,cfile->handle,set_name);

    do {
      retval=0;
      if ((set_fd=open(setfile,O_RDONLY))>-1) {
	// get perf_start
	if ((nlen=lives_read(set_fd,&pb_fps,sizint,TRUE))) {
	  cfile->pb_fps=pb_fps/1000.;
	  lives_read(set_fd,&cfile->frameno,sizint,TRUE);
	  lives_read(set_fd,name,256,TRUE);
	}
	close (set_fd);
      }
      else retval=do_read_failed_error_s_with_retry(setfile,g_strerror(errno),NULL);
    } while (retval==LIVES_RETRY);

    g_free (setfile);
    needs_update=TRUE;
  }

  if (strlen(name)==0) {
    g_snprintf (name,256,"set_clip %.3d",clipnum);
  }
  if (strlen(mainw->set_name)&&strcmp (name+strlen (name)-1,")")) {
    g_snprintf (cfile->name,256,"%s (%s)",name,set_name);
  }
  else {
    g_snprintf (cfile->name,256,"%s",name);
  }

  if (needs_update) save_clip_values(mainw->current_file);

}



void restore_file(const gchar *file_name) {
  gchar *com=g_strdup("dummy");
  gchar *mesg,*mesg1,*tmp;
  gboolean is_OK=TRUE;
  gchar *fname=g_strdup(file_name);

  gint old_file=mainw->current_file,current_file;
  gint new_file=mainw->first_free_file;
  gboolean not_cancelled;

  gchar *subfname;

  // create a new file
  if (!get_new_handle(new_file,fname)) {
    return;
  }
  
  mesg=g_strdup_printf(_ ("Restoring %s..."),file_name);
  d_print(mesg);
  g_free(mesg);
  
  mainw->current_file=new_file;
  
  cfile->hsize=mainw->def_width;
  cfile->vsize=mainw->def_height;
  
  switch_to_file((mainw->current_file=old_file),new_file);
  set_main_title(cfile->file_name,0);
  
  com=g_strdup_printf("smogrify restore \"%s\" \"%s\"",cfile->handle,
		      (tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(tmp);
  g_free(com);
  unlink (cfile->info_file);
  
  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    close_current_file(old_file);
    return;
  }

  // TODO - check for EOF

  cfile->restoring=TRUE;
  not_cancelled=do_progress_dialog(TRUE,TRUE,_ ("Restoring"));
  cfile->restoring=FALSE;

  if (mainw->error||!not_cancelled) {
    if (mainw->error) {
      do_blocking_error_dialog (mainw->msg);
    }
    close_current_file(old_file);
    return;
  }
  
  // call function to return rest of file details
  //fsize, afilesize and frames
  is_OK=read_headers(file_name);

  if (mainw->cached_list!=NULL) {
    g_list_free_strings(mainw->cached_list);
    g_list_free(mainw->cached_list);
    mainw->cached_list=NULL;
  }

  if (!is_OK) {
    mesg=g_strdup_printf(_ ("\n\nThe file %s is corrupt.\nLiVES was unable to restore it.\n"),file_name);
    do_blocking_error_dialog(mesg);
    g_free(mesg);
    
    d_print_failed();
    close_current_file(old_file);
    return;
  }
  if (!check_frame_count(mainw->current_file)) get_frame_count(mainw->current_file);
  
  // add entry to window menu
  // TODO - do this earlier and allow switching during restore
  add_to_winmenu();

  if (prefs->show_recent) {
    add_to_recent(file_name,0.,0,NULL);
  }

  if (cfile->frames>0) {
    cfile->start=1;
  }
  else {
    cfile->start=0;
  }
  cfile->end=cfile->frames;
  cfile->arps=cfile->arate;
  cfile->pb_fps=cfile->fps;
  cfile->opening=FALSE;
  cfile->proc_ptr=NULL;

  cfile->changed=FALSE;

  if (prefs->autoload_subs) {
    subfname=g_strdup_printf("%s/%s/subs.srt",prefs->tmpdir,cfile->handle);
    if (g_file_test(subfname,G_FILE_TEST_EXISTS)) {
      subtitles_init(cfile,subfname,SUBTITLE_TYPE_SRT);
    }
    else {
      g_free(subfname);
      subfname=g_strdup_printf("%s/%s/subs.sub",prefs->tmpdir,cfile->handle);
      if (g_file_test(subfname,G_FILE_TEST_EXISTS)) {
	subtitles_init(cfile,subfname,SUBTITLE_TYPE_SUB);
      }
    }
    g_free(subfname);
  }

  g_snprintf(cfile->type,40,"Frames");
  mesg1=g_strdup_printf(_ ("Frames=%d type=%s size=%dx%d bpp=%d fps=%.3f\nAudio:"),cfile->frames,cfile->type,cfile->hsize,cfile->vsize,cfile->bpp,cfile->fps);

  if (cfile->afilesize==0l) {
    cfile->achans=0;
    mesg=g_strdup_printf (_ ("%s none\n"),mesg1);
  }
  else {
    mesg=g_strdup_printf(_ ("%s %d Hz %d channel(s) %d bps\n"),mesg1,cfile->arate,cfile->achans,cfile->asampsize);
  }
  d_print(mesg);
  g_free(mesg);
  g_free(mesg1);

  cfile->is_loaded=TRUE;
  current_file=mainw->current_file;

  // set new bpp
  cfile->bpp=(cfile->img_type==IMG_TYPE_JPEG)?24:32;

  if (!save_clip_values(current_file)) {
    close_current_file(old_file);
    return;
  }

  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);

  switch_to_file((mainw->current_file=old_file),current_file);

#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif

}


gint save_event_frames(void) {
  // when doing a resample, we save a list of frames for the back end to do
  // a reorder

  // here we also update the frame_index for clips of type CLIP_TYPE_FILE

  int header_fd,i=0;
  int retval;
  gchar *hdrfile=g_strdup_printf("%s/%s/event.frames",prefs->tmpdir,cfile->handle);
  gint perf_start,perf_end;
  
  gint nevents;

  if (cfile->event_list==NULL) {
    unlink (hdrfile);
    return -1;
  }
  
  perf_start=(gint)(cfile->fps*event_list_get_start_secs (cfile->event_list))+1;
  perf_end=perf_start+(nevents=count_events (cfile->event_list,FALSE,0,0))-1;
  
  event_list_to_block (cfile->event_list,nevents);

  if (cfile->frame_index!=NULL) {
    gint xframes=cfile->frames;

    if (cfile->frame_index_back!=NULL) g_free(cfile->frame_index_back);
    cfile->frame_index_back=cfile->frame_index;
    cfile->frame_index=NULL;

    create_frame_index(mainw->current_file,FALSE,0,nevents);

    for (i=0;i<nevents;i++) {
      cfile->frame_index[i]=cfile->frame_index_back[(cfile->events[0]+i)->value-1];
    }

    cfile->frames=nevents;
    if (!check_if_non_virtual(mainw->current_file,1,cfile->frames)) save_frame_index(mainw->current_file);
    cfile->frames=xframes;
  }

  do {
    retval=0;
    header_fd=creat(hdrfile,S_IRUSR|S_IWUSR);
    if (header_fd<0) {
      retval=do_write_failed_error_s_with_retry(hdrfile,g_strerror(errno),NULL);
    }
    else {
      mainw->write_failed=FALSE;
      lives_write(header_fd,&perf_start,sizint,FALSE);
      
      if (!(cfile->events[0]==NULL)) {
	for (i=0;i<=perf_end-perf_start;i++) {
	  if (mainw->write_failed) break;
	  lives_write(header_fd,&((cfile->events[0]+i)->value),sizint,TRUE);
	}
	g_free (cfile->events[0]);
	cfile->events[0]=NULL;
      }

      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(hdrfile,NULL,NULL);
      }

      close(header_fd);

    }
  } while (retval==LIVES_RETRY);


  if (mainw->write_failed) {
    mainw->write_failed=FALSE;
    i=-1;
  }

  g_free(hdrfile);
  return i;
}



/////////////////////////////////////////////////
/// scrap file
///  the scrap file is used during recording to dump any streamed (non-disk) clips to
/// during render/preview we load frames from the scrap file, but only as necessary

static gdouble scrap_mb;  // MiB written to file
static gulong free_mb; // MiB free to write

gboolean open_scrap_file (void) {
  // create a virtual clip
  gint current_file=mainw->current_file;
  gint new_file=mainw->first_free_file;
  
  gchar *dir;
  gchar *scrap_handle;

  if (!get_temp_handle (new_file,TRUE)) return FALSE;
  get_next_free_file();

  mainw->scrap_file=mainw->current_file=new_file;

  g_snprintf(cfile->type,40,"scrap");
  cfile->frames=0;

  scrap_handle=g_strdup_printf("scrap|%s",cfile->handle);

  add_to_recovery_file(scrap_handle);

  mainw->cliplist = g_list_append (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));

  g_free(scrap_handle);

  dir=g_strdup_printf("%s/%s",prefs->tmpdir,cfile->handle);
  free_mb=get_fs_free(dir)/1000000;
  g_free(dir);

  mainw->current_file=current_file;

  scrap_mb=0.;

  return TRUE;
}






gboolean load_from_scrap_file(weed_plant_t *layer, int frame) {
  // load raw frame data from scrap file

  // this will also set cfile width and height - for letterboxing etc.

  // return FALSE if the frame does not exist/we are unable to read it


  int width,height,palette,nplanes;
  int clamping,subspace,sampling;
  int *rowstrides;

  int i,fd;

  gchar *oname=g_strdup_printf ("%s/%s/%08d.scrap",prefs->tmpdir,mainw->files[mainw->scrap_file]->handle,frame);

  gchar buf[sizint+1];

  size_t bytes,tsize;

  void **pdata;

  fd=open(oname,O_RDONLY);

  if (fd==-1) return FALSE;

  memset(&buf[sizint],0,1);

  bytes=read(fd,buf,sizint);
  if (bytes<sizint) return FALSE;

  palette=atoi(buf);
  weed_set_int_value(layer,"current_palette",palette);



  if (weed_palette_is_yuv_palette(palette)) {

    bytes=read(fd,buf,sizint);
    if (bytes<sizint) return FALSE;
    
    clamping=atoi(buf);
    weed_set_int_value(layer,"YUV_clamping",clamping);

    bytes=read(fd,buf,sizint);
    if (bytes<sizint) return FALSE;
    
    subspace=atoi(buf);
    weed_set_int_value(layer,"YUV_subspace",subspace);

    bytes=read(fd,buf,sizint);
    if (bytes<sizint) return FALSE;
    
    sampling=atoi(buf);
    weed_set_int_value(layer,"YUV_sampling",sampling);
  }


  bytes=read(fd,buf,sizint);
  if (bytes<sizint) return FALSE;

  width=atoi(buf);
  weed_set_int_value(layer,"width",width);


  bytes=read(fd,buf,sizint);
  if (bytes<sizint) return FALSE;

  height=atoi(buf);
  weed_set_int_value(layer,"height",height);


  nplanes=weed_palette_get_numplanes(palette);

  rowstrides=g_malloc(nplanes*sizint);

  for (i=0;i<nplanes;i++) {
    bytes=read(fd,buf,sizint);
    if (bytes<sizint) {
      g_free(rowstrides);
      return FALSE;
    }
    rowstrides[i]=atoi(buf);
  }

  weed_set_int_array(layer,"rowstrides",nplanes,rowstrides);

  pdata=g_malloc(nplanes*sizeof(void *));

  for (i=0;i<nplanes;i++) {
    pdata[i]=NULL;
  }

  weed_set_voidptr_array(layer,"pixel_data",nplanes,pdata);

  for (i=0;i<nplanes;i++) {
    tsize=rowstrides[i]*height*weed_palette_get_plane_ratio_vertical(palette,i);
    pdata[i]=g_malloc(tsize);
    bytes=read(fd,pdata[i],tsize);
    if (bytes<tsize) {
      g_free(rowstrides);
      g_free(pdata);
      return FALSE;
    }
  }

  g_free(rowstrides);

  weed_set_voidptr_array(layer,"pixel_data",nplanes,pdata);
  g_free(pdata);


  close (fd);

  return TRUE;
}



gint save_to_scrap_file (weed_plant_t *layer) {
  // returns frame number

  // dump the raw frame data to a file

  // format is:
  // (int)palette,[(int)YUV_clamping,(int)YUV_subspace,(int)YUV_sampling,](int)rowstrides[],(int)height,(char *)pixel_data[]

  // sampling, clamping and subspace are only written for YUV palettes

  // we also check if there is enough free space left; if not, recording is paused


  int fd;
  int flags=O_WRONLY|O_CREAT|O_TRUNC;
  
  int width,height,palette,nplanes,error;
  int *rowstrides;

  int clamping,subspace,sampling;

  int i;

  void **pdata;

  gchar *oname=g_strdup_printf ("%s/%s/%08d.scrap",prefs->tmpdir,mainw->files[mainw->scrap_file]->handle,mainw->files[mainw->scrap_file]->frames+1);

  gchar *buf,*framecount;

  struct stat filestat;

#ifdef O_NOATIME
  flags|=O_NOATIME;
#endif

  fd=open(oname,flags,S_IRUSR|S_IWUSR);

  if (fd==-1) {
    g_free(oname);
    return mainw->files[mainw->scrap_file]->frames;
  }

  mainw->write_failed=FALSE;

  // write current_palette, rowstrides and height
  palette=weed_get_int_value(layer,"current_palette",&error);
  buf=g_strdup_printf("%d",palette);
  lives_write(fd,buf,sizint,TRUE);
  g_free(buf);

  if (mainw->write_failed) {
    g_free(oname);
    return mainw->files[mainw->scrap_file]->frames;
  }

  if (weed_palette_is_yuv_palette(palette)) {
    if (weed_plant_has_leaf(layer,"YUV_clamping")) {
      clamping=weed_get_int_value(layer,"YUV_clamping",&error);
    }
    else clamping=WEED_YUV_CLAMPING_CLAMPED;
    buf=g_strdup_printf("%d",clamping);
    lives_write(fd,buf,sizint,TRUE);
    g_free(buf);

    if (weed_plant_has_leaf(layer,"YUV_subspace")) {
      subspace=weed_get_int_value(layer,"YUV_subspace",&error);
    }
    else subspace=WEED_YUV_SUBSPACE_YUV;
    buf=g_strdup_printf("%d",subspace);
    lives_write(fd,buf,sizint,TRUE);
    g_free(buf);

    if (weed_plant_has_leaf(layer,"YUV_sampling")) {
      sampling=weed_get_int_value(layer,"YUV_sampling",&error);
    }
    else sampling=WEED_YUV_SAMPLING_DEFAULT;
    buf=g_strdup_printf("%d",sampling);
    lives_write(fd,buf,sizint,TRUE);
    g_free(buf);
  }

  width=weed_get_int_value(layer,"width",&error);
  buf=g_strdup_printf("%d",width);
  lives_write(fd,buf,sizint,TRUE);
  g_free(buf);

  height=weed_get_int_value(layer,"height",&error);
  buf=g_strdup_printf("%d",height);
  lives_write(fd,buf,sizint,TRUE);
  g_free(buf);
  

  nplanes=weed_palette_get_numplanes(palette);
  
  rowstrides=weed_get_int_array(layer,"rowstrides",&error);
  
  for (i=0;i<nplanes;i++) {
    buf=g_strdup_printf("%d",rowstrides[i]);
    lives_write(fd,buf,sizint,TRUE);
    g_free(buf);
  }
  
   
  // now write pixel_data planes
  
  pdata=weed_get_voidptr_array(layer,"pixel_data",&error);
  
  for (i=0;i<nplanes;i++) {
    lives_write(fd,pdata[i],rowstrides[i]*height*weed_palette_get_plane_ratio_vertical(palette,i),TRUE);
  }
  
  fstat(fd,&filestat);

  scrap_mb+=(gdouble)(filestat.st_size)/1000000.;

  // check free space every 1000 frames
  if (mainw->files[mainw->scrap_file]->frames%1000==0) {
    gchar *dir=g_strdup_printf("%s/%s",prefs->tmpdir,mainw->files[mainw->scrap_file]->handle);
    free_mb=get_fs_free(dir)/1000000;
    g_free(dir);
  }

  if ((!mainw->fs||prefs->play_monitor!=prefs->gui_monitor)&&prefs->show_framecount) {
    if (scrap_mb<(gdouble)free_mb*.75) {
      framecount=g_strdup_printf(_("rec %.2f MB"),scrap_mb); // translators: rec(ord) %.2f M(ega)B(ytes)
    }
    else {
      // warn if scrap_file > 3/4 of free space
      framecount=g_strdup_printf(_("!rec %.2f MB"),scrap_mb); // translators: rec(ord) %.2f M(ega)B(ytes)
    }
    gtk_entry_set_text(GTK_ENTRY(mainw->framecounter),framecount);
    g_free(framecount);
  }

  fsync(fd); // try to sync file access, to make saving smoother
  close(fd);

  weed_free(rowstrides);
  weed_free(pdata);

  g_free(oname);

  // check if we have enough free space left on the volume
  if ((glong)(((gdouble)free_mb-scrap_mb)/1000.)<prefs->rec_stop_gb) {
    // check free space again
    gchar *dir=g_strdup_printf("%s/%s",prefs->tmpdir,mainw->files[mainw->scrap_file]->handle);
    free_mb=get_fs_free(dir)/1000000;
    g_free(dir);

    if ((glong)(((gdouble)free_mb-scrap_mb)/1000.)<prefs->rec_stop_gb) {
      if (mainw->record&&!mainw->record_paused) {
	gchar *msg=g_strdup_printf(_("\nRECORDING WAS PAUSED BECAUSE FREE DISK SPACE IS BELOW %ld GB !\nRecord stop level can be set in Preferences.\n"),prefs->rec_stop_gb);
	d_print(msg);
	g_free(msg);
	on_record_perf_activate(NULL,NULL);
      }
    }
  }

  return ++mainw->files[mainw->scrap_file]->frames;

}



void close_scrap_file (void) {
  gint current_file=mainw->current_file;

  if (mainw->scrap_file==-1) return;

  mainw->current_file=mainw->scrap_file;
  close_current_file(current_file);

  mainw->cliplist=g_list_remove (mainw->cliplist, GINT_TO_POINTER (mainw->scrap_file));

  if (prefs->crash_recovery) rewrite_recovery_file();

  mainw->scrap_file=-1;
}



void recover_layout_map(numclips) {
  // load global layout map for a set and assign entries to clips [mainw->files[i]->layout_map]
  GList *mlist,*lmap_node,*lmap_node_next,*lmap_entry_list,*lmap_entry_list_next;
  layout_map *lmap_entry;
  gchar **array;
  
  if (numclips>MAX_FILES) numclips=MAX_FILES;

  if ((mlist=load_layout_map())!=NULL) {
    int i;
    // assign layout map to clips
    for (i=1;i<=numclips;i++) {
      if (mainw->files[i]==NULL) continue;
      lmap_node=mlist;
      while (lmap_node!=NULL) {
	lmap_node_next=lmap_node->next;
	lmap_entry=lmap_node->data;
	g_print("cf %d %s %s %ld %ld\n",i,mainw->files[i]->handle,lmap_entry->handle,(mainw->files[i]->unique_id),(lmap_entry->unique_id));
	if (!strcmp(mainw->files[i]->handle,lmap_entry->handle)&&(mainw->files[i]->unique_id==lmap_entry->unique_id)) {
	  // check handle and unique id match
	  // got a match, assign list to layout_map and delete this node
	  lmap_entry_list=lmap_entry->list;
	  while (lmap_entry_list!=NULL) {
	    lmap_entry_list_next=lmap_entry_list->next;
	    array=g_strsplit(lmap_entry_list->data,"|",-1);
	    g_print("test %s\n",array[0]);
	    if (!g_file_test(array[0],G_FILE_TEST_EXISTS)) {
	      g_print("removing\n");
	      // layout file has been deleted, remove this entry
	      if (lmap_entry_list->prev!=NULL) lmap_entry_list->prev->next=lmap_entry_list_next;
	      else lmap_entry->list=lmap_node_next;
	      if (lmap_entry_list_next!=NULL) lmap_entry_list_next->prev=lmap_entry_list->prev;
	      g_free(lmap_entry_list->data);
	      //g_free(lmap_entry_list);    // i don't know why, but this causes a segfault
	    }
	    g_strfreev(array);
	    lmap_entry_list=lmap_entry_list_next;
	  }
	  mainw->files[i]->layout_map=lmap_entry->list;
	  g_free(lmap_entry->handle);
	  g_free(lmap_entry->name);
	  g_free(lmap_entry);
	  if (lmap_node->prev!=NULL) lmap_node->prev->next=lmap_node_next;
	  else mlist=lmap_node_next;
	  if (lmap_node_next!=NULL) lmap_node_next->prev=lmap_node->prev;
	  //g_free(lmap_node);   // i don't know why, but this causes a segfault
	}
	lmap_node=lmap_node_next;
      }
    }
  
    lmap_node=mlist;
    while (lmap_node!=NULL) {
      lmap_entry=lmap_node->data;
      if (lmap_entry->name!=NULL) g_free(lmap_entry->name);
      if (lmap_entry->handle!=NULL) g_free(lmap_entry->handle);
      if (lmap_entry->list!=NULL) {
	g_list_free_strings(lmap_entry->list);
	g_list_free(lmap_entry->list);
      }
      lmap_node=lmap_node->next;
    }
    if (mlist!=NULL) g_list_free(mlist);

  }
}






static gboolean recover_files(gchar *recovery_file, gboolean auto_recover) {
  FILE *rfile;
  gchar buff[256],*buffptr;
  gchar *clipdir;

  int retval;
  gint new_file,clipnum=0;
  gboolean last_was_normal_file=FALSE;
  gboolean is_scrap;
  gboolean did_set_check=FALSE;

  const lives_clip_data_t *cdata=NULL;

  splash_end();

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (!auto_recover) {
    if (!do_warning_dialog
	(_("\nFiles from a previous run of LiVES were found.\nDo you want to attempt to recover them ?\n"))) {
      unlink(recovery_file);

      if (mainw->multitrack!=NULL) {
	mt_sensitise(mainw->multitrack);
      }

      return FALSE;
    }
  }

  do {
    retval=0;
    rfile=fopen(recovery_file,"r");
    if (!rfile) {
      retval=do_read_failed_error_s_with_retry(recovery_file,g_strerror(errno),NULL);
      if (retval==LIVES_CANCEL) return FALSE;
    }
  } while (retval==LIVES_RETRY);

  do_threaded_dialog(_("Recovering files"),FALSE);

  d_print(_("Recovering files..."));
  threaded_dialog_spin();
  
  mainw->suppress_dprint=TRUE;
  
  while (1) {
    threaded_dialog_spin();
    is_scrap=FALSE;
    
    if (mainw->cached_list!=NULL) {
      g_list_free_strings(mainw->cached_list);
      g_list_free(mainw->cached_list);
      mainw->cached_list=NULL;
    }

    mainw->read_failed=FALSE;
    
    if (lives_fgets(buff,256,rfile)==NULL) {
      gint current_file=mainw->current_file;
      if (last_was_normal_file&&mainw->multitrack==NULL) {
	switch_to_file((mainw->current_file=0),current_file);
      }
      reset_clip_menu();
      while (g_main_context_iteration(NULL,FALSE));
      threaded_dialog_spin();

      if (mainw->read_failed) {
	do_read_failed_error_s(recovery_file);
      }
      break;
    }

    memset(buff+strlen(buff)-strlen("\n"),0,1);

    if (!strcmp(buff+strlen(buff)-1,"*")) {
      // set to be opened
      memset(buff+strlen(buff)-2,0,1);
      last_was_normal_file=FALSE;
      if (!is_legal_set_name(buff,TRUE)) continue;
      g_snprintf(mainw->set_name,128,"%s",buff);

      if (!on_load_set_ok(NULL,GINT_TO_POINTER(TRUE))) {
	fclose(rfile);
	end_threaded_dialog();

	if (strlen(mainw->set_name)>0) recover_layout_map(mainw->current_file);

	if (mainw->multitrack!=NULL) {
	  mainw->current_file=mainw->multitrack->render_file;
	  polymorph(mainw->multitrack,POLY_NONE);
	  polymorph(mainw->multitrack,POLY_CLIPS);
	  mt_sensitise(mainw->multitrack);
	}

	mainw->suppress_dprint=FALSE;
	d_print_failed();
	return TRUE;
      }
    }
    else {
      // load single file

      if (!strncmp(buff,"scrap|",6)) {
	is_scrap=TRUE;
	buffptr=buff+6;
      }
      else {
	buffptr=buff;
      }

      clipdir=g_build_filename(prefs->tmpdir,buffptr,NULL);

      if (!g_file_test(clipdir,G_FILE_TEST_IS_DIR)) {
	g_free(clipdir);
	continue;
      }
      g_free(clipdir);
      if ((new_file=mainw->first_free_file)==-1) {
	fclose(rfile);
	end_threaded_dialog();
	too_many_files();

	if (strlen(mainw->set_name)>0) recover_layout_map(mainw->current_file);

	if (mainw->multitrack!=NULL) {
	  mainw->current_file=mainw->multitrack->render_file;
	  polymorph(mainw->multitrack,POLY_NONE);
	  polymorph(mainw->multitrack,POLY_CLIPS);
	  mt_sensitise(mainw->multitrack);
	  mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	}

	mainw->suppress_dprint=FALSE;
	d_print_failed();
	return TRUE;
      }
      // TODO - dirsep
      if (strstr(buffptr,"/clips/")) {
	gchar **array;
	threaded_dialog_spin();
	array=g_strsplit(buffptr,"/clips/",-1);
	mainw->was_set=TRUE;
	g_snprintf(mainw->set_name,128,"%s",array[0]);
	g_strfreev(array);

	if (!did_set_check&&!check_for_lock_file(mainw->set_name,0)) {
	  do_set_locked_warning(mainw->set_name);
	  did_set_check=TRUE;
	}

	threaded_dialog_spin();
      }
      last_was_normal_file=TRUE;
      mainw->current_file=new_file;
      threaded_dialog_spin();
      cfile=(file *)(g_malloc(sizeof(file)));
      g_snprintf(cfile->handle,256,"%s",buffptr);
      cfile->clip_type=CLIP_TYPE_DISK; // the default

      //create a new cfile and fill in the details
      create_cfile();
      threaded_dialog_spin();

      if (!is_scrap) {
	// get file details
	read_headers(".");
      }
      else {
	mainw->scrap_file=mainw->current_file;
      }

      if (mainw->current_file<1) continue;

      if (load_frame_index(mainw->current_file)) {

	gboolean next=FALSE;
	while (1) {
	  threaded_dialog_spin();
	  if ((cdata=get_decoder_cdata(cfile,NULL))==NULL) {
	    if (mainw->error) {
	      if (do_original_lost_warning(cfile->file_name)) {
		
		// TODO ** - show layout errors
		
		continue;
	      }
	    }
	    else {
	      do_no_decoder_error(cfile->file_name);
	    }
	    next=TRUE;
	  }
	  threaded_dialog_spin();
	  break;
	}
	if (next) {
	  g_free(cfile);
	  mainw->first_free_file=mainw->current_file;
	  continue;
	}
	cfile->clip_type=CLIP_TYPE_FILE;
	get_mime_type(cfile->type,40,cdata);
      }

      if (cfile->ext_src!=NULL) {
	if (!check_clip_integrity(cfile,cdata)) {
	  g_free(cfile);
	  mainw->first_free_file=mainw->current_file;
	  continue;
	}
      }
      else {
	if (is_scrap||!check_frame_count(mainw->current_file)) get_frame_count(mainw->current_file);
      }
  
      if (!is_scrap) {
	// read the playback fps, play frame, and name
	threaded_dialog_spin();
	open_set_file (mainw->set_name,++clipnum);
	threaded_dialog_spin();
	
	if (mainw->cached_list!=NULL) {
	  threaded_dialog_spin();
	  g_list_free_strings(mainw->cached_list);
	  g_list_free(mainw->cached_list);
	  threaded_dialog_spin();
	  mainw->cached_list=NULL;
	}
	
	if (mainw->current_file<1) continue;
	
	get_total_time (cfile);
	if (cfile->achans) cfile->aseek_pos=(long)((gdouble)(cfile->frameno-1.)/cfile->fps*cfile->arate*cfile->achans*(cfile->asampsize/8));
	
	// add to clip menu
	threaded_dialog_spin();
	add_to_winmenu();
	get_next_free_file();
	cfile->start=cfile->frames>0?1:0;
	cfile->end=cfile->frames;
	cfile->is_loaded=TRUE;
	unlink (cfile->info_file);
	set_main_title(cfile->name,0);
	
	if (mainw->multitrack!=NULL) {
	  gint current_file=mainw->current_file;
	  lives_mt *multi=mainw->multitrack;
	  mainw->multitrack=NULL;
	  reget_afilesize (mainw->current_file);
	  mainw->multitrack=multi;
	  get_total_time(cfile);
	  mainw->current_file=mainw->multitrack->render_file;
	  mt_init_clips(mainw->multitrack,current_file,TRUE);
	  while (g_main_context_iteration(NULL,FALSE));
	  mt_clip_select(mainw->multitrack,TRUE);
	  while (g_main_context_iteration(NULL,FALSE));
	  mainw->current_file=current_file;
	}
	
	threaded_dialog_spin();
	
#ifdef ENABLE_OSC
	lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif
      }
      else {
	mainw->cliplist = g_list_append (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
	get_next_free_file();
      }
    }
  }

  end_threaded_dialog();

  fclose(rfile);

  if (strlen(mainw->set_name)>0) recover_layout_map(mainw->current_file);

  if (mainw->multitrack!=NULL) {
    mainw->current_file=mainw->multitrack->render_file;
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

  mainw->suppress_dprint=FALSE;
  d_print_done();
  return TRUE;
}





void add_to_recovery_file (const gchar *handle) {
  gchar *com=g_strdup_printf("/bin/echo \"%s\" >> \"%s\"",handle,mainw->recovery_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    return;
  }

  if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL) 
    write_backup_layout_numbering(mainw->multitrack);
}



void rewrite_recovery_file(void) {
  // part of the crash recovery system
  int i;
  gchar *recovery_entry;
  int recovery_fd=-1;
  int retval;
  GList *clist=mainw->cliplist;
  gboolean opened=FALSE;

  if (clist==NULL) {
    unlink(mainw->recovery_file);
    return;
  }

  do {
    retval=0;
    mainw->write_failed=FALSE;
    opened=FALSE;
    recovery_fd=-1;

    while (clist!=NULL) {
      i=GPOINTER_TO_INT(clist->data);
      if (mainw->files[i]->clip_type==CLIP_TYPE_FILE||mainw->files[i]->clip_type==CLIP_TYPE_DISK) {
	if (i!=mainw->scrap_file) recovery_entry=g_strdup_printf("%s\n",mainw->files[i]->handle);
	else recovery_entry=g_strdup_printf("scrap|%s\n",mainw->files[i]->handle);

	if (!opened) recovery_fd=creat(mainw->recovery_file,S_IRUSR|S_IWUSR);
	if (recovery_fd<0) retval=do_write_failed_error_s_with_retry(mainw->recovery_file,g_strerror(errno),NULL);
	else {
	  opened=TRUE;
	  lives_write(recovery_fd,recovery_entry,strlen(recovery_entry),TRUE);
	  if (mainw->write_failed) retval=do_write_failed_error_s_with_retry(mainw->recovery_file,NULL,NULL);
	}
	g_free(recovery_entry);
      }
      if (mainw->write_failed) break;
      clist=clist->next;
    }

  } while (retval==LIVES_RETRY);

  if (!opened) unlink(mainw->recovery_file);
  else if (recovery_fd>=0) close(recovery_fd);

  if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL) 
    write_backup_layout_numbering(mainw->multitrack);

}


gboolean check_for_recovery_files (gboolean auto_recover) {
  gboolean retval=FALSE;
  gchar *recovery_file,*recovery_numbering_file;
  guint recpid=0;

  size_t bytes;
  int info_fd;
  gchar *info_file=g_strdup_printf("%s/.recovery.%d",prefs->tmpdir,getpid());
  gchar *com=g_strdup_printf("smogrify get_recovery_file %d %d \"%s\" recovery> \"%s\"",getuid(),getgid(),
			     capable->myname,info_file);
  
  unlink(info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    return FALSE;
  }

  info_fd=open(info_file,O_RDONLY);
  if (info_fd>-1) {
    if ((bytes=read(info_fd,mainw->msg,256))>0) {
      memset(mainw->msg+bytes,0,1);
      if ((recpid=atoi(mainw->msg))>0) {
	
      }
    }
    close(info_fd);
  }
  unlink(info_file);
  g_free(info_file);

  if (recpid==0) return FALSE;
  
  retval=recover_files((recovery_file=g_strdup_printf("%s/recovery.%d.%d.%d",prefs->tmpdir,getuid(),
						      getgid(),recpid)),auto_recover);
  unlink(recovery_file);
  g_free(recovery_file);
  
  mainw->com_failed=FALSE;

  // check for layout recovery file
  recovery_file=g_strdup_printf("%s/layout.%d.%d.%d",prefs->tmpdir,getuid(),getgid(),recpid);
  if (g_file_test (recovery_file, G_FILE_TEST_EXISTS)) {
    // move files temporarily to stop them being cleansed
    com=g_strdup_printf("/bin/mv \"%s\" \"%s/.layout.%d.%d.%d\"",recovery_file,prefs->tmpdir,getuid(),getgid(),getpid());
    lives_system(com,FALSE);
    g_free(com);
    recovery_numbering_file=g_strdup_printf("%s/layout_numbering.%d.%d.%d",prefs->tmpdir,getuid(),getgid(),recpid);
    com=g_strdup_printf("/bin/mv \"%s\" \"%s/.layout_numbering.%d.%d.%d\"",recovery_numbering_file,prefs->tmpdir,
			getuid(),getgid(),getpid());
    lives_system(com,FALSE);
    g_free(com);
    g_free(recovery_numbering_file);
    mainw->recoverable_layout=TRUE;
  }
  else {
    if (mainw->scrap_file!=-1) close_scrap_file();
  }
  g_free(recovery_file);
  
  if (mainw->com_failed) return FALSE;

  com=g_strdup_printf("smogrify clean_recovery_files %d %d \"%s\"",getuid(),getgid(),capable->myname);
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->recoverable_layout) {
    recovery_file=g_strdup_printf("%s/.layout.%d.%d.%d",prefs->tmpdir,getuid(),getgid(),getpid());
    com=g_strdup_printf("/bin/mv \"%s\" \"%s/layout.%d.%d.%d\"",recovery_file,prefs->tmpdir,getuid(),getgid(),getpid());
    lives_system(com,FALSE);
    g_free(com);
    g_free(recovery_file);

    recovery_numbering_file=g_strdup_printf("%s/.layout_numbering.%d.%d.%d",prefs->tmpdir,getuid(),getgid(),getpid());
    com=g_strdup_printf("/bin/mv \"%s\" \"%s/layout_numbering.%d.%d.%d\"",recovery_numbering_file,prefs->tmpdir,getuid(),
			getgid(),getpid());
    lives_system(com,FALSE);
    g_free(com);
    g_free(recovery_numbering_file);
  }

  rewrite_recovery_file();

  if (!mainw->recoverable_layout) do_after_crash_warning();

  return retval;
}
