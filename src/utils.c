// utils.c
// LiVES
// (c) G. Finch 2003 - 2012 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/file.h>

#include "main.h"
#include "support.h"
#include "audio.h"
#include "resample.h"

static gboolean  omute,  osepwin,  ofs,  ofaded,  odouble;


char *filename_from_fd(char *val, int fd) {
  // return filename from an open fd, freeing val first

  // in case of error we return val


  // call like: foo=filename_from_fd(foo,fd);

  gchar *fdpath;
  gchar *fidi;
  char rfdpath[PATH_MAX];
  struct stat stb0,stb1;

  if (fstat(fd,&stb0)) return val;

  fidi=g_strdup_printf("%d",fd);
  fdpath=g_build_filename("/proc","self","fd",fidi,NULL);
  g_free(fidi);

  if (readlink(fdpath,rfdpath,PATH_MAX)==-1) return val;
  g_free(fdpath);

  if (stat(rfdpath,&stb1)) return val;

  if (stb0.st_dev!=stb1.st_dev) return val;

  if (stb0.st_ino!=stb1.st_ino) return val;

  if (val!=NULL) g_free(val);
  return g_strdup(rfdpath);
}



static void reverse_bytes(uint8_t *out, const uint8_t *in, size_t count) {
  register int i;
  for (i=0;i<count;i++) {
    out[i]=in[count-i-1];
  }
}



// system calls

int lives_system(const char *com, gboolean allow_error) {
  int retval;

  retval=system(com);

  if (retval) {
    gchar *msg=NULL;
    mainw->com_failed=TRUE;
    if (!allow_error) {
      msg=g_strdup_printf("lives_system failed with code %d: %s",retval,com);
      LIVES_ERROR(msg);
      do_system_failed_error(com,retval,NULL);
    }
#ifndef LIVES_NO_DEBUG
    else {
      msg=g_strdup_printf("lives_system failed with code %d: %s (not an error)",retval,com);
      LIVES_DEBUG(msg);
    }
#endif
    if (msg!=NULL) g_free(msg);
  }
  return retval;
}



pid_t lives_fork(const char *com) {
  // returns a negative number which is the pgid to lives_kill
  // to signal to sub process and all children
  // TODO *** - error check

  pid_t ret;

  if (!(ret=fork())) {
    setsid(); // create new session id
    setpgid(getpid(),0); // create new pgid
    lives_system(com,TRUE);
    _exit(0);
  }

  return ret;
}





ssize_t lives_write(int fd, const void *buf, size_t count, gboolean allow_fail) {
  ssize_t retval;
  retval=write(fd, buf, count);

  if (retval<count) {
    gchar *msg=NULL;
    mainw->write_failed=TRUE;
    mainw->write_failed_file=filename_from_fd(mainw->write_failed_file,fd);
    msg=g_strdup_printf("Write failed %"PRIu64" of %"PRIu64" in: %s",(unsigned long)retval,
			(unsigned long)count,mainw->write_failed_file);
    if (!allow_fail) {
      LIVES_ERROR(msg);
      close(fd);
    }
#ifndef LIVES_NO_DEBUG
    else {
      gchar *ffile=filename_from_fd(NULL,fd);
      msg=g_strdup_printf("Write failed %"PRIu64" of %"PRIu64" in: %s (not an error)",(unsigned long)retval,
			  (unsigned long)count,ffile);
      LIVES_DEBUG(msg);
      g_free(ffile);
    }
#endif
    if (msg!=NULL) g_free(msg);
  }
  return retval;
}




ssize_t lives_write_le(int fd, const void *buf, size_t count, gboolean allow_fail) {
  if (capable->byte_order==G_BIG_ENDIAN&&(prefs->bigendbug!=1)) {
    uint8_t xbuf[count];
    reverse_bytes(xbuf,(const uint8_t *)buf,count);
    return lives_write(fd,xbuf,count,allow_fail);
  }
  else {
    return lives_write(fd,buf,count,allow_fail);
  }

}



int lives_fputs(const char *s, FILE *stream) {
  int retval;

  retval=fputs(s,stream);

  if (retval==EOF) {
    mainw->write_failed=TRUE;
  }

  return retval;
}


char *lives_fgets(char *s, int size, FILE *stream) {
  char *retval;

  retval=fgets(s,size,stream);

  if (retval==NULL&&ferror(stream)) {
    mainw->read_failed=TRUE;
  }

  return retval;
}



ssize_t lives_read(int fd, void *buf, size_t count, gboolean allow_less) {
  ssize_t retval=read(fd, buf, count);

  if (retval<count) {
    gchar *msg=NULL;
    if (!allow_less||retval<0) {
      mainw->read_failed=TRUE;
      mainw->read_failed_file=filename_from_fd(mainw->read_failed_file,fd);
      msg=g_strdup_printf("Read failed %"PRIu64" of %"PRIu64" in: %s",(unsigned long)retval,
			  (unsigned long)count,mainw->read_failed_file);
      LIVES_ERROR(msg);
      close(fd);
    }
#ifndef LIVES_NO_DEBUG
    else {
      gchar *ffile=filename_from_fd(NULL,fd);
      msg=g_strdup_printf("Read got %"PRIu64" of %"PRIu64" in: %s (not an error)",(unsigned long)retval,
			  (unsigned long)count,ffile);
      LIVES_DEBUG(msg);
      g_free(ffile);
    }
#endif
    if (msg!=NULL) g_free(msg);
  }
  return retval;
}



ssize_t lives_read_le(int fd, void *buf, size_t count, gboolean allow_less) {
  if (capable->byte_order==G_BIG_ENDIAN&&!prefs->bigendbug) {
    uint8_t xbuf[count];
    ssize_t retval=lives_read(fd,buf,count,allow_less);
    if (retval<count) return retval;
    reverse_bytes((uint8_t *)buf,(const uint8_t *)xbuf,count);
    return retval;
  }
  else {
    return lives_read(fd,buf,count,allow_less);
  }

}



gchar *lives_format_storage_space_string(guint64 space) {
  gchar *fmt;

  if (space>lives_10pow(18)) {
    // TRANSLATORS: Exabytes
    fmt=g_strdup_printf(_("%.2f EB"),(gdouble)space/(gdouble)lives_10pow(18));
  }
  else if (space>lives_10pow(15)) {
    // TRANSLATORS: Petabytes
    fmt=g_strdup_printf(_("%.2f PB"),(gdouble)space/(gdouble)lives_10pow(15));
  }
  else if (space>lives_10pow(12)) {
    // TRANSLATORS: Terabytes
    fmt=g_strdup_printf(_("%.2f TB"),(gdouble)space/(gdouble)lives_10pow(12));
  }
  else if (space>lives_10pow(9)) {
    // TRANSLATORS: Gigabytes
    fmt=g_strdup_printf(_("%.2f GB"),(gdouble)space/(gdouble)lives_10pow(9));
  }
  else if (space>lives_10pow(6)) {
    // TRANSLATORS: Megabytes
    fmt=g_strdup_printf(_("%.2f MB"),(gdouble)space/(gdouble)lives_10pow(6));
  }
  else if (space>1024) {
    // TRANSLATORS: Kilobytes (1024 bytes)
    fmt=g_strdup_printf(_("%.2f KiB"),(gdouble)space/1024.);
  }
  else {
    fmt=g_strdup_printf(_("%d bytes"),space);
  }

  return fmt;
}




lives_storage_status_t get_storage_status(const char *dir, guint64 warn_level, guint64 *dsval) {
  guint64 ds;
  if (!is_writeable_dir(dir)) return LIVES_STORAGE_STATUS_UNKNOWN;
  ds=get_fs_free(dir);
  if (dsval!=NULL) *dsval=ds;
  if (ds<prefs->ds_crit_level) return LIVES_STORAGE_STATUS_CRITICAL;
  if (ds<warn_level) return LIVES_STORAGE_STATUS_WARNING;
  return LIVES_STORAGE_STATUS_NORMAL;
}




int lives_chdir(const char *path, gboolean allow_fail) {
  int retval;

  retval=chdir(path);

  if (retval) {
    gchar *msg=g_strdup_printf("Chdir failed to: %s",path);
    mainw->chdir_failed=TRUE;
    if (!allow_fail) {
      LIVES_ERROR(msg);
      do_chdir_failed_error(path);
    }
    else LIVES_DEBUG(msg);
    g_free(msg);
  }
  return retval;
}



LIVES_INLINE void lives_freep(void **ptr) {
  // free a pointer and nullify it, only if it is non-null to start with
  // pass the address of the pointer in
  if (ptr!=NULL&&*ptr!=NULL) {
    g_free(*ptr);
    *ptr=NULL;
  }
}




// special de-allocators which avoid free()ing mainw->do_not_free...this is necessary to "hack" into gdk-pixbuf
// do not use this directly in code (use g_free() or weed_free() as appropriate)
void lives_free(gpointer ptr) {
  (*mainw->free_fn)(ptr);
}

// special de-allocators which avoid free()ing mainw->do_not_free...this is necessary to "hack" into gdk-pixbuf
// do not use this directly in code (use g_free() or weed_free() as appropriate)
void lives_free_with_check(gpointer ptr) {
  if (ptr==mainw->do_not_free) return;
  free(ptr);
}




LIVES_INLINE int lives_kill(pid_t pid, int sig) {
  return kill(pid,sig);
};

LIVES_INLINE int lives_killpg(int pgrp, int sig) {
  return killpg(pgrp,sig);
};


LIVES_INLINE gint myround(gdouble n) {
  return (n>=0.)?(gint)(n + 0.5):(gint)(n - 0.5);
}

LIVES_INLINE void clear_mainw_msg (void) {
  memset (mainw->msg,0,512);
}


LIVES_INLINE uint64_t lives_10pow(int pow) {
  register int i;
  uint64_t res=1;
  for (i=0;i<pow;i++) res*=10;
  return res;
}

LIVES_INLINE int get_approx_ln(guint x) {
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  x++;
  return x>>1;
}


/**  return current (wallclock) time in ticks (units of 10 nanoseconds)
 */

int64_t lives_get_current_ticks(void) {
  gettimeofday(&tv, NULL);
  return U_SECL*tv.tv_sec+tv.tv_usec*U_SEC_RATIO;
}





/** set alarm for now + delta ticks (10 nanosec)
 * param ticks (10 nanoseconds) is the offset when we want our alarm to trigger 
 * returns int handle or -1
 * call lives_get_alarm(handle) to test if time arrived
 */

int lives_alarm_set(int64_t ticks) {
  int i;
  int64_t cticks;

  // we will assign [this] next
  int ret=mainw->next_free_alarm;

  // no alarm slots left
  if (mainw->next_free_alarm==-1) {
    LIVES_WARN("No alarms left");
    return -1;
  }

  // get current ticks
  cticks=lives_get_current_ticks();

  // set to now + offset
  mainw->alarms[mainw->next_free_alarm]=cticks+ticks;

  i=++mainw->next_free_alarm;

  // find free slot for next time
  while (mainw->alarms[i]!=LIVES_NO_ALARM_TICKS&&i<LIVES_MAX_ALARMS) {
    i++;
  }
  if (i==LIVES_MAX_ALARMS) {
    // no slots left
    mainw->next_free_alarm=-1;
  }
  // OK
  else mainw->next_free_alarm=i;

  return ret+1;
}


/*** check if alarm time passed yet, if so clear that alarm and return TRUE
 * else return FALSE
 */

gboolean lives_alarm_get(int alarm_handle) {
  int64_t cticks;

  // invalid alarm number
  if (alarm_handle<=0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid get alarm handle");
    return FALSE;
  }

  // offset of 1 was added for caller
  alarm_handle--;

  // alarm time was never set !
  if (mainw->alarms[alarm_handle]==LIVES_NO_ALARM_TICKS) {
    LIVES_WARN("Alarm time not set");
    return TRUE;
  }

  // get current ticks
  cticks=lives_get_current_ticks();

  if (cticks>mainw->alarms[alarm_handle]) {
    // reached alarm time, free up this timer and return TRUE
    mainw->alarms[alarm_handle]=LIVES_NO_ALARM_TICKS;

    if (mainw->next_free_alarm==-1 || (alarm_handle<mainw->next_free_alarm)) {
      mainw->next_free_alarm=alarm_handle;
      mainw->alarms[alarm_handle]=LIVES_NO_ALARM_TICKS;
      LIVES_DEBUG("Alarm reached");
      return TRUE;
    }

  }

  // alarm time not reached yet
  return FALSE;
}



void lives_alarm_clear(int alarm_handle) {
  if (alarm_handle<=0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid clear alarm handle");
    return;
  }

  alarm_handle--;

  mainw->alarms[alarm_handle]=LIVES_NO_ALARM_TICKS;
  if (mainw->next_free_alarm==-1 || alarm_handle<mainw->next_free_alarm) 
    mainw->next_free_alarm=alarm_handle;
}



LIVES_INLINE gchar *g_strappend (gchar *string, gint len, const gchar *xnew) {
  gchar *tmp=g_strconcat (string,xnew,NULL);
  g_snprintf(string,len,"%s",tmp);
  g_free(tmp);
  return string;
}


LIVES_INLINE GList *g_list_append_unique(GList *xlist, const gchar *add) {
  if (g_list_find_custom(xlist,add,(GCompareFunc)strcmp)==NULL) return g_list_append(xlist,g_strdup(add));
  return xlist;
}



#ifdef IS_IRIX
void setenv(const char *name, const char *val, int _xx) {
  int len  = strlen(name) + strlen(val) + 2;
  char *env = malloc(len);

  if (env != NULL) {
    strcpy(env, name);
    strcat(env, "=");
    strcat(env, val);
    putenv(env);
  }
}
#endif



/* convert to/from a big endian 32 bit float for internal use */
LIVES_INLINE float LEFloat_to_BEFloat(float f) {
  char *b=(char *)(&f);
  if (G_BYTE_ORDER==G_LITTLE_ENDIAN) {
    float fl;
    guchar rev[4];
    rev[0]=b[3];
    rev[1]=b[2];
    rev[2]=b[1];
    rev[3]=b[0];
    fl=*(float *)rev;
    return fl;
  }
  return f;
}


LIVES_INLINE gdouble calc_time_from_frame (gint clip, gint frame) {
  return (frame-1.)/mainw->files[clip]->fps;
}

LIVES_INLINE gint calc_frame_from_time (gint filenum, gdouble time) {
  // return the nearest frame (rounded) for a given time, max is cfile->frames
  int frame=0;
  if (time<0.) return mainw->files[filenum]->frames?1:0;
  frame=(gint)(time*mainw->files[filenum]->fps+1.49999);
  return (frame<mainw->files[filenum]->frames)?frame:mainw->files[filenum]->frames;
}

LIVES_INLINE gint calc_frame_from_time2 (gint filenum, gdouble time) {
  // return the nearest frame (rounded) for a given time
  // allow max (frames+1)
  int frame=0;
  if (time<0.) return mainw->files[filenum]->frames?1:0;
  frame=(gint)(time*mainw->files[filenum]->fps+1.49999);
  return (frame<mainw->files[filenum]->frames+1)?frame:mainw->files[filenum]->frames+1;
}

LIVES_INLINE gint calc_frame_from_time3 (gint filenum, gdouble time) {
  // return the nearest frame (floor) for a given time
  // allow max (frames+1)
  int frame=0;
  if (time<0.) return mainw->files[filenum]->frames?1:0;
  frame=(gint)(time*mainw->files[filenum]->fps+1.);
  return (frame<mainw->files[filenum]->frames+1)?frame:mainw->files[filenum]->frames+1;
}





static gboolean check_for_audio_stop (gint fileno, gint first_frame, gint last_frame) {
  // return FALSE if audio stops playback

#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&mainw->jackd->playing_file==fileno) {
    if (!mainw->loop) {
      if (!mainw->loop_cont) {
	if (mainw->aframeno<first_frame||mainw->aframeno>last_frame) {
	  return FALSE;
	}
      }
    }
    else {
      if (!mainw->loop_cont) {
	if (mainw->aframeno<1||
	    calc_time_from_frame(mainw->current_file,mainw->aframeno)>cfile->laudio_time) { 
	  return FALSE;
	}
      }
    }
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&mainw->pulsed->playing_file==fileno) {
    if (!mainw->loop) {
      if (!mainw->loop_cont) {
	if (mainw->aframeno<first_frame||mainw->aframeno>last_frame) {
	  return FALSE;
	}
      }
    }
    else {
      if (!mainw->loop_cont) {
	if (mainw->aframeno<1||
	    calc_time_from_frame(mainw->current_file,mainw->aframeno)>cfile->laudio_time) { 
	  return FALSE;
	}
      }
    }
  }
#endif
  return TRUE;
}


void calc_aframeno(gint fileno) {
#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK&&(mainw->jackd!=NULL||mainw->jackd_read!=NULL)&&
      mainw->jackd->playing_file==fileno) {
    // get seek_pos from jack
    if (mainw->jackd_read!=NULL) mainw->aframeno=lives_jack_get_pos(mainw->jackd_read)/cfile->fps+1.;
    else mainw->aframeno=lives_jack_get_pos(mainw->jackd)/cfile->fps+1.;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE&&(mainw->pulsed!=NULL||mainw->pulsed_read!=NULL)&&
      mainw->pulsed->playing_file==fileno) {
    // get seek_pos from pulse
    if (mainw->pulsed_read!=NULL) mainw->aframeno=lives_pulse_get_pos(mainw->pulsed_read)/cfile->fps+1.;
    else mainw->aframeno=lives_pulse_get_pos(mainw->pulsed)/cfile->fps+1.;
  }
#endif
}




gint calc_new_playback_position(gint fileno, gint64 otc, gint64 *ntc) {
  // returns a frame number (floor) using sfile->last_frameno and ntc-otc
  // takes into account looping modes

  // the range is first_frame -> last_frame

  // which is generally 1 -> sfile->frames, unless we are playing a selection

  // in case the frame is out of range and playing, returns 0 and sets mainw->cancelled 

  // ntc is adjusted backwards to timecode of the new frame


  // the basic operation is quite simple, given the time difference between the last frame and 
  // now, we calculate the new frame from the current fps and then ensure it is in the range
  // first_frame -> last_frame

  // Complications arise because we have ping-pong loop mode where the the play direction 
  // alternates - here we need to determine how many times we have reached the start or end 
  // play point. This is similar to the winding number in topological calculations.

  // caller should check return value of ntc, and if it differs from otc, show the frame


  // note we also calculate the audio "frame" and position for realtime audio players
  // this is done so we can check here if audio limits stopped playback


  gint64 dtc=*ntc-otc;
  file *sfile=mainw->files[fileno];

  gint dir=0;
  gint cframe,nframe;

  gint first_frame,last_frame;

  gboolean do_resync=FALSE;

  gdouble fps;

  if (sfile==NULL) return 0;

  fps=sfile->pb_fps;

  if (mainw->playing_file==-1) fps=sfile->fps;

  cframe=sfile->last_frameno;

  if (fps==0.) {
    *ntc=otc;
    calc_aframeno(fileno);
    return cframe;
  }

  // dtc is delt ticks, quantise this to the frame rate and round down
  dtc=q_gint64_floor(dtc,fps);

  // ntc is the time when the frame should have been played
  *ntc=otc+dtc;

  // nframe is our new frame
  nframe=cframe+(gint)((gdouble)dtc/U_SEC*fps+(fps>0?.5:-.5));

  if (nframe==cframe||mainw->foreign) return nframe;

  // calculate audio "frame" from the number of samples played
  if (mainw->playing_file==fileno) {
    calc_aframeno(fileno);
  }

  if (mainw->playing_file==fileno) {
    last_frame=(mainw->playing_sel&&!mainw->is_rendering)?sfile->end:mainw->play_end;
    first_frame=mainw->playing_sel?sfile->start:mainw->play_start;
  }
  else {
    last_frame=sfile->frames;
    first_frame=1;
  }


  if (mainw->playing_file==fileno) {

    if (mainw->noframedrop) {
      // if noframedrop is set, we may not skip any frames
      // - the usual situation is that we are allowed to skip frames
      if (nframe>cframe) nframe=cframe+1;
      else if (nframe<cframe) nframe=cframe-1;
    }

    // check if video stopped playback
    if (nframe<first_frame||nframe>last_frame) {
      if (mainw->whentostop==STOP_ON_VID_END) {
	mainw->cancelled=CANCEL_VID_END;
	return 0;
      }
    }


    // check if audio stopped playback
#ifdef RT_AUDIO
    if (mainw->whentostop==STOP_ON_AUD_END&&sfile->achans>0&&sfile->frames>0) {
      if (!check_for_audio_stop(fileno,first_frame,last_frame)) {
	mainw->cancelled=CANCEL_AUD_END;
	return 0;
      }
    }
#endif
  }
  
  if (sfile->frames==0) return 0;

  // get our frame back to within bounds
  
  nframe-=first_frame;

  if (fps>0) {
    dir=0;
    if (mainw->ping_pong) {
      dir=(gint)((gdouble)nframe/(gdouble)(last_frame-first_frame+1));
      dir%=2;
    }
  }
  else {
    dir=1;
    if (mainw->ping_pong) {
      nframe-=(last_frame-first_frame);
      dir=(gint)((gdouble)nframe/(gdouble)(last_frame-first_frame+1));
      dir%=2;
      dir++;
    }
  }

  nframe%=(last_frame-first_frame+1);

  if (fps<0) {
    // backwards
    if (dir==1) {
      // even winding
      if (!mainw->ping_pong) {
	// loop
	if (nframe<0) nframe+=last_frame+1;
	else nframe+=first_frame;
	if (nframe>cframe&&mainw->playing_file==fileno&&mainw->loop_cont&&!mainw->loop) {
	  // resync audio at end of loop section (playing backwards)
	  do_resync=TRUE;
	}
      }
      else {
	nframe+=last_frame; // normal
	if (nframe>last_frame) {
	  nframe=last_frame-(nframe-last_frame);
	  if (mainw->playing_file==fileno) dirchange_callback (NULL,NULL,0,(GdkModifierType)0,GINT_TO_POINTER(FALSE));
	  else sfile->pb_fps=-sfile->pb_fps;
	}
      }
    }
    else {
      // odd winding
      nframe=ABS(nframe)+first_frame;
      if (mainw->ping_pong) {
	// bounce
	if (mainw->playing_file==fileno) dirchange_callback (NULL,NULL,0,(GdkModifierType)0,GINT_TO_POINTER(FALSE));
	else sfile->pb_fps=-sfile->pb_fps;
      }
    }
  }
  else {
    // forwards
    nframe+=first_frame;
    if (dir==1) {
      // odd winding
      if (mainw->ping_pong) {
	// bounce
	nframe=last_frame-(nframe-(first_frame-1));
	if (mainw->playing_file==fileno) dirchange_callback (NULL,NULL,0,(GdkModifierType)0,GINT_TO_POINTER(FALSE));
	else sfile->pb_fps=-sfile->pb_fps;
      }
    }
    else if (mainw->playing_sel&&!mainw->ping_pong&&mainw->playing_file==fileno&&nframe<cframe&&mainw->loop_cont&&!mainw->loop) {
      // resync audio at start of loop selection
      if (nframe<first_frame) {
	nframe=last_frame-(first_frame-nframe)+1;
      }
      do_resync=TRUE;
    }
    if (nframe<first_frame) {
      // scratch or transport backwards
      if (mainw->ping_pong) {
	nframe=first_frame;
	if (mainw->playing_file==fileno) dirchange_callback (NULL,NULL,0,(GdkModifierType)0,GINT_TO_POINTER(FALSE));
	else sfile->pb_fps=-sfile->pb_fps;

      }
      else nframe=last_frame-nframe;
    }
  }

  if (nframe<first_frame) nframe=first_frame;
  if (nframe>last_frame) nframe=last_frame;

  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
    if (do_resync||(mainw->scratch!=SCRATCH_NONE&&mainw->playing_file==fileno)) {
      gboolean is_jump=FALSE;
      if (mainw->scratch==SCRATCH_JUMP) is_jump=TRUE;
      mainw->scratch=SCRATCH_NONE;
      resync_audio(nframe);
      if (is_jump) mainw->video_seek_ready=TRUE;
      if (mainw->whentostop==STOP_ON_AUD_END&&sfile->achans>0) {
	// we check for audio stop here, but the seek may not have happened yet
	if (!check_for_audio_stop(fileno,first_frame,last_frame)) {
	  mainw->cancelled=CANCEL_AUD_END;
	  return 0;
	}
      }
    }
  }

  return nframe;
}





void calc_maxspect(gint rwidth, gint rheight, gint *cwidth, gint *cheight) {
  // calculate maxspect (maximum size which maintains aspect ratio)
  // of cwidth, cheight - given restrictions rwidth * rheight

  gdouble aspect;

  if (*cwidth<=0||*cheight<=0||rwidth<=0||rheight<=0) return;

  if (*cwidth>rwidth) {
    // image too wide shrink it
    aspect=(gdouble)rwidth/(gdouble)(*cwidth);
    *cwidth=rwidth;
    *cheight=(gdouble)(*cheight)*aspect;
  }
  if (*cheight>rheight) {
    // image too tall shrink it
    aspect=(gdouble)rheight/(gdouble)(*cheight);
    *cheight=rheight;
    *cwidth=(gdouble)(*cwidth)*aspect;
  }

  aspect=(gdouble)*cwidth/(gdouble)*cheight;

  if ((gdouble)rheight*aspect<=rwidth) {
    // bound by rheight
    *cheight=rheight;
    *cwidth=((gdouble)rheight*aspect+.5);
    if (*cwidth>rwidth) *cwidth=rwidth;
  }
  else {
    // bound by rwidth
    *cwidth=rwidth;
    *cheight=((gdouble)rwidth/aspect+.5);
    if (*cheight>rheight) *cheight=rheight;
  }
}




/////////////////////////////////////////////////////////////////////////////


void init_clipboard(void) {
  gint current_file=mainw->current_file;
  gchar *com;

  if (clipboard==NULL) {
    // here is where we create the clipboard
    // use get_new_handle(clipnumber,name);
    if (!get_new_handle(0,"clipboard")) {
      mainw->error=TRUE;
      return;
    }
  }
  else if (clipboard->frames>0) { 
    // clear old clipboard
    // need to set current file to 0 before monitoring progress
    mainw->current_file=0;
    mainw->com_failed=FALSE;
    com=g_strdup_printf("smogrify delete_all \"%s\"",clipboard->handle);
    unlink(clipboard->info_file);
    lives_system(com,FALSE);
    g_free(com);

    if (mainw->com_failed) {
      mainw->current_file=current_file;
      return;
    }

    cfile->progress_start=cfile->start;
    cfile->progress_end=cfile->end;
    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_ ("Clearing the clipboard"));
  }

  mainw->current_file=current_file;
}



void d_print(const gchar *text) {
  // print out output in the main message area (and info log)

  

  // there are several small tweaks for this:

  // mainw->suppress_dprint :: TRUE - dont print anything, return (for silencing noisy message blocks)
  // mainw->no_switch_dprint :: TRUE - disable printing of switch message when maine->current_file changes

  // mainw->last_dprint_file :: clip number of last mainw->current_file;
  gchar *switchtext,*tmp;

  GtkTextIter end_iter;
  GtkTextMark *mark;

  GtkTextBuffer *tbuf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(mainw->textview1));

  if (!capable->smog_version_correct) return;

  if (mainw->suppress_dprint) return;

  if (GTK_IS_TEXT_VIEW (mainw->textview1)) {
    gtk_text_buffer_get_end_iter(tbuf,&end_iter);
    gtk_text_buffer_insert(tbuf,&end_iter,text,-1);
    if (mainw->current_file!=mainw->last_dprint_file&&mainw->current_file!=0&&mainw->multitrack==NULL&&
	(mainw->current_file==-1||cfile->clip_type!=CLIP_TYPE_GENERATOR)&&!mainw->no_switch_dprint) {
      if (mainw->current_file>0) {
	switchtext=g_strdup_printf (_ ("\n==============================\nSwitched to clip %s\n"),
				    cfile->clip_type!=CLIP_TYPE_VIDEODEV?(tmp=g_path_get_basename(cfile->name)):
				    (tmp=g_strdup(cfile->name)));
	g_free(tmp);
      }
      else {
	switchtext=g_strdup (_ ("\n==============================\nSwitched to empty clip\n"));
      }
      gtk_text_buffer_get_end_iter(tbuf,&end_iter);
      gtk_text_buffer_insert(tbuf,&end_iter,switchtext,-1);
      g_free (switchtext);
    }
    if ((mainw->current_file==-1||cfile->clip_type!=CLIP_TYPE_GENERATOR)&&
	(!mainw->no_switch_dprint||mainw->current_file!=0)) mainw->last_dprint_file=mainw->current_file;
    gtk_text_buffer_get_end_iter(tbuf,&end_iter);
    mark=gtk_text_buffer_create_mark(tbuf,NULL,&end_iter,FALSE);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW (mainw->textview1),mark);
    gtk_text_buffer_delete_mark (tbuf,mark);
  }
}



gboolean add_lmap_error(lives_lmap_error_t lerror, const gchar *name, gpointer user_data, gint clipno, 
			gint frameno, gdouble atime, gboolean affects_current) {
  // potentially add a layout map error to the layout textbuffer
  GtkTextIter end_iter;
  gchar *text,*name2;
  gchar **array;
  GList *lmap;
  gdouble orig_fps;
  gint resampled_frame;
  gdouble max_time;

  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter);

  if (affects_current&&user_data==NULL) {
    mainw->affected_layout_marks=g_list_append(mainw->affected_layout_marks,
					       (gpointer)gtk_text_buffer_create_mark
					       (GTK_TEXT_BUFFER(mainw->layout_textbuffer),NULL,&end_iter,TRUE));
  }

  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    if (strlen(name)==0) name2=g_strdup(_("(blank)"));
    else name2=g_strdup(name);
    text=g_strdup_printf
      (_("The set name has been changed from %s to %s. Affected layouts have been updated accordingly\n"),
       name2,(gchar *)user_data);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(name2);
    g_free(text);
    break;
  case LMAP_ERROR_MISSING_CLIP:
    if (prefs->warning_mask&WARN_MASK_LAYOUT_MISSING_CLIPS) return FALSE;
    text=g_strdup_printf(_("The clip %s is missing from this set.\nIt is required by the following layouts:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
  case LMAP_ERROR_CLOSE_FILE:
    text=g_strdup_printf(_("The clip %s has been closed.\nIt is required by the following layouts:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
    text=g_strdup_printf(_("Frames have been shifted in the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_DELETE_FRAMES:
    text=g_strdup_printf(_("Frames have been deleted from the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_DELETE_AUDIO:
    text=g_strdup_printf(_("Audio has been deleted from the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
    text=g_strdup_printf(_("Audio has been shifted in clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_ALTER_AUDIO:
    text=g_strdup_printf(_("Audio has been altered in the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_ALTER_FRAMES:
    text=g_strdup_printf(_("Frames have been altered in the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  }

  if (affects_current&&user_data!=NULL) {
    mainw->affected_layout_marks=g_list_append(mainw->affected_layout_marks,
					       (gpointer)gtk_text_buffer_create_mark
					       (GTK_TEXT_BUFFER(mainw->layout_textbuffer),NULL,&end_iter,TRUE));
  }

  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    lmap=mainw->current_layouts_map;
    while (lmap!=NULL) {
      array=g_strsplit((gchar *)lmap->data,"|",-1);
      text=g_strdup_printf("%s\n",array[0]);
      gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
      g_free(text);
      //mainw->affected_layouts_map=g_list_append_unique(mainw->affected_layouts_map,array[0]);
      g_strfreev(array);
      lmap=lmap->next;
    }
    break;
  case LMAP_ERROR_MISSING_CLIP:
  case LMAP_ERROR_CLOSE_FILE:
    if (affects_current) {
      text=g_strdup_printf("%s\n",mainw->cl_string);
      gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
      g_free(text);
      mainw->affected_layouts_map=g_list_append_unique(mainw->affected_layouts_map,mainw->cl_string);

      mainw->affected_layout_marks=g_list_append(mainw->affected_layout_marks,(gpointer)gtk_text_buffer_create_mark(GTK_TEXT_BUFFER(mainw->layout_textbuffer),NULL,&end_iter,TRUE));

    }
    lmap=(GList *)user_data;
    while (lmap!=NULL) {
      array=g_strsplit((gchar *)lmap->data,"|",-1);
      text=g_strdup_printf("%s\n",array[0]);
      gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
      g_free(text);
      mainw->affected_layouts_map=g_list_append_unique(mainw->affected_layouts_map,array[0]);
      g_strfreev(array);
      lmap=lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
  case LMAP_ERROR_DELETE_FRAMES:
  case LMAP_ERROR_ALTER_FRAMES:
    if (affects_current) {
      text=g_strdup_printf("%s\n",mainw->cl_string);
      gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
      g_free(text);
      mainw->affected_layouts_map=g_list_append_unique(mainw->affected_layouts_map,mainw->cl_string);

      mainw->affected_layout_marks=g_list_append(mainw->affected_layout_marks,(gpointer)gtk_text_buffer_create_mark(GTK_TEXT_BUFFER(mainw->layout_textbuffer),NULL,&end_iter,TRUE));
    }
    lmap=(GList *)user_data;
    while (lmap!=NULL) {
      array=g_strsplit((gchar *)lmap->data,"|",-1);
      orig_fps=strtod(array[3],NULL);
      resampled_frame=count_resampled_frames(frameno,orig_fps,mainw->files[clipno]->fps);
      if (resampled_frame<=atoi(array[2])) {
	text=g_strdup_printf("%s\n",array[0]);
	gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
	g_free(text);
	mainw->affected_layouts_map=g_list_append_unique(mainw->affected_layouts_map,array[0]);
      }
      g_strfreev(array);
      lmap=lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
  case LMAP_ERROR_DELETE_AUDIO:
  case LMAP_ERROR_ALTER_AUDIO:
    if (affects_current) {
      text=g_strdup_printf("%s\n",mainw->cl_string);
      gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
      g_free(text);
      mainw->affected_layouts_map=g_list_append_unique(mainw->affected_layouts_map,mainw->cl_string);

      mainw->affected_layout_marks=g_list_append(mainw->affected_layout_marks,(gpointer)gtk_text_buffer_create_mark(GTK_TEXT_BUFFER(mainw->layout_textbuffer),NULL,&end_iter,TRUE));
    }
    lmap=(GList *)user_data;
    while (lmap!=NULL) {
      array=g_strsplit((gchar *)lmap->data,"|",-1);
      max_time=strtod(array[4],NULL);
      if (max_time>0.&&atime<=max_time) {
	text=g_strdup_printf("%s\n",array[0]);
	gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
	g_free(text);
	mainw->affected_layouts_map=g_list_append_unique(mainw->affected_layouts_map,array[0]);
      }
      g_strfreev(array);
      lmap=lmap->next;
    }
    break;
  }

  gtk_widget_set_sensitive (mainw->show_layout_errors, TRUE);
  if (mainw->multitrack!=NULL) gtk_widget_set_sensitive (mainw->multitrack->show_layout_errors, TRUE);
  return TRUE;
}


void clear_lmap_errors(void) {
  GtkTextIter start_iter,end_iter;
  GList *lmap;

  gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&start_iter);
  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter);
  gtk_text_buffer_delete(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&start_iter,&end_iter);

  lmap=mainw->affected_layouts_map;

  while (lmap!=NULL) {
    g_free(lmap->data);
    lmap=lmap->next;
  }
  g_list_free(lmap);

  mainw->affected_layouts_map=NULL;
  gtk_widget_set_sensitive (mainw->show_layout_errors, FALSE);
  if (mainw->multitrack!=NULL) gtk_widget_set_sensitive (mainw->multitrack->show_layout_errors, FALSE);

  if (mainw->affected_layout_marks!=NULL) {
    remove_current_from_affected_layouts(mainw->multitrack);
  }

}


gboolean check_for_lock_file(const gchar *set_name, gint type) {
  // check for lock file
  // do this via the back-end (smogrify)
  // this allows for the locking scheme to be more flexible

  // smogrify indicates a lock very simply by by writing >0 bytes to stdout
  // we redirect the output to info_file and read it

  int info_fd;
  gchar *msg=NULL;
  ssize_t bytes;

  gchar *info_file=g_strdup_printf("%s/.locks.%d",prefs->tmpdir,getpid());
  gchar *com=g_strdup_printf("smogrify check_for_lock \"%s\" \"%s\" %d >\"%s\"",set_name,capable->myname,
			     getpid(),info_file);

  unlink(info_file);
  threaded_dialog_spin();
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  threaded_dialog_spin();
  g_free(com);

  clear_mainw_msg();

  if (mainw->com_failed) return FALSE;

  info_fd=open(info_file,O_RDONLY);
  if (info_fd>-1) {
    if ((bytes=read(info_fd,mainw->msg,256))>0) {
      close(info_fd);
      memset(mainw->msg+bytes,0,1);

      if (type==0) {
	msg=g_strdup_printf(_("Set %s\ncannot be opened, as it is in use\nby another copy of LiVES.\n"),set_name);
	threaded_dialog_spin();
	do_error_dialog(msg);
	threaded_dialog_spin();
      }
      else if (type==1) {
	msg=g_strdup_printf
	  (_("\nThe set %s is currently in use by another copy of LiVES.\nPlease choose another set name.\n"),set_name);
	if (!mainw->osc_auto) do_blocking_error_dialog(msg);
      }
      if (msg!=NULL) {
	g_free(msg);
      }
      unlink(info_file);
      g_free(info_file);
      return FALSE;
    }
  }
  close (info_fd);
  unlink(info_file);
  g_free(info_file);

  return TRUE;
}


gboolean is_legal_set_name(const gchar *set_name, gboolean allow_dupes) {
  // check (clip) set names for validity
  // - may not be of zero length
  // - may not contain spaces or characters / \ * "
  // - must NEVER be name of a set in use by another copy of LiVES (i.e. with a lock file)

  // - as of 1.6.0:
  // -  may not start with a .
  // -  may not contain ..

  // may not be longer than 128 chars

  // iff allow_dupes is FALSE then we disallow the name of any existing set (has a subdirectory in the working directory)

  int i;

  gchar *msg;
  gchar *reject=" /\\*\"";
  size_t slen=strlen(set_name);

  if (slen==0) {
    if (!mainw->osc_auto) do_blocking_error_dialog(_("\nSet names may not be blank.\n"));
    return FALSE;
  }

  if (slen>128) {
    if (!mainw->osc_auto) do_blocking_error_dialog(_("\nSet names may not be longer than 128 characters.\n"));
    return FALSE;
  }

  if (strcspn(set_name,reject)!=slen) {
    msg=g_strdup_printf(_("\nSet names may not contain spaces or the characters%s.\n"),reject);
    if (!mainw->osc_auto) do_blocking_error_dialog(msg);
    g_free(msg);
    return FALSE;
  }

  for (i=0;i<slen;i+=2) {
    if (set_name[i]=='.'&&((i==0||set_name[i-1]=='.')||(i<slen-1&&set_name[i+1]=='.'))) {
      msg=g_strdup(_("\nSet names may not start with a '.' or contain '..'\n"));
      if (!mainw->osc_auto) do_blocking_error_dialog(msg);
      g_free(msg);
      return FALSE;
    }
  }

  // check if this is a set in use by another copy of LiVES
  if (!check_for_lock_file(set_name,1)) return FALSE;
  
  if (!allow_dupes) {
    // check for duplicate set names
    gchar *set_dir=g_build_filename(prefs->tmpdir,set_name,NULL);
    if (g_file_test(set_dir,G_FILE_TEST_IS_DIR)) {
      g_free(set_dir);
      msg=g_strdup_printf(_("\nThe set %s already exists.\nPlease choose another set name.\n"),set_name);
      do_blocking_error_dialog(msg);
      g_free(msg);
      return FALSE;
    }
    g_free(set_dir);
  }

  return TRUE;
}




gboolean check_frame_count(gint idx) {
  // check number of frames is correct
  // for files of type CLIP_TYPE_DISK
  // - check the image files (e.g. jpeg or png)

  // use a "goldilocks" algorithm (just the right frames, not too few and not too many

  // ingores gaps

  // make sure nth frame is there...
  gchar *frame=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,mainw->files[idx]->handle,mainw->files[idx]->frames,
			       mainw->files[idx]->img_type==IMG_TYPE_JPEG?"jpg":"png");

  if (!g_file_test(frame,G_FILE_TEST_EXISTS)) {
    // not enough frames
    g_free(frame);
    return FALSE;
  }
  g_free(frame);

  // ...make sure n + 1 th frame is not
  frame=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,mainw->files[idx]->handle,mainw->files[idx]->frames+1,
			mainw->files[idx]->img_type==IMG_TYPE_JPEG?"jpg":"png");
  if (g_file_test(frame,G_FILE_TEST_EXISTS)) {
    // too many frames
    g_free(frame);
    return FALSE;
  }
  g_free(frame);

  // just right
  return TRUE;
}



void get_frame_count(gint idx) {
  // sets mainw->files[idx]->frames with current framecount

  // calls smogrify which physically finds the last frame using a (fast) O(log n) binary search method

  // for CLIP_TYPE_DISK only

  // (CLIP_TYPE_FILE should use the decoder plugin frame count)

  gint info_fd;
  int retval;
  ssize_t bytes;
  gchar *info_file=g_strdup_printf("%s/.check.%d",prefs->tmpdir,getpid());
  gchar *com=g_strdup_printf("smogrify count_frames \"%s\" \"%s\" > \"%s\"",mainw->files[idx]->handle,
			     mainw->files[idx]->img_type==IMG_TYPE_JPEG?"jpg":"png",info_file);

  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);
  
  if (mainw->com_failed) {
    g_free(info_file);
    return;
  }

  do {
    retval=0;
    info_fd=open(info_file,O_RDONLY);
    if (info_fd<0) {
      retval=do_read_failed_error_s_with_retry(info_file,g_strerror(errno),NULL);
    }
    else {
      if ((bytes=lives_read(info_fd,mainw->msg,256,TRUE))>0) {
	if (bytes==0) {
	  retval=do_read_failed_error_s_with_retry(info_file,NULL,NULL);
	}
	else {
	  memset(mainw->msg+bytes,0,1);
	  mainw->files[idx]->frames=atoi(mainw->msg);
	}
      }
      close(info_fd);
    }
  } while (retval==LIVES_RETRY);

  unlink(info_file);
  g_free(info_file);
}






void get_next_free_file(void) {
  // get next free file slot, or -1 if we are full
  // can support MAX_FILES files (default 65536)
  while ((mainw->first_free_file!=-1)&&mainw->files[mainw->first_free_file]!=NULL) {
    mainw->first_free_file++;
    if (mainw->first_free_file>=MAX_FILES) mainw->first_free_file=-1;
  }
}


void get_dirname(gchar *filename) {
  gchar *tmp;
  // get directory name from a file
  //filename should point to gchar[PATH_MAX]

  g_snprintf (filename,PATH_MAX,"%s%s",(tmp=g_path_get_dirname (filename)),G_DIR_SEPARATOR_S);
  g_free(tmp);

  if (!strcmp(filename,"//")) {
    memset(filename+1,0,1);
    return;
  }

  if (!strncmp(filename,"./",2)) {
    gchar *tmp1=g_get_current_dir(),*tmp=g_build_filename(tmp1,filename+2,NULL);
    g_free(tmp1);
    g_snprintf(filename,PATH_MAX,"%s",tmp);
    g_free(tmp);
  }
}


gchar *get_dir(const gchar *filename) {
  gchar tmp[PATH_MAX];
  g_snprintf(tmp,PATH_MAX,"%s",filename);
  get_dirname(tmp);
  return g_strdup(tmp);
}


void get_basename(gchar *filename) {
  // get basename from a file
  // (filename without directory)
  //filename should point to gchar[PATH_MAX]
  gchar *tmp=g_path_get_basename(filename);
  g_snprintf (filename,PATH_MAX,"%s",tmp);
  g_free(tmp);
}

void get_filename(gchar *filename, gboolean strip_dir) {
  // get filename (part without extension) of a file
  //filename should point to gchar[PATH_MAX]
  gchar **array;
  if (strip_dir) get_basename(filename);
  array=g_strsplit(filename,".",-1);
  g_snprintf(filename,PATH_MAX,"%s",array[0]);
  g_strfreev(array);
}


gchar *get_extension(const gchar *filename) {
  gchar *tmp=g_path_get_basename(filename);
  gint ntok=get_token_count((gchar *)filename,'.');
  gchar **array=g_strsplit(tmp,".",-1);
  gchar *ret=g_strdup(array[ntok-1]);
  g_strfreev(array);
  g_free(tmp);
  return ret;
}


gchar *ensure_extension(const gchar *fname, const gchar *ext) {
  if (!strcmp(fname+strlen(fname)-strlen(ext),ext)) return g_strdup(fname);
  return g_strconcat(fname,ext,NULL);
}


gboolean ensure_isdir(gchar *fname) {
  // ensure dirname ends in a single dir separator
  // fname should be gchar[PATH_MAX]

  // returns TRUE if fname was altered

  size_t slen=strlen(fname);
  size_t offs=slen-1;
  gchar *tmp;

  while (offs>=0&&!strcmp(fname+offs,G_DIR_SEPARATOR_S)) offs--;
  if (offs==slen-2) return FALSE;
  memset(fname+offs+1,0,1);
  tmp=g_strdup_printf("%s%s",fname,G_DIR_SEPARATOR_S);
  g_snprintf(fname,PATH_MAX,"%s",tmp);
  g_free(tmp);
  return TRUE;
}


void get_location(const gchar *exe, gchar *val, gint maxlen) {
  // find location of "exe" in path
  // sets it in val which is a char array of maxlen bytes

  gchar *loc;
  if ((loc=g_find_program_in_path (exe))!=NULL) {
    g_snprintf (val,maxlen,"%s",loc);
    g_free (loc);
  }
  else {
    memset (val,0,1);
  }
}


gchar *repl_tmpdir(const gchar *entry, gboolean fwd) {
  // replace prefs->tmpdir with string tmpdir or vice-versa. This allows us to relocate tmpdir if necessary.
  // used for layout.map file
  // return value should be g_free()'d

  // fwd TRUE replaces "/tmp/foo" with "tmpdir"
  // fwd FALSE replaces "tmpdir" with "/tmp/foo"


  gchar *string=g_strdup(entry);;

  if (fwd) {
    if (!strncmp(entry,prefs->tmpdir,strlen(prefs->tmpdir))) {
      g_free(string);
      string=g_strdup_printf("tmpdir%s",entry+strlen(prefs->tmpdir));
    }
  }
  else {
    if (!strncmp(entry,"tmpdir",6)) {
      g_free(string);
      string=g_build_filename(prefs->tmpdir,entry+6,NULL);
    }
  }
  return string;
}


void remove_layout_files(GList *map) {
  // removes a GList of layouts from the set layout map

  // removes from: - global layouts map
  //               - disk
  //               - clip layout maps

  // called after, for example: a clip is removed or altered and the user opts to remove all associated layouts

  gchar *com,*msg;
  gchar *fname,*fdir;
  gchar **array;
  GList *lmap,*lmap_next,*cmap,*cmap_next,*map_next;
  size_t maplen;
  int i;
  gboolean is_current;

  while (map!=NULL) {
    map_next=map->next;
    if (map->data!=NULL) {
      if (!strcmp((gchar *)map->data,mainw->cl_string)) {
	is_current=TRUE;
	fname=g_strdup(mainw->cl_string);
      }
      else {
	is_current=FALSE;
	maplen=strlen((gchar *)map->data);
	
	// remove from mainw->current_layouts_map
	cmap=mainw->current_layouts_map;
	while (cmap!=NULL) {
	  cmap_next=cmap->next;
	  if (!strcmp((gchar *)cmap->data,(gchar *)map->data)) {
	    mainw->current_layouts_map=g_list_remove_link(mainw->current_layouts_map,cmap);
	    break;
	  }
	  cmap=cmap_next;
	}

	array=g_strsplit((gchar *)map->data,"|",-1);
	fname=repl_tmpdir(array[0],FALSE);
	g_strfreev(array);
      }

      // fname should now hold the layout name on disk

      msg=g_strdup_printf(_("Removing layout %s\n"),fname);
      d_print(msg);
      g_free(msg);

      if (!is_current) {
	com=g_strdup_printf("/bin/rm \"%s\" 2>/dev/null",fname);
	lives_system(com,TRUE);
	g_free(com);
	

	// if no more layouts in parent dir, we can delete dir

	// ensure that parent dir is below our own working dir
	if (!strncmp(fname,prefs->tmpdir,strlen(prefs->tmpdir))) {
	  // is in tmpdir, safe to remove parents

	  gchar *protect_file=g_build_filename(prefs->tmpdir,"noremove",NULL);

	  mainw->com_failed=FALSE;
	  // touch a file in tpmdir, so we cannot remove tmpdir itself
	  com=g_strdup_printf("/bin/touch \"%s\" >/dev/null 2>&1",protect_file);
	  lives_system(com,FALSE);
	  g_free(com);
	  
	  if (!mainw->com_failed) {
	    // ok, the "touch" worked
	    // now we call rmdir -p : remove directory + any empty parents
	    fdir=g_path_get_dirname (fname);
	    com=g_strdup_printf("/bin/rmdir -p \"%s\" 2>/dev/null",fdir);
	    lives_system(com,TRUE);
	    g_free(com);
	    g_free(fdir);
	  }

	  // remove the file we touched to clean up
	  unlink(protect_file);
	  g_free(protect_file);
	}
	

	// remove from mainw->files[]->layout_map
	for (i=1;i<=MAX_FILES;i++) {
	  if (mainw->files[i]!=NULL) {
	    if (mainw->files[i]->layout_map!=NULL) {
	      lmap=mainw->files[i]->layout_map;
	      while (lmap!=NULL) {
		lmap_next=lmap->next;
		if (!strncmp((gchar *)lmap->data,(gchar *)map->data,maplen)) {
		  // remove matching entry
		  if (lmap->prev!=NULL) lmap->prev->next=lmap_next;
		  else mainw->files[i]->layout_map=lmap_next;
		  if (lmap->next!=NULL) lmap_next->prev=lmap->prev;
		  lmap->next=lmap->prev=NULL;
		  g_free(lmap->data);
		  g_list_free(lmap);
		}
		lmap=lmap_next;
	      }
	    }
	  }
	}
      }
      else {
	// asked to remove the currently loaded layout

	if (mainw->stored_event_list!=NULL||mainw->sl_undo_mem!=NULL) {
	  // we are in CE mode, so event_list is in storage
	  stored_event_list_free_all(TRUE);
	}
	// in mt mode we need to do more
	else remove_current_from_affected_layouts(mainw->multitrack);

	// and we dont want to try reloading this next time
	prefs->ar_layout=FALSE;
	set_pref("ar_layout","");
	memset(prefs->ar_layout_name,0,1);
      }
      g_free(fname);
    }
    map=map_next;
  }

  // save updated layout.map
  save_layout_map(NULL,NULL,NULL,NULL);
  
}



void get_play_times(void) {
  // update the on-screen timer bars,
  // and if we are not playing,
  // get play times for video, audio channels, and total (longest) time
  gchar *tmpstr;
  gdouble offset=0;
  gdouble offset_left=0;
  gdouble offset_right=0;
  gdouble allocwidth;
  gdouble allocheight;

  if (mainw->current_file==-1||mainw->foreign||cfile==NULL||mainw->multitrack!=NULL) return;

  if (mainw->playing_file==-1) {
    get_total_time (cfile);
  }

  if (!mainw->is_ready) return;

  if (mainw->laudio_drawable==NULL||mainw->raudio_drawable==NULL) return;


  // draw timer bars
  allocwidth=mainw->video_draw->allocation.width;
  allocheight=mainw->video_draw->allocation.height;

  
  if (mainw->laudio_drawable!=NULL) {
    gdk_draw_rectangle (mainw->laudio_drawable,
			mainw->laudio_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->laudio_draw)],
			TRUE,
			0, 0,
			allocwidth,
			allocheight
			);
  }
  
  
  if (mainw->raudio_drawable!=NULL) {
    gdk_draw_rectangle (mainw->raudio_drawable,
			mainw->raudio_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->raudio_draw)],
			TRUE,
			0, 0,
			allocwidth,
			allocheight
			);
  }

  if (mainw->video_drawable!=NULL) {
    gdk_draw_rectangle (mainw->video_drawable,
			mainw->video_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->video_draw)],
			TRUE,
			0, 0,
			allocwidth,
			allocheight
			);
  }

  if (cfile->frames>0) {
    offset_left=(cfile->start-1)/cfile->fps/cfile->total_time*allocwidth;
    offset_right=(cfile->end)/cfile->fps/cfile->total_time*allocwidth;

    
    if (mainw->video_drawable!=NULL) {
      gdk_draw_rectangle (mainw->video_drawable,
			  mainw->video_draw->style->black_gc,
			  TRUE,
			  0, 0,
			  cfile->video_time/cfile->total_time*allocwidth-1,
			  prefs->bar_height);
      
      gdk_draw_rectangle (mainw->video_drawable,
			  mainw->video_draw->style->white_gc,
			  TRUE,
			  offset_left, 0,
			  offset_right-offset_left,
			  prefs->bar_height);
    }
  }
  if (cfile->achans>0) {
    if (mainw->playing_file>-1) {
      offset_left=((mainw->playing_sel&&(prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE))?
		   cfile->start-1.:mainw->audio_start-1.)/cfile->fps/cfile->total_time*allocwidth;
      if (mainw->audio_end&&!mainw->loop) {
	offset_right=(((prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE))?
		      cfile->end:mainw->audio_end)/cfile->fps/cfile->total_time*allocwidth;
      }
      else {
	offset_right=allocwidth*cfile->laudio_time/cfile->total_time;
      }
    }
    
    
    if (mainw->laudio_drawable!=NULL) {
      gdk_draw_rectangle (mainw->laudio_drawable,
			  mainw->laudio_draw->style->black_gc,
			  TRUE,
			  0, 0,
			  cfile->laudio_time/cfile->total_time*allocwidth-1,
			  prefs->bar_height);
    
    
      gdk_draw_rectangle (mainw->laudio_drawable,
			  mainw->laudio_draw->style->white_gc,
			  TRUE,
			  offset_left, 0,
			  offset_right-offset_left,
			  prefs->bar_height);
  
    
      gdk_draw_rectangle (mainw->laudio_drawable,
			  mainw->laudio_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->laudio_draw)],
			  TRUE,
			  cfile->laudio_time/cfile->total_time*allocwidth, 0,
			  allocwidth-(cfile->laudio_time/cfile->total_time*allocwidth),
			  prefs->bar_height);
    }
    if (cfile->achans>1) {
      if (mainw->raudio_drawable!=NULL) {
	gdk_draw_rectangle (mainw->raudio_drawable,
			    mainw->raudio_draw->style->black_gc,
			    TRUE,
			    0, 0,
			    cfile->raudio_time/cfile->total_time*allocwidth-1,
			    prefs->bar_height);
      
	gdk_draw_rectangle (mainw->raudio_drawable,
			    mainw->raudio_draw->style->white_gc,
			    TRUE,
			    offset_left, 0,
			    offset_right-offset_left,
			    prefs->bar_height);
      
      
	gdk_draw_rectangle (mainw->raudio_drawable,
			    mainw->raudio_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->raudio_draw)],
			    TRUE,
			    cfile->raudio_time/cfile->total_time*allocwidth, 0,
			    allocwidth-(cfile->raudio_time/cfile->total_time*allocwidth),
			    prefs->bar_height);
      }
    }
  }

  // playback cursors
  if (mainw->playing_file>-1) {
    if (cfile->frames>0) {
      offset=(mainw->actual_frame-.5)/cfile->fps;
      offset/=cfile->total_time/allocwidth;
      if (mainw->video_drawable!=NULL) {
	if (offset>=offset_left&&offset<=offset_right) {
	  gdk_draw_line (mainw->video_drawable,
			 mainw->video_draw->style->black_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->video_drawable,
			 mainw->video_draw->style->white_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
      
	if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	  gdk_draw_line (mainw->video_drawable,
			 mainw->video_draw->style->black_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight-prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->video_drawable,
			 mainw->video_draw->style->white_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight-prefs->bar_height);
	}
      }
      GTK_RULER (mainw->hruler)->position=offset*cfile->total_time/allocwidth;
      gtk_widget_queue_draw (mainw->hruler);
    }
    if (cfile->achans>0&&cfile->is_loaded) {
      if ((prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE)&&
	  (mainw->event_list==NULL||!mainw->preview)) {
#ifdef ENABLE_JACK
	if (mainw->jackd!=NULL&&prefs->audio_player==AUD_PLAYER_JACK) {
	  offset=allocwidth*((gdouble)mainw->jackd->seek_pos/cfile->arate/cfile->achans/
			     cfile->asampsize*8)/cfile->total_time;
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	if (mainw->pulsed!=NULL&&prefs->audio_player==AUD_PLAYER_PULSE) {
	  offset=allocwidth*((gdouble)mainw->pulsed->seek_pos/cfile->arate/cfile->achans/
			     cfile->asampsize*8)/cfile->total_time;
	}
#endif
      }
      else offset=allocwidth*(mainw->aframeno-.5)/cfile->fps/cfile->total_time;
      if (mainw->laudio_drawable!=NULL) {
	if ((offset>=offset_left&&offset<=offset_right)||mainw->loop) {
	  gdk_draw_line (mainw->laudio_drawable,
			 mainw->laudio_draw->style->black_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->laudio_drawable,
			 mainw->laudio_draw->style->white_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
      
	if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	  gdk_draw_line (mainw->laudio_drawable,
			 mainw->laudio_draw->style->black_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight-prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->laudio_drawable,
			 mainw->laudio_draw->style->white_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight-prefs->bar_height);
	}
      }

      if (cfile->achans>1) {
	if (mainw->raudio_drawable!=NULL) {
	  if ((offset>=offset_left&&offset<=offset_right)||mainw->loop) {
	    gdk_draw_line (mainw->raudio_drawable,
			   mainw->raudio_draw->style->black_gc,
			   offset, 0,
			   offset,
			   prefs->bar_height);
	  }
	  else {
	    gdk_draw_line (mainw->raudio_drawable,
			   mainw->raudio_draw->style->white_gc,
			   offset, 0,
			   offset,
			   prefs->bar_height);
	  }
	  
	  
	  if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	    gdk_draw_line (mainw->raudio_drawable,
			   mainw->raudio_draw->style->black_gc,
			   offset, prefs->bar_height,
			   offset,
			   allocheight-prefs->bar_height);
	  }
	  else {
	    gdk_draw_line (mainw->raudio_drawable,
			   mainw->raudio_draw->style->white_gc,
			   offset, prefs->bar_height,
			   offset,
			   allocheight-prefs->bar_height);
	  }
	}
      }
      if (cfile->frames==0) {
	GTK_RULER (mainw->hruler)->position=offset*cfile->total_time/allocwidth;
	gtk_widget_queue_draw (mainw->hruler);
      }
    }
  }
  
  if (mainw->playing_file==-1||(mainw->switch_during_pb&&!mainw->faded)) {
    if (cfile->total_time>0.) {
      // set the range of the timeline
      if (!cfile->opening_loc) {
	gtk_widget_show (mainw->hruler);
      }
      gtk_widget_show (mainw->eventbox5);
      gtk_widget_show (mainw->video_draw);
      gtk_widget_show (mainw->laudio_draw);
      gtk_widget_show (mainw->raudio_draw);

      GTK_RULER (mainw->hruler)->upper=cfile->total_time;
      gtk_widget_queue_draw(mainw->hruler);

      draw_little_bars(cfile->pointer_time);
      if (mainw->playing_file==-1&&prefs->sepwin_type==1&&mainw->sep_win&&cfile->is_loaded) {
	if (mainw->preview_box==NULL) {
	  // create the preview box that shows frames
	  make_preview_box();
	}
	// and add it the play window
	if (mainw->preview_box->parent==NULL&&(cfile->clip_type==CLIP_TYPE_DISK||
					       cfile->clip_type==CLIP_TYPE_FILE)&&!mainw->is_rendering) {
	  gtk_widget_queue_draw(mainw->play_window);
	  gtk_container_add (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
	}
	load_preview_image(FALSE);
      }
    }
    else {
      gtk_widget_hide (mainw->hruler);
      gtk_widget_hide (mainw->eventbox5);
    }

    if (cfile->opening_loc||(cfile->frames==123456789&&cfile->opening)) {
      tmpstr=g_strdup(_ ("Video [opening...]"));
    }
    else {
      if (cfile->video_time>0.) {
	tmpstr=g_strdup_printf(_ ("Video [%.2f sec]"),cfile->video_time);
      }
      else {
	if (cfile->video_time<=0.&&cfile->frames>0) {
	  tmpstr=g_strdup (_ ("(Undefined)"));
	}
	else {
	  tmpstr=g_strdup (_ ("(No video)"));
	}
      }
    }
    gtk_label_set_text(GTK_LABEL(mainw->vidbar),tmpstr);
    g_free(tmpstr);
    if (cfile->achans==0) {
      tmpstr=g_strdup (_ ("(No audio)"));
    }
    else {
      if (cfile->opening_audio) {
	if (cfile->achans==1) {
	  tmpstr=g_strdup (_ ("Mono  [opening...]"));
	}
	else {
	  tmpstr=g_strdup (_ ("Left Audio [opening...]"));
	}
      }
      else {
	if (cfile->achans==1) {
	  tmpstr=g_strdup_printf(_ ("Mono [%.2f sec]"),cfile->laudio_time);
	}
	else {
	  tmpstr=g_strdup_printf(_ ("Left Audio [%.2f sec]"),cfile->laudio_time);
	}
      }
    }
    gtk_label_set_text(GTK_LABEL(mainw->laudbar),tmpstr);
    g_free(tmpstr);
    if (cfile->achans>1) {
      if (cfile->opening_audio) {
	tmpstr=g_strdup (_ ("Right Audio [opening...]"));
      }
      else {
	tmpstr=g_strdup_printf(_ ("Right Audio [%.2f sec]"),cfile->raudio_time);
      }
      gtk_label_set_text(GTK_LABEL(mainw->raudbar),tmpstr);
      gtk_widget_show (mainw->raudbar);
      g_free(tmpstr);
    }
    else {
      gtk_widget_hide (mainw->raudbar);
    }
  }
  else {
    gdouble ptrtime=(mainw->actual_frame-.5)/cfile->fps;
    if (ptrtime<0.) ptrtime=0.;
    draw_little_bars(ptrtime);
  }
  gtk_widget_queue_draw(mainw->video_draw);
  gtk_widget_queue_draw(mainw->laudio_draw);
  gtk_widget_queue_draw(mainw->raudio_draw);
  gtk_widget_queue_draw(mainw->vidbar);
  gtk_widget_queue_draw(mainw->hruler);
}
    

void draw_little_bars (gdouble ptrtime) {
  //draw the vertical player bars
  gdouble allocheight=mainw->video_draw->allocation.height-prefs->bar_height;
  gdouble offset=ptrtime/cfile->total_time*mainw->vidbar->allocation.width;
  gint frame;

  if (!prefs->show_gui) return;

  if (!(frame=calc_frame_from_time(mainw->current_file,ptrtime)))
   frame=cfile->frames;

  if (cfile->frames>0) {
    if (mainw->video_drawable!=NULL) {
      if (frame>=cfile->start&&frame<=cfile->end) {
	gdk_draw_line (mainw->video_drawable,
		       mainw->video_draw->style->black_gc,
		       offset, 0,
		       offset,
		       prefs->bar_height);
      }
      else {
	gdk_draw_line (mainw->video_drawable,
		       mainw->video_draw->style->white_gc,
		       offset, 0,
		       offset,
		       prefs->bar_height);
      }
      
      if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	gdk_draw_line (mainw->video_drawable,
		       mainw->video_draw->style->black_gc,
		       offset, prefs->bar_height,
		       offset,
		       allocheight);
      }
      else {
	gdk_draw_line (mainw->video_drawable,
		       mainw->video_draw->style->white_gc,
		       offset, prefs->bar_height,
		       offset,
		       allocheight);
      }
    }
  }

  if (mainw->playing_file>-1) return;

  if (cfile->achans>0) {
    if (mainw->laudio_drawable!=NULL) {
      if ((frame>=cfile->start&&frame<=cfile->end)||mainw->loop) {
	gdk_draw_line (mainw->laudio_drawable,
		       mainw->laudio_draw->style->black_gc,
		       offset, 0,
		       offset,
		       prefs->bar_height);
      }
      else {
	gdk_draw_line (mainw->laudio_drawable,
		       mainw->laudio_draw->style->white_gc,
		       offset, 0,
		       offset,
		       prefs->bar_height);
      }
      
      if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	gdk_draw_line (mainw->laudio_drawable,
		       mainw->laudio_draw->style->black_gc,
		       offset, prefs->bar_height,
		       offset,
		       allocheight);
      }
      else {
	gdk_draw_line (mainw->laudio_drawable,
		       mainw->laudio_draw->style->white_gc,
		       offset, prefs->bar_height,
		       offset,
		       allocheight);
      }
    }
    if (cfile->achans>1) {
      if (mainw->raudio_drawable!=NULL) {
	if ((frame>=cfile->start&&frame<=cfile->end)||mainw->loop) {
	  gdk_draw_line (mainw->raudio_drawable,
			 mainw->raudio_draw->style->black_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->raudio_drawable,
			 mainw->raudio_draw->style->white_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
	
	
	if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	  gdk_draw_line (mainw->raudio_drawable,
			 mainw->raudio_draw->style->black_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight);
	}
	else {
	  gdk_draw_line (mainw->raudio_drawable,
			 mainw->raudio_draw->style->white_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight);
	}
      }
    }
  }
  threaded_dialog_spin();
}




void get_total_time (file *file) {
  // get times (video, left and right audio)

  file->laudio_time=file->raudio_time=file->video_time=file->total_time=0.;

  if (file->opening&&file->frames!=123456789) {
    if (file->frames*file->fps>0) {
      file->video_time=file->total_time=file->frames/file->fps;
    }
    return;
  }

  if (file->fps>0.) {
    file->total_time=file->video_time=file->frames/file->fps;
  }

  if (file->asampsize*file->arate*file->achans) {
    file->laudio_time=(gdouble)(file->afilesize/(file->asampsize>>3)/file->achans)/(gdouble)file->arate;
    if (file->achans>1) {
      file->raudio_time=file->laudio_time;
    }
  }

  if (file->laudio_time>file->total_time) file->total_time=file->laudio_time;
  if (file->raudio_time>file->total_time) file->total_time=file->raudio_time;

  if (file->laudio_time+file->raudio_time==0.&&!file->opening) {
    file->achans=file->afilesize=file->asampsize=file->arate=file->arps=0;
  }
}



void 
find_when_to_stop (void) {
  // work out when to stop playing
  //
  // ---------------
  //        no loop              loop to fit                 loop cont
  //        -------              -----------                 ---------
  // a>v    stop on video end    stop on audio end           no stop
  // v>a    stop on video end    stop on video end           no stop
  // generator start - not playing : stop on vid_end, unless pure audio;
  if (cfile->clip_type==CLIP_TYPE_GENERATOR||mainw->aud_rec_fd!=-1) mainw->whentostop=STOP_ON_VID_END;
  else if (mainw->multitrack!=NULL&&cfile->frames>0) mainw->whentostop=STOP_ON_VID_END;
  else if (cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE) mainw->whentostop=NEVER_STOP;
  else if (cfile->opening_only_audio) mainw->whentostop=STOP_ON_AUD_END;
  else if (cfile->opening_audio) mainw->whentostop=STOP_ON_VID_END;
  else if (mainw->loop_cont&&!mainw->preview) mainw->whentostop=NEVER_STOP;
  else if (cfile->frames==0||(mainw->loop&&cfile->achans>0&&!mainw->is_rendering&&(mainw->audio_end/cfile->fps)
			      <MAX (cfile->laudio_time,cfile->raudio_time))) mainw->whentostop=STOP_ON_AUD_END;
  else mainw->whentostop=STOP_ON_VID_END; // tada...
}


#define ASPECT_ALLOWANCE 0.005


void 
minimise_aspect_delta (gdouble aspect,gint hblock,gint vblock,gint hsize,gint vsize,gint *width,gint *height) {
  // we will use trigonometry to calculate the smallest difference between a given
  // aspect ratio and the actual frame size. If the delta is smaller than current 
  // we set the height and width
  gint cw=width[0];
  gint ch=height[0];

  gint real_width,real_height;
  gulong delta,current_delta;

  // minimise d[(x-x1)^2 + (y-y1)^2]/d[x1], to get approximate values
  gint calc_width=(gint)((vsize+aspect*hsize)*aspect/(aspect*aspect+1.));

  int i;

  current_delta=(hsize-cw)*(hsize-cw)+(vsize-ch)*(vsize-ch);

#ifdef DEBUG_ASPECT
  g_printerr ("aspect %.8f : width %d height %d is best fit\n",aspect,calc_width,(gint)(calc_width/aspect));
#endif
  // use the block size to find the nearest allowed size
  for (i=-1;i<2;i++) {
    real_width=(gint)(calc_width/hblock+i)*hblock;
    real_height=(gint)(real_width/aspect/vblock+.5)*vblock;
    delta=(hsize-real_width)*(hsize-real_width)+(vsize-real_height)*(vsize-real_height);


    if (real_width%hblock!=0||real_height%vblock!=0||ABS((gdouble)real_width/(gdouble)real_height-aspect)>ASPECT_ALLOWANCE) {
      // encoders can be fussy, so we need to fit both aspect ratio and blocksize      
      while (1) {
	real_width=((int)(real_width/hblock)+1)*hblock;
	real_height=(int)((gdouble)real_width/aspect+.5);
	
	if (real_height%vblock==0) break;
	
	real_height=((int)(real_height/vblock)+1)*vblock;
	real_width=(int)((gdouble)real_height*aspect+.5);
	
	if (real_width%hblock==0) break;
	
      }
    }

#ifdef DEBUG_ASPECT
    g_printerr ("block quantise to %d x %d\n",real_width,real_height);
#endif
    if (delta<current_delta) {
#ifdef DEBUG_ASPECT
      g_printerr ("is better fit\n");
#endif
      current_delta=delta;
      width[0]=real_width;
      height[0]=real_height;
    }
  }
}

void 
zero_spinbuttons (void) {
  g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
  gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_start),0.,0.);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),0.);
  g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);
  g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
  gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_end),0.,0.);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),0.);
  g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);
}


void hide_cursor(GdkWindow *window) {
  //make the cursor invisible in playback windows

#if GTK_CHECK_VERSION(3,0,0)
cairo_surface_t *s;
GdkPixbuf *pixbuf;

 if (hidden_cursor==NULL) {
   s = cairo_image_surface_create (CAIRO_FORMAT_A1, 1, 1);
   pixbuf = gdk_pixbuf_get_from_surface (s,
					 0, 0,
					 1, 1);
   
   cairo_surface_destroy (s);
   
   hidden_cursor = gdk_cursor_new_from_pixbuf (gdk_display_get_default(), pixbuf, 0, 0);
   
   g_object_unref (pixbuf);
 }
#else
  char cursor_bits[] = {0x00};
  char cursormask_bits[] = {0x00};
  GdkPixmap *source, *mask;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0, 0, 0 };

  if (hidden_cursor==NULL) {
    source = gdk_bitmap_create_from_data (NULL, cursor_bits,
					  1, 1);
    mask = gdk_bitmap_create_from_data (NULL, cursormask_bits,
					1, 1);
    hidden_cursor = gdk_cursor_new_from_pixmap (source, mask, &fg, &bg, 0, 0);
    gdk_pixmap_unref (source);
    gdk_pixmap_unref (mask);
  }
#endif 

 gdk_window_set_cursor (window, hidden_cursor);
}


void 
unhide_cursor(GdkWindow *window) {
  gdk_window_set_cursor(window,NULL);
}



LIVES_INLINE void toggle_button_toggle (GtkToggleButton *tbutton) {
  if (gtk_toggle_button_get_active(tbutton)) gtk_toggle_button_set_active(tbutton,FALSE);
  else gtk_toggle_button_set_active(tbutton,FALSE);
}




gboolean switch_aud_to_jack(void) {
#ifdef ENABLE_JACK
  if (mainw->is_ready) {
    lives_jack_init();
    if (mainw->jackd==NULL) {
      jack_audio_init();
      jack_audio_read_init();
      mainw->jackd=jack_get_driver(0,TRUE);
      if (jack_open_device(mainw->jackd)) {
	mainw->jackd=NULL;
	return FALSE;
      }
      mainw->jackd->whentostop=&mainw->whentostop;
      mainw->jackd->cancelled=&mainw->cancelled;
      mainw->jackd->in_use=FALSE;
      mainw->jackd->play_when_stopped=(prefs->jack_opts&JACK_OPTS_NOPLAY_WHEN_PAUSED)?FALSE:TRUE;
      jack_driver_activate(mainw->jackd);
    }
    mainw->aplayer_broken=FALSE;
    gtk_widget_show(mainw->vol_toolitem);
    gtk_widget_show(mainw->vol_label);
    gtk_widget_show (mainw->recaudio_submenu);

    if (mainw->vpp!=NULL&&mainw->vpp->get_audio_fmts!=NULL) 
      mainw->vpp->audio_codec=get_best_audio(mainw->vpp);

#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed_read!=NULL) {
      pulse_close_client(mainw->pulsed_read);
      mainw->pulsed_read=NULL;
    }
    
    if (mainw->pulsed!=NULL) {
      pulse_close_client(mainw->pulsed);
      mainw->pulsed=NULL;
      pulse_shutdown();
    }
#endif
  }
  prefs->audio_player=AUD_PLAYER_JACK;
  set_pref("audio_player","jack");
  g_snprintf(prefs->aplayer,512,"%s","jack");

  if (mainw->is_ready&&mainw->vpp!=NULL&&mainw->vpp->get_audio_fmts!=NULL) 
    mainw->vpp->audio_codec=get_best_audio(mainw->vpp);

  return TRUE;
#endif
  return FALSE;
}



gboolean switch_aud_to_pulse(void) {
#ifdef HAVE_PULSE_AUDIO
  gboolean retval;

  if (mainw->is_ready) {
    if ((retval=lives_pulse_init(-1))) {
      if (mainw->pulsed==NULL) {
	pulse_audio_init();
	pulse_audio_read_init();
	mainw->pulsed=pulse_get_driver(TRUE);
	mainw->pulsed->whentostop=&mainw->whentostop;
	mainw->pulsed->cancelled=&mainw->cancelled;
	mainw->pulsed->in_use=FALSE;
	pulse_driver_activate(mainw->pulsed);
      }
      mainw->aplayer_broken=FALSE;
      gtk_widget_show(mainw->vol_toolitem);
      gtk_widget_show(mainw->vol_label);
      gtk_widget_show (mainw->recaudio_submenu);

      prefs->audio_player=AUD_PLAYER_PULSE;
      set_pref("audio_player","pulse");
      g_snprintf(prefs->aplayer,512,"%s","pulse");

      if (mainw->vpp!=NULL&&mainw->vpp->get_audio_fmts!=NULL) 
	mainw->vpp->audio_codec=get_best_audio(mainw->vpp);

    }
    
#ifdef ENABLE_JACK
    if (mainw->jackd_read!=NULL) {
      jack_close_device(mainw->jackd_read);
      mainw->jackd_read=NULL;
    }

    if (mainw->jackd!=NULL) {
      jack_close_device(mainw->jackd);
      mainw->jackd=NULL;
    }
#endif
    return retval;
  }

#endif
  return FALSE;
}



void switch_aud_to_sox(gboolean set_in_prefs) {
  prefs->audio_player=AUD_PLAYER_SOX;
  get_pref_default("sox_command",prefs->audio_play_command,256);
  if (set_in_prefs) set_pref("audio_player","sox");
  g_snprintf(prefs->aplayer,512,"%s","sox");
  set_pref("audio_play_command",prefs->audio_play_command);
  if (mainw->is_ready) {
    gtk_widget_hide(mainw->vol_toolitem);
    gtk_widget_hide(mainw->vol_label);
    gtk_widget_hide (mainw->recaudio_submenu);
    
    if (mainw->vpp!=NULL&&mainw->vpp->get_audio_fmts!=NULL) 
      mainw->vpp->audio_codec=get_best_audio(mainw->vpp);
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read!=NULL) {
    jack_close_device(mainw->jackd_read);
    mainw->jackd_read=NULL;
  }

  if (mainw->jackd!=NULL) {
    jack_close_device(mainw->jackd);
    mainw->jackd=NULL;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read!=NULL) {
    pulse_close_client(mainw->pulsed_read);
    mainw->pulsed_read=NULL;
  }

  if (mainw->pulsed!=NULL) {
    pulse_close_client(mainw->pulsed);
    mainw->pulsed=NULL;
    pulse_shutdown();
  }
#endif

}



void
switch_aud_to_mplayer(gboolean set_in_prefs) {
  int i;
  for (i=1;i<MAX_FILES;i++) {
    if (mainw->files[i]!=NULL) {
      if (i!=mainw->current_file&&mainw->files[i]->opening) {
	do_error_dialog(_ ("LiVES cannot switch to mplayer whilst clips are loading."));
	return;
      }
    }
  }
  
  prefs->audio_player=AUD_PLAYER_MPLAYER;
  get_pref_default("mplayer_audio_command",prefs->audio_play_command,256);
  if (set_in_prefs) set_pref("audio_player","mplayer");
  g_snprintf(prefs->aplayer,512,"%s","mplayer");
  set_pref("audio_play_command",prefs->audio_play_command);
  if (mainw->is_ready) {
    gtk_widget_hide(mainw->vol_toolitem);
    gtk_widget_hide(mainw->vol_label);
    gtk_widget_hide (mainw->recaudio_submenu);

    if (mainw->vpp!=NULL&&mainw->vpp->get_audio_fmts!=NULL) 
      mainw->vpp->audio_codec=get_best_audio(mainw->vpp);
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read!=NULL) {
    jack_close_device(mainw->jackd_read);
    mainw->jackd_read=NULL;
  }

  if (mainw->jackd!=NULL) {
    jack_close_device(mainw->jackd);
    mainw->jackd=NULL;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read!=NULL) {
    pulse_close_client(mainw->pulsed_read);
    mainw->pulsed_read=NULL;
  }

  if (mainw->pulsed!=NULL) {
    pulse_close_client(mainw->pulsed);
    mainw->pulsed=NULL;
    pulse_shutdown();
  }
#endif

}



void
prepare_to_play_foreign(void) {
  // here we are going to 'play' a captured external window
  gint new_file=mainw->first_free_file;
  GdkVisual *vissi;

  // create a new 'file' to play into
  if (!get_new_handle(new_file,NULL)) {
    return;
  }

  mainw->current_file=new_file;

  if (mainw->rec_achans>0) {
    cfile->arate=cfile->arps=mainw->rec_arate;
    cfile->achans=mainw->rec_achans;
    cfile->asampsize=mainw->rec_asamps;
    cfile->signed_endian=mainw->rec_signed_endian;
#ifdef HAVE_PULSE_AUDIO
    if (mainw->rec_achans>0&&prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed_read==NULL) {
      lives_pulse_init(0);
      pulse_audio_read_init();
      pulse_rec_audio_to_clip(mainw->current_file,-1,RECA_WINDOW_GRAB);
    }
#endif
#ifdef ENABLE_JACK
    if (mainw->rec_achans>0&&prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd_read==NULL) 
      jack_rec_audio_to_clip(mainw->current_file,-1,RECA_WINDOW_GRAB);
#endif
  }

  cfile->hsize=mainw->foreign_width/2+1;
  cfile->vsize=mainw->foreign_height/2+3;

  cfile->fps=cfile->pb_fps=mainw->rec_fps;

  resize(-2);

  gtk_widget_show (mainw->playframe);
  gtk_widget_show (mainw->playarea);
  while (g_main_context_iteration(NULL,FALSE));

  // size must be exact, must not be larger than play window or we end up with nothing
  mainw->pwidth=mainw->playframe->allocation.width-H_RESIZE_ADJUST+2;
  mainw->pheight=mainw->playframe->allocation.height-V_RESIZE_ADJUST+2;

  cfile->hsize=mainw->pwidth;
  cfile->vsize=mainw->pheight;


  mainw->foreign_window=gdk_window_foreign_new(mainw->foreign_id);
  gdk_window_set_keep_above(mainw->foreign_window,TRUE);

  // seems not to work
  //gdk_window_reparent(mainw->foreign_window, mainw->playarea->window, 0, 0);
  //while (g_main_context_iteration(NULL,FALSE));
  ////////////////////////
  
  //vissi=gdk_x11_screen_lookup_visual(gdk_screen_get_default(),hextodec(mainw->foreign_visual));

  vissi=gdk_visual_get_best_with_depth (mainw->foreign_bpp);

#if GTK_CHECK_VERSION(3,0,0)
  mainw->foreign_cmap=NULL;
#else
#ifdef USE_X11
  mainw->foreign_cmap=gdk_x11_colormap_foreign_new(vissi, 
						   gdk_x11_colormap_get_xcolormap(gdk_colormap_new(vissi,TRUE)));

#endif
#endif

  mainw->play_start=1;
  if (mainw->rec_vid_frames==-1) mainw->play_end=INT_MAX;
  else mainw->play_end=mainw->rec_vid_frames;

  mainw->rec_samples=-1;

  omute=mainw->mute;
  osepwin=mainw->sep_win;
  ofs=mainw->fs;
  ofaded=mainw->faded;
  odouble=mainw->double_size;

  mainw->mute=TRUE;
  mainw->sep_win=FALSE;
  mainw->fs=FALSE;
  mainw->faded=TRUE;
  mainw->double_size=FALSE;

  gtk_widget_hide(mainw->t_double);
  gtk_widget_hide(mainw->t_bckground);
  gtk_widget_hide(mainw->t_sepwin);
  gtk_widget_hide(mainw->t_infobutton);
}


gboolean
after_foreign_play(void) {
  // read details from capture file
  int capture_fd;
  gchar *capfile=g_strdup_printf("%s/.capture.%d",prefs->tmpdir,getpid());
  gchar capbuf[256];
  ssize_t length;
  gint new_file=-1;
  gint new_frames=0;
  gint old_file=mainw->current_file;

  gchar *com;
  gchar **array;
  gchar file_name[PATH_MAX];

  // assume for now we only get one clip passed back
  if ((capture_fd=open(capfile,O_RDONLY))>-1) {
    memset(capbuf,0,256);
    if ((length=read(capture_fd,capbuf,256))) {
      if (get_token_count (capbuf,'|')>2) {
	array=g_strsplit(capbuf,"|",3);
	new_frames=atoi(array[1]);

	if (new_frames>0) {
	  new_file=mainw->first_free_file;
	  mainw->current_file=new_file;
	  cfile=(file *)(g_malloc(sizeof(file)));
	  g_snprintf(cfile->handle,255,"%s",array[0]);
	  g_strfreev(array);
	  create_cfile();
	  g_snprintf(cfile->file_name,256,"Capture %d",mainw->cap_number);
	  g_snprintf(cfile->name,256,"Capture %d",mainw->cap_number++);
	  g_snprintf(cfile->type,40,"Frames");

	  cfile->progress_start=cfile->start=1;
	  cfile->progress_end=cfile->frames=cfile->end=new_frames;
	  cfile->pb_fps=cfile->fps=mainw->rec_fps;

	  cfile->hsize=CEIL(mainw->foreign_width,4);
	  cfile->vsize=CEIL(mainw->foreign_height,4);
	  
	  if (mainw->rec_achans>0) {
	    cfile->arate=cfile->arps=mainw->rec_arate;
	    cfile->achans=mainw->rec_achans;
	    cfile->asampsize=mainw->rec_asamps;
	    cfile->signed_endian=mainw->rec_signed_endian;
	  }
	  
	  // TODO - dirsep

	  g_snprintf(file_name,PATH_MAX,"%s/%s/",prefs->tmpdir,cfile->handle);
	  
	  com=g_strdup_printf("smogrify fill_and_redo_frames \"%s\" %d %d %d \"%s\" %.4f %d %d %d %d %d",
			      cfile->handle,cfile->frames,cfile->hsize,cfile->vsize,
			      cfile->img_type==IMG_TYPE_JPEG?"jpg":"png",cfile->fps,cfile->arate,
			      cfile->achans,cfile->asampsize,!(cfile->signed_endian&AFORM_UNSIGNED),
			      !(cfile->signed_endian&AFORM_BIG_ENDIAN));
	  unlink(cfile->info_file);
	  mainw->com_failed=FALSE;
	  lives_system(com,FALSE);


	  cfile->nopreview=TRUE;
	  if (!mainw->com_failed&&do_progress_dialog(TRUE,TRUE,_ ("Cleaning up clip"))) {
	    get_next_free_file();
	  }
	  else {
	    // cancelled cleanup
	    new_frames=0;
	    close_current_file(old_file);
	  }
	  g_free(com);
	}
	else g_strfreev(array);
      }
      close(capture_fd);
      unlink(capfile);
    }
  }

  if (new_frames==0) {
    // nothing captured; or cancelled
    g_free(capfile);
    return FALSE;
  }

  cfile->nopreview=FALSE;
  g_free(capfile);

  add_to_winmenu();
  if (mainw->multitrack==NULL) switch_to_file(old_file,mainw->current_file);
  
  else {
    mainw->current_file=mainw->multitrack->render_file;
    mt_init_clips(mainw->multitrack,new_file,TRUE);
    mt_clip_select(mainw->multitrack,TRUE);
  }

  cfile->is_loaded=TRUE;
  cfile->changed=TRUE;
  save_clip_values(mainw->current_file);
  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
#ifdef ENABLE_OSC
  lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif
  return TRUE;
}


void
set_menu_text(GtkWidget *menuitem, const gchar *text, gboolean use_mnemonic) {
  GtkWidget *label;
  if (GTK_IS_MENU_ITEM (menuitem)) {
    label=gtk_bin_get_child(GTK_BIN(menuitem));
    if (use_mnemonic) {
      gtk_label_set_text_with_mnemonic(GTK_LABEL(label),text);
    }
    else {
      gtk_label_set_text(GTK_LABEL(label),text);
    }
  }
}


void
get_menu_text(GtkWidget *menuitem, gchar *text) {
  GtkWidget *label=gtk_bin_get_child(GTK_BIN(menuitem));
  g_snprintf(text,255,"%s",gtk_label_get_text(GTK_LABEL(label)));
}

void
get_menu_text_long(GtkWidget *menuitem, gchar *text) {
  GtkWidget *label=gtk_bin_get_child(GTK_BIN(menuitem));
  g_snprintf(text,32768,"%s",gtk_label_get_text(GTK_LABEL(label)));
}


LIVES_INLINE gboolean int_array_contains_value(int *array, int num_elems, int value) {
  int i;
  for (i=0;i<num_elems;i++) {
    if (array[i]==value) return TRUE;
  }
  return FALSE;
}


void 
reset_clip_menu (void) {
  // sometimes the clip menu gets messed up, e.g. after reloading a set.
  // This function will clean up the 'x's and so on.

  int i;
  GtkWidget *active_image=NULL;
  gchar menutext[32768];

  for (i=1;i<=MAX_FILES;i++) {
    if (!(mainw->files[i]==NULL)) {
      if (!(active_image==NULL)) {
	active_image=NULL;
      }
      if (mainw->files[i]->opening) {
	active_image = gtk_image_new_from_stock ("gtk-no", GTK_ICON_SIZE_MENU);
      }
      else {
	if (i==mainw->current_file) {
	  active_image = gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_MENU);
	}
      }
      if (!(active_image==NULL)) {
	gtk_widget_show (active_image);
      }
      if (mainw->files[i]->menuentry!=NULL) {
	get_menu_text_long(mainw->files[i]->menuentry,menutext);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->files[i]->menuentry), active_image);
	set_menu_text(mainw->files[i]->menuentry,menutext,FALSE);
	gtk_widget_queue_draw(mainw->files[i]->menuentry);
      }
    }
  }
}



gboolean check_file(const gchar *file_name, gboolean check_existing) {
  int check;
  gboolean exists=FALSE;
  gchar *msg;
  // file_name should be in utf8
  gchar *lfile_name=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL);

  // check if file exists
  if (g_file_test (lfile_name, G_FILE_TEST_EXISTS)) {
    if (check_existing) {
      msg=g_strdup_printf (_ ("\n%s\nalready exists.\n\nOverwrite ?\n"),file_name);
      if (!do_warning_dialog(msg)) {
	g_free (msg);
	g_free(lfile_name);
	return FALSE;
      }
      g_free (msg);
    }
    check=open(lfile_name,O_WRONLY);
    exists=TRUE;
  }
  // if not, check if we can write to it
  else {
    check=open(lfile_name,O_CREAT|O_EXCL|O_WRONLY,S_IRUSR|S_IWUSR);
  }

  if (check<0) {
    if (mainw!=NULL&&mainw->is_ready) {
      if (errno==EACCES)
	do_file_perm_error(lfile_name);
      else 
	do_write_failed_error_s(lfile_name,NULL);
    }
    g_free(lfile_name);
    return FALSE;
  }

  close(check);
  if (!exists) {
    unlink (lfile_name);
  }
  g_free(lfile_name);
  return TRUE;
}



gboolean check_dir_access (const gchar *dir) {
  // if a directory exists, make sure it is readable and writable
  // otherwise create it and then check

  // dir is in locale encoding

  // see also is_writeable_dir() which uses statvfs

  gboolean exists=g_file_test (dir, G_FILE_TEST_EXISTS);
  gchar *com;
  gchar *testfile;
  gboolean is_OK=FALSE;

  if (!exists) {
    com=g_strdup_printf ("/bin/mkdir -p %s",dir);
    lives_system (com,TRUE);
    g_free (com);
  }
  if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) return FALSE;

  testfile=g_build_filename (dir,"livestst.txt",NULL);
  com=g_strdup_printf ("/bin/touch \"%s\"",testfile);
  lives_system (com,TRUE);
  g_free (com);
  if ((is_OK=g_file_test(testfile, G_FILE_TEST_EXISTS))) {
    unlink (testfile);
  }
  g_free (testfile);
  if (!exists) {
    rmdir(dir);
  }
  return is_OK;
}


gboolean check_dev_busy(gchar *devstr) {
  int ret;
  int fd=open(devstr,O_RDONLY|O_NONBLOCK);
  if (fd==-1) return FALSE;
  ret=flock(fd,LOCK_EX|LOCK_NB);
  close(fd);
  if (ret==-1) return FALSE;
  return TRUE;
}



void activate_url_inner(const gchar *link) {
#ifdef HAVE_GTK_NICE_VERSION
  GError *err=NULL;
  gtk_show_uri(NULL,link,GDK_CURRENT_TIME,&err);
#else
  gchar *com = getenv("BROWSER");
  com = g_strdup_printf("\"%s\" '%s' &", com ? com : "gnome-open", link);
  lives_system(com,FALSE);
  g_free(com);
#endif
}


void activate_url (GtkAboutDialog *about, const gchar *link, gpointer data) {
  activate_url_inner(link);
}


void show_manual_section (const gchar *lang, const gchar *section) {
  gchar *tmp=NULL,*tmp2=NULL;
  const gchar *link;

  link=g_strdup_printf("%s%s%s%s",LIVES_MANUAL_URL,(lang==NULL?"":(tmp2=g_strdup_printf("//%s//",lang))),
		       LIVES_MANUAL_FILENAME,(section==NULL?"":(tmp=g_strdup_printf("#%s",section))));

  activate_url_inner(link);

  if (tmp!=NULL) g_free(tmp);
  if (tmp2!=NULL) g_free(tmp2);

}


gulong
get_file_size(int fd) {
  // get the size of file fd
  struct stat filestat;
  fstat(fd,&filestat);
  return (gulong)(filestat.st_size);
}

gulong
sget_file_size(const gchar *name) {
  // get the size of file fd
  struct stat filestat;
  int fd;

  if ((fd=open(name,O_RDONLY))==-1) {
    return (guint)0;
  }

  fstat(fd,&filestat);
  close(fd);

  return (gulong)(filestat.st_size);
}


void reget_afilesize (int fileno) {
  // re-get the audio file size
  gchar *afile;
  file *sfile=mainw->files[fileno];
  gboolean bad_header=FALSE;

  if (mainw->multitrack!=NULL) return; // otherwise achans gets set to 0...

  if (!sfile->opening) afile=g_build_filename (prefs->tmpdir,sfile->handle,"audio",NULL);
  else afile=g_build_filename (prefs->tmpdir,sfile->handle,"audiodump.pcm",NULL);
  if ((sfile->afilesize=sget_file_size (afile))==0l) {
    if (!sfile->opening&&fileno!=mainw->ascrap_file&&fileno!=mainw->scrap_file) {
      if (sfile->arate!=0||sfile->achans!=0||sfile->asampsize!=0||sfile->arps!=0) {
	sfile->arate=sfile->achans=sfile->asampsize=sfile->arps=0;
	save_clip_value(fileno,CLIP_DETAILS_ACHANS,&sfile->achans);
	if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
	save_clip_value(fileno,CLIP_DETAILS_ARATE,&sfile->arps);
	if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
	save_clip_value(fileno,CLIP_DETAILS_PB_ARATE,&sfile->arate);
	if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
	save_clip_value(fileno,CLIP_DETAILS_ASAMPS,&sfile->asampsize);
	if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
	if (bad_header) do_header_write_error(fileno);
      }
    }
  }
  g_free (afile);
}





void
colour_equal(GdkColor *c1, const GdkColor *c2) {
  c1->pixel=c2->pixel;
  c1->red=c2->red;
  c1->green=c2->green;
  c1->blue=c2->blue;
}


gboolean
create_event_space(gint length) {
  // try to create desired events
  // if we run out of memory, all events requested are freed, and we return FALSE
  // otherwise we return TRUE

  // NOTE: this is the OLD event system, it's only used for reordering in the clip editor

  if (cfile->events[0]!=NULL) {
    g_free(cfile->events[0]);
  }
  if ((cfile->events[0]=(event *)(g_try_malloc(sizeof(event)*length)))==NULL) {
    // memory overflow
    return FALSE;
  }
  memset(cfile->events[0],0,length*sizeof(event));
  return TRUE;
}



gint lives_list_index (GList *list, const gchar *data) {
  // find data in list, GTK's version is broken
  // well, actually not broken - but we need to use strcmp

  int i;
  gint len;
  if (list==NULL) return -1;

  len=g_list_length (list);

  for (i=0;i<len;i++) {
    if (!strcmp ((gchar *)g_list_nth_data (list,i),data)) return i;
  }
  return -1;
}






void 
add_to_recent(const gchar *filename, gdouble start, gint frames, const gchar *extra_params) {
  gchar buff[PATH_MAX];
  gchar *file,*tmp;

  if (frames>0) {
    if (extra_params==NULL||(strlen(extra_params)==0)) file=g_strdup_printf ("%s|%.2f|%d",filename,start,frames);
    else file=g_strdup_printf ("%s|%.2f|%d\n%s",filename,start,frames,extra_params);
  }
  else {
    if (extra_params==NULL||(strlen(extra_params)==0)) file=g_strdup (filename);
    else file=g_strdup_printf ("%s\n%s",filename,extra_params);
  }

  get_menu_text(mainw->recent1,buff);
  if (strcmp(file,buff)) {
    get_menu_text(mainw->recent2,buff);
    if (strcmp(file,buff)) {
      get_menu_text(mainw->recent3,buff);
      if (strcmp(file,buff)) {
	// not in list, or at pos 4
	get_menu_text(mainw->recent3,buff);
	set_menu_text(mainw->recent4,buff,FALSE);
	if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent4,buff,FALSE);
	set_pref("recent4",(tmp=g_filename_from_utf8(buff,-1,NULL,NULL,NULL)));
	g_free(tmp);

	get_menu_text(mainw->recent2,buff);
	set_menu_text(mainw->recent3,buff,FALSE);
	if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent3,buff,FALSE);
	set_pref("recent3",(tmp=g_filename_from_utf8(buff,-1,NULL,NULL,NULL)));
	g_free(tmp);

	get_menu_text(mainw->recent1,buff);
	set_menu_text(mainw->recent2,buff,FALSE);
	if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent2,buff,FALSE);
	set_pref("recent2",(tmp=g_filename_from_utf8(buff,-1,NULL,NULL,NULL)));
	g_free(tmp);
	
	set_menu_text(mainw->recent1,file,FALSE);
	if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent1,file,FALSE);
	set_pref("recent1",(tmp=g_filename_from_utf8(file,-1,NULL,NULL,NULL)));
	g_free(tmp);
      }
      else {
	// #3 in list
	get_menu_text(mainw->recent2,buff);
	set_menu_text(mainw->recent3,buff,FALSE);
	if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent3,buff,FALSE);
	set_pref("recent3",(tmp=g_filename_from_utf8(buff,-1,NULL,NULL,NULL)));
	g_free(tmp);
	
	get_menu_text(mainw->recent1,buff);
	set_menu_text(mainw->recent2,buff,FALSE);
	if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent2,buff,FALSE);
	set_pref("recent2",(tmp=g_filename_from_utf8(buff,-1,NULL,NULL,NULL)));
	g_free(tmp);

	set_menu_text(mainw->recent1,file,FALSE);
	if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent1,file,FALSE);
	set_pref("recent1",(tmp=g_filename_from_utf8(file,-1,NULL,NULL,NULL)));
	g_free(tmp);
      }
    }
    else {
      // #2 in list
      get_menu_text(mainw->recent1,buff);
      set_menu_text(mainw->recent2,buff,FALSE);
      if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent2,buff,FALSE);
      set_pref("recent2",(tmp=g_filename_from_utf8(buff,-1,NULL,NULL,NULL)));
      g_free(tmp);
	
      set_menu_text(mainw->recent1,file,FALSE);
      if (mainw->multitrack!=NULL) set_menu_text(mainw->multitrack->recent1,file,FALSE);
      set_pref("recent1",(tmp=g_filename_from_utf8(file,-1,NULL,NULL,NULL)));
      g_free(tmp);
    }
  }
  else {
    // I'm number 1, so why change ;-)
  }

  get_menu_text(mainw->recent1,buff);
  if (strlen(buff)) {
    gtk_widget_show (mainw->recent1);
  }
  get_menu_text(mainw->recent2,buff);
  if (strlen(buff)) {
    gtk_widget_show (mainw->recent2);
  }
  get_menu_text(mainw->recent3,buff);
  if (strlen(buff)) {
    gtk_widget_show (mainw->recent3);
  }
  get_menu_text(mainw->recent4,buff);
  if (strlen(buff)) {
    gtk_widget_show (mainw->recent4);
  }

  g_free(file);
}


void lives_set_cursor_style(lives_cursor_t cstyle, GdkWindow *window) {
  if (mainw->cursor!=NULL) gdk_cursor_unref(mainw->cursor);
  mainw->cursor=NULL;

  if (window==NULL) {
    if (mainw->multitrack==NULL&&mainw->is_ready) window=mainw->LiVES->window;
    else if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) window=mainw->multitrack->window->window;
    else return;
  }

  switch(cstyle) {
  case LIVES_CURSOR_NORMAL:
    gdk_window_set_cursor (window, NULL);
    return;
  case LIVES_CURSOR_BUSY:
    mainw->cursor=gdk_cursor_new(GDK_WATCH);
    gdk_window_set_cursor (window, mainw->cursor);
    return;
  default:
    return;
  }
}




gint 
verhash (gchar *version) {
  gchar *s;
  gint major=0;
  gint minor=0;
  gint micro=0;

  if (!(strlen(version))) return 0;

  s=strtok (version,".");
  if (!(s==NULL)) {
    major=atoi (s);
    s=strtok (NULL,".");
    if (!(s==NULL)) {
      minor=atoi (s);
      s=strtok (NULL,".");
      if (!(s==NULL)) {
	micro=atoi (s);
      }
    }
  }
  return major*1000000+minor*1000+micro;
}



#ifdef PRODUCE_LOG
// disabled by default
void lives_log(const char *what) {
  char *lives_log_file=g_build_filename(prefs->tmpdir,LIVES_LOG_FILE,NULL);
  if (mainw->log_fd<0) mainw->log_fd=open(lives_log_file,O_WRONLY|O_CREAT,S_IRUSER|S_IWUSER);
  if (mainw->log_fd!=-1) {
    char *msg=g_strdup("%s|%d|",what,mainw->current_file);
    write (mainw->log_fd,msg,strlen(msg));
    g_free(msg);
  }
  g_free(lives_log_file);
}
#endif




// TODO - move into undo.c
void 
set_undoable (const gchar *what, gboolean sensitive) {
  if (mainw->current_file>-1) {
    cfile->redoable=FALSE;
    cfile->undoable=sensitive;
    if (!(what==NULL)) {
      gchar *what_safe=g_strdelimit (g_strdup (what),"_",' ');
      g_snprintf(cfile->undo_text,32,_ ("_Undo %s"),what_safe);
      g_snprintf(cfile->redo_text,32,_ ("_Redo %s"),what_safe);
      g_free (what_safe);
    }
    else {
      cfile->undoable=FALSE;
      cfile->undo_action=UNDO_NONE;
      g_snprintf(cfile->undo_text,32,"%s",_ ("_Undo"));
      g_snprintf(cfile->redo_text,32,"%s",_ ("_Redo"));
    }
    set_menu_text(mainw->undo,cfile->undo_text,TRUE);
    set_menu_text(mainw->redo,cfile->redo_text,TRUE);
  }

  gtk_widget_hide(mainw->redo);
  gtk_widget_show(mainw->undo);
  gtk_widget_set_sensitive (mainw->undo,sensitive);

#ifdef PRODUCE_LOG
  lives_log(what);
#endif


}

void 
set_redoable (const gchar *what, gboolean sensitive) {
  if (mainw->current_file>-1) {
    cfile->undoable=FALSE;
    cfile->redoable=sensitive;
    if (!(what==NULL)) {
      gchar *what_safe=g_strdelimit (g_strdup (what),"_",' ');
      g_snprintf(cfile->undo_text,32,_ ("_Undo %s"),what_safe);
      g_snprintf(cfile->redo_text,32,_ ("_Redo %s"),what_safe);
      g_free (what_safe);
    }
    else {
      cfile->redoable=FALSE;
      cfile->undo_action=UNDO_NONE;
      g_snprintf(cfile->undo_text,32,"%s",_ ("_Undo"));
      g_snprintf(cfile->redo_text,32,"%s",_ ("_Redo"));
    }
    set_menu_text(mainw->undo,cfile->undo_text,TRUE);
    set_menu_text(mainw->redo,cfile->redo_text,TRUE);
  }

  gtk_widget_hide(mainw->undo);
  gtk_widget_show(mainw->redo);
  gtk_widget_set_sensitive (mainw->redo,sensitive);
}


void 
set_sel_label (GtkWidget *sel_label) {
  gchar *tstr,*frstr,*tmp;
  gchar *sy,*sz;

  if (mainw->current_file==-1||!cfile->frames||mainw->multitrack!=NULL) {
    gtk_label_set_text(GTK_LABEL(sel_label),_ ("-------------Selection------------"));
  }
  else {
    tstr=g_strdup_printf ("%.2f",calc_time_from_frame (mainw->current_file,cfile->end+1)-
			  calc_time_from_frame (mainw->current_file,cfile->start));
    frstr=g_strdup_printf ("%d",cfile->end-cfile->start+1);

    // TRANSLATORS: - try to keep the text of the middle part the same length, by deleting "-" if necessary
    gtk_label_set_text(GTK_LABEL(sel_label),(tmp=g_strconcat ("---------- [ ",tstr,(sy=(g_strdup(_(" sec ] ----------Selection---------- [ ")))),frstr,(sz=g_strdup(_(" frames ] ----------"))),NULL)));
    g_free(sy);
    g_free(sz);

    g_free (tmp);
    g_free (frstr);
    g_free (tstr);
  }
  gtk_widget_queue_draw (sel_label);
}




LIVES_INLINE void g_list_free_strings(GList *slist) {
  GList *list=slist;
  if (list==NULL) return;
  while (list!=NULL) {
    if (list->data!=NULL) {
      //g_printerr("free %s\n",list->data);
      g_free(list->data);
    }
    list=list->next;
  }
}


gboolean cache_file_contents(const gchar *filename) {
  FILE *hfile;
  gchar buff[65536];

  if (mainw->cached_list!=NULL) {
    g_list_free_strings(mainw->cached_list);
    g_list_free(mainw->cached_list);
    mainw->cached_list=NULL;
  }

  if (!(hfile=fopen(filename,"r"))) return FALSE;
  while (fgets(buff,65536,hfile)!=NULL) {
    mainw->cached_list=g_list_append(mainw->cached_list,g_strdup(buff));
    threaded_dialog_spin();
  }
  fclose(hfile);
  return TRUE;
}


gchar *get_val_from_cached_list(const gchar *key, size_t maxlen) {
  GList *clist=mainw->cached_list;
  gchar *keystr_start=g_strdup_printf("<%s>",key);
  gchar *keystr_end=g_strdup_printf("</%s>",key);
  size_t kslen=strlen(keystr_start);
  size_t kelen=strlen(keystr_end);

  gboolean gotit=FALSE;
  gchar buff[maxlen];

  memset(buff,0,1);

  while (clist!=NULL) {
    if (gotit) {
      if (!strncmp(keystr_end,(gchar *)clist->data,kelen)) {
	if (clist->prev!=NULL) clist->prev->next=clist->next;
	break;
      }
      if (strncmp((gchar *)clist->data,"#",1)) g_strappend(buff,maxlen,(gchar *)clist->data);
      else {
	if (clist->prev!=NULL) clist->prev->next=clist->next;
      }
    }
    else if (!strncmp(keystr_start,(gchar *)clist->data,kslen)) {
      gotit=TRUE;
      if (clist->prev!=NULL) clist->prev->next=clist->next;
    }
    clist=clist->next;
  }
  g_free(keystr_start);
  g_free(keystr_end);

  if (!gotit) return NULL;

  if (strlen(buff)>0) memset(buff+strlen(buff)-1,0,1); // remove trailing newline

  return g_strdup(buff);
}




gchar *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp) {
  gchar *key=NULL;

  switch (what) {
  case CLIP_DETAILS_HEADER_VERSION:
    key=g_strdup("header_version");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_BPP:
    key=g_strdup("bpp");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_FPS:
    key=g_strdup("fps");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_PB_FPS:
    key=g_strdup("pb_fps");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_WIDTH:
    key=g_strdup("width");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_HEIGHT:
    key=g_strdup("height");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    key=g_strdup("unique_id");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_ARATE:
    key=g_strdup("audio_rate");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_PB_ARATE:
    key=g_strdup("pb_audio_rate");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_ACHANS:
    key=g_strdup("audio_channels");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_ASIGNED:
    key=g_strdup("audio_signed");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_AENDIAN:
    key=g_strdup("audio_endian");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_ASAMPS:
    key=g_strdup("audio_sample_size");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_FRAMES:
    key=g_strdup("frames");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_TITLE:
    key=g_strdup("title");
    break;
  case CLIP_DETAILS_AUTHOR:
    key=g_strdup("author");
    break;
  case CLIP_DETAILS_COMMENT:
    key=g_strdup("comment");
    break;
  case CLIP_DETAILS_KEYWORDS:
    key=g_strdup("keywords");
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    key=g_strdup("pb_frameno");
    if (maxlenp!=NULL) *maxlenp=256;
    break;
  case CLIP_DETAILS_CLIPNAME:
    key=g_strdup("clipname");
    break;
  case CLIP_DETAILS_FILENAME:
    key=g_strdup("filename");
    break;
  case CLIP_DETAILS_INTERLACE:
    key=g_strdup("interlace");
    break;
  default:
    break;
  }
  return key;
}



gboolean get_clip_value(int which, lives_clip_details_t what, void *retval, size_t maxlen) {
  FILE *valfile;
  time_t old_time=0,new_time=0;
  struct stat mystat;

  gchar *vfile;
  gchar *lives_header=NULL;
  gchar *old_header;
  gchar *com;
  gchar *val;
  gchar *key;
  gchar *tmp;

  int alarm_handle;
  int retval2=0;

  gboolean timeout;

  if (mainw->cached_list==NULL) {
    
    lives_header=g_build_filename(prefs->tmpdir,mainw->files[which]->handle,"header.lives",NULL);
    old_header=g_build_filename(prefs->tmpdir,mainw->files[which]->handle,"header",NULL);
    
    // TODO - remove this some time before 2038
    if (!stat(old_header,&mystat)) old_time=mystat.st_mtime;
    if (!stat(lives_header,&mystat)) new_time=mystat.st_mtime;
    g_free(old_header);
    
    if (old_time>new_time) {
      g_free(lives_header);
      return FALSE; // clip has been edited by an older version of LiVES
    }
  }
  //////////////////////////////////////////////////
  key=clip_detail_to_string(what,&maxlen);

  if (key==NULL) {
    tmp=g_strdup_printf("Invalid detail %d requested from file %s",which,lives_header);
    LIVES_ERROR(tmp);
    g_free(tmp);
    g_free(lives_header);
    return FALSE;
  }

  mainw->read_failed=FALSE;

  if (mainw->cached_list!=NULL) {
    val=get_val_from_cached_list(key,maxlen);
    g_free(key);
    if (val==NULL) return FALSE;
  }
  else {
    com=g_strdup_printf("smogrify get_clip_value \"%s\" %d %d \"%s\"",key,getuid(),getpid(),lives_header);
    g_free(lives_header);
    g_free(key);
    
    val=(gchar *)g_malloc(maxlen);
    memset(val,0,maxlen);
    
    threaded_dialog_spin();

    if (lives_system(com,TRUE)) {
      tempdir_warning();
      threaded_dialog_spin();
      g_free(com);
      return FALSE;
    }
    
    vfile=g_strdup_printf("%s/.smogval.%d.%d",prefs->tmpdir,getuid(),getpid());

    do {
      retval2=0;
      timeout=FALSE;
      mainw->read_failed=FALSE;
      
      alarm_handle=lives_alarm_set(LIVES_PREFS_TIMEOUT);
      
      do {
	if (!((valfile=fopen(vfile,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
	  if (!timeout) {
	    if (!(mainw==NULL)) {
	      weed_plant_t *frame_layer=mainw->frame_layer;
	      mainw->frame_layer=NULL;
	      while (g_main_context_iteration(NULL,FALSE));
	      mainw->frame_layer=frame_layer;
	    }
	    g_usleep(prefs->sleep_time);
	  }
	  else break;
	}
      } while (!valfile);
      
      lives_alarm_clear(alarm_handle);
      
      if (timeout) {
	retval2=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
      }
      else {
	mainw->read_failed=FALSE;
	lives_fgets(val,maxlen,valfile);
	fclose(valfile);
	unlink(vfile);
	if (mainw->read_failed) {
	  retval2=do_read_failed_error_s_with_retry(vfile,NULL,NULL);
	}
      }
    } while (retval2==LIVES_RETRY);
    
    g_free(vfile);
    g_free(com);
  }

  if (retval2==LIVES_CANCEL) {
    return FALSE;
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
  case CLIP_DETAILS_WIDTH:
  case CLIP_DETAILS_HEIGHT:
  case CLIP_DETAILS_ARATE:
  case CLIP_DETAILS_ACHANS:
  case CLIP_DETAILS_ASAMPS:
  case CLIP_DETAILS_FRAMES:
  case CLIP_DETAILS_HEADER_VERSION:
    *(gint *)retval=atoi(val);
    break;
  case CLIP_DETAILS_ASIGNED:
    *(gint *)retval=0;
    if (mainw->files[which]->header_version==0) *(gint *)retval=atoi(val);
    if (*(gint *)retval==0&&(!strcasecmp(val,"false"))) *(gint *)retval=1; // unsigned
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    *(gint *)retval=atoi(val);
    if (retval==0) *(gint *)retval=1;
    break;
  case CLIP_DETAILS_PB_ARATE:
    *(gint *)retval=atoi(val);
    if (retval==0) *(gint *)retval=mainw->files[which]->arps;
    break;
  case CLIP_DETAILS_INTERLACE:
    *(gint *)retval=atoi(val);
    break;
  case CLIP_DETAILS_FPS:
    *(gdouble *)retval=strtod(val,NULL);
    if (*(gdouble *)retval==0.) *(gdouble *)retval=prefs->default_fps;
    break;
  case CLIP_DETAILS_PB_FPS:
    *(gdouble *)retval=strtod(val,NULL);
    if (*(gdouble *)retval==0.) *(gdouble *)retval=mainw->files[which]->fps;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    if (capable->cpu_bits==32) {
      *(gint64 *)retval=strtoll(val,NULL,10);
    }
    else {
      *(gint64 *)retval=strtol(val,NULL,10);
    }
    break;
  case CLIP_DETAILS_AENDIAN:
    *(gint *)retval=atoi(val)*2;
    break;
  case CLIP_DETAILS_TITLE:
  case CLIP_DETAILS_AUTHOR:
  case CLIP_DETAILS_COMMENT:
  case CLIP_DETAILS_CLIPNAME:
  case CLIP_DETAILS_KEYWORDS:
    g_snprintf((gchar *)retval,maxlen,"%s",val);
    break;
  case CLIP_DETAILS_FILENAME:
    g_snprintf((gchar *)retval,maxlen,"%s",(tmp=g_filename_to_utf8(val,-1,NULL,NULL,NULL)));
    g_free(tmp);
    break;
  }
  g_free(val);
  return TRUE;
}



void save_clip_value(int which, lives_clip_details_t what, void *val) {
  gchar *lives_header;
  gchar *com,*tmp;
  gchar *myval;
  gchar *key;

  mainw->write_failed=mainw->com_failed=FALSE;

  if (which==0||which==mainw->scrap_file) return;

  lives_header=g_build_filename(prefs->tmpdir,mainw->files[which]->handle,"header.lives",NULL);
  key=clip_detail_to_string(what,NULL);

  if (key==NULL) {
    tmp=g_strdup_printf("Invalid detail %d added for file %s",which,lives_header);
    LIVES_ERROR(tmp);
    g_free(tmp);
    g_free(lives_header);
    return;
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_FPS:
    if (!mainw->files[which]->ratio_fps) myval=g_strdup_printf("%.3f",*(gdouble *)val);
    else myval=g_strdup_printf("%.8f",*(gdouble *)val);
    break;
  case CLIP_DETAILS_PB_FPS:
    if (mainw->files[which]->ratio_fps&&(mainw->files[which]->pb_fps==mainw->files[which]->fps)) 
      myval=g_strdup_printf("%.8f",*(gdouble *)val);
    else myval=g_strdup_printf("%.3f",*(gdouble *)val);
    break;
  case CLIP_DETAILS_WIDTH:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_HEIGHT:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    myval=g_strdup_printf("%"PRId64,*(gint64 *)val);
    break;
  case CLIP_DETAILS_ARATE:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_PB_ARATE:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_ACHANS:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_ASIGNED:
    if (*(gint *)val==1) myval=g_strdup("true");
    else myval=g_strdup("false");
    break;
  case CLIP_DETAILS_AENDIAN:
    myval=g_strdup_printf("%d",*(gint *)val/2);
    break;
  case CLIP_DETAILS_ASAMPS:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_FRAMES:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_INTERLACE:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_TITLE:
    myval=g_strdup((gchar *)val);
    break;
  case CLIP_DETAILS_AUTHOR:
    myval=g_strdup((gchar *)val);
    break;
  case CLIP_DETAILS_COMMENT:
    myval=g_strdup((gchar *)val);
    break;
  case CLIP_DETAILS_KEYWORDS:
    myval=g_strdup((gchar *)val);
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_CLIPNAME:
    myval=g_strdup((gchar *)val);
    break;
  case CLIP_DETAILS_FILENAME:
    myval=g_filename_from_utf8((gchar *)val,-1,NULL,NULL,NULL);
    break;
  case CLIP_DETAILS_HEADER_VERSION:
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  default:
    return;
  }

  if (mainw->clip_header!=NULL) {
    gchar *keystr_start=g_strdup_printf("<%s>\n",key);
    gchar *keystr_end=g_strdup_printf("\n</%s>\n",key);
    lives_fputs(keystr_start,mainw->clip_header);
    lives_fputs(myval,mainw->clip_header);
    lives_fputs(keystr_end,mainw->clip_header);
    g_free(keystr_start);
    g_free(keystr_end);

  }
  else {
    com=g_strdup_printf("smogrify set_clip_value \"%s\" \"%s\" \"%s\"",lives_header,key,myval);
    lives_system(com,FALSE);
    g_free(com);
  }

  g_free(lives_header);
  g_free(myval);
  g_free(key);
  
  return;
}



GList *get_set_list(const gchar *dir) {
  // get list of sets in top level dir
  GList *setlist=NULL;
  DIR *tldir,*subdir;
  struct dirent *tdirent,*subdirent;
  gchar *subdirname;

  if (dir==NULL) return NULL;

  tldir=opendir(dir);

  if (tldir==NULL) return NULL;

  while (1) {
    tdirent=readdir(tldir);

    if (tdirent==NULL) {
      closedir(tldir);
      return setlist;
    }
    
    if (!strncmp(tdirent->d_name,"..",strlen(tdirent->d_name))) continue;

    subdirname=g_build_filename(dir,tdirent->d_name,NULL);

    subdir=opendir(subdirname);

    if (subdir==NULL) {
      g_free(subdirname);
      continue;
    }

    while (1) {
      subdirent=readdir(subdir);

      if (subdirent==NULL) {
	break;
      }

      if (!strcmp(subdirent->d_name,"order")) {
	setlist=g_list_append(setlist,g_strdup(tdirent->d_name));
	break;
      }
    }
    g_free(subdirname);
    closedir(subdir);
  }
}




gboolean check_for_ratio_fps (gdouble fps) {
  gboolean ratio_fps;
  gchar *test_fps_string1=g_strdup_printf ("%.3f00000",fps);
  gchar *test_fps_string2=g_strdup_printf ("%.8f",fps);
  
  if (strcmp (test_fps_string1,test_fps_string2)) {
    // got a ratio
    ratio_fps=TRUE;
  }
  else {
    ratio_fps=FALSE;
  }
  g_free (test_fps_string1);
  g_free (test_fps_string2);

  return ratio_fps;
}


gdouble get_ratio_fps(const gchar *string) {
  // return a ratio (8dp) fps from a string with format num:denom
  gdouble fps;
  gchar *fps_string;
  gchar **array=g_strsplit(string,":",2);
  gint num=atoi (array[0]);
  gint denom=atoi (array[1]);
  g_strfreev (array);
  fps=(gdouble)num/(gdouble)denom;
  fps_string=g_strdup_printf("%.8f",fps);
  fps=g_strtod(fps_string,NULL);
  g_free(fps_string);
  return fps;
}



gchar *remove_trailing_zeroes(gdouble val) {
  int i;
  gdouble xval=val;

  if (val==(int)val) return g_strdup_printf("%d",(int)val);
  for (i=0;i<=16;i++) {
    xval*=10.;
    if (xval==(int)xval) return g_strdup_printf("%.*f",i,val);
  }
  return g_strdup_printf("%.*f",i,val);
}


guint get_signed_endian (gboolean is_signed, gboolean little_endian) {
  // asigned TRUE == signed, FALSE == unsigned


  if (is_signed) {
    if (little_endian) {
      return 0;
    }
    else {
      return AFORM_BIG_ENDIAN;
    }
  }
  else {
    if (!is_signed) { 
      if (little_endian) {
	return AFORM_UNSIGNED;
      }
      else {
	return AFORM_UNSIGNED|AFORM_BIG_ENDIAN;
      }
    }
  }
  return AFORM_UNKNOWN;
}




gint 
get_token_count (const gchar *string, int delim) {
  gint pieces=1;
  if (string==NULL) return 0;
  if (delim<=0||delim>255) return 1;

  while ((string=strchr(string,delim))!=NULL) {
    pieces++;
    string++;
  }
  return pieces;
}



gchar *subst (const gchar *string, const gchar *from, const gchar *to) {
  // return a string with all occurrences of from replaced with to
  // return value should be freed after use
  gchar *ret=g_strdup(string),*first;
  gchar *search=ret;

  while ((search=strstr (search,from))!=NULL) {
    first=g_strndup(ret,search-ret);
    search=g_strdup(search+strlen(from));
    g_free(ret);
    ret=g_strconcat (first,to,search,NULL);
    g_free(search);
    search=ret+strlen(first)+strlen(to);
    g_free(first);
  }
  return ret;
}

gchar *insert_newlines(const gchar *text, int maxwidth) {
  // crude formating of strings, ensure a newline after every run of maxwidth chars
  // does not take into account for example utf8 multi byte chars

  char newline[]="\n";
  gchar *retstr;
  register int i;
  int xtoffs;
  gboolean needsnl=FALSE;
  size_t req_size=1;  // for the terminating \0
  size_t tlen;
  size_t nlen=strlen(newline);
  size_t runlen=0;
  wchar_t utfsym;

  if (text==NULL) return NULL;

  if (maxwidth<1) return g_strdup("Bad maxwidth, dummy");

  tlen=strlen(text);

  xtoffs=mbtowc(NULL,NULL,0); // reset read state


  //pass 1, get the required size
  for (i=0;i<tlen;i+=xtoffs) {
    xtoffs=mbtowc(&utfsym,&text[i],4); // get next utf8 wchar
    if (!strncmp(text+i,"\n",nlen)) runlen=0; // is a newline (in any encoding)
    else {
      runlen++;
      if (needsnl) req_size+=nlen; ///< we will insert a nl here
    }

    if (runlen==maxwidth) {
      if (i<tlen-1 && (strncmp(text+i+1,"\n",nlen))) {
	// needs a newline
	needsnl=TRUE;
	runlen=0;
      }
    }
    else needsnl=FALSE;
    req_size+=xtoffs;
  }




  retstr=(gchar *)g_malloc(req_size);
  req_size=0; // reuse as a ptr to offset in retstr
  runlen=0;
  needsnl=FALSE;

  //pass 2, copy and insert newlines


  for (i=0;i<tlen;i+=xtoffs) {
    xtoffs=mbtowc(&utfsym,&text[i],4); // get next utf8 wchar
    if (!strncmp(text+i,"\n",nlen)) runlen=0; // is a newline (in any encoding)
    else {
      runlen++;
      if (needsnl) {
	memcpy(retstr+req_size,newline,nlen);
	req_size+=nlen;
      }
    }

    if (runlen==maxwidth) {
      if (i<tlen-1 && (strncmp(text+i+1,"\n",nlen))) {
	// needs a newline
	needsnl=TRUE;
	runlen=0;
      }
    }
    else needsnl=FALSE;
    memcpy(retstr+req_size,&utfsym,xtoffs);
    req_size+=xtoffs;
  }

  memset(retstr+req_size,0,1);

  return retstr;
}


void combo_set_popdown_strings (GtkCombo *combo, GList *list) {
  // this avoids an assert in some versions of GTK+ when list==NULL
  GList *empty_list=NULL;
  empty_list=g_list_append (empty_list,(gpointer)"");
  if (list==NULL) gtk_combo_set_popdown_strings (combo,empty_list);
  else {
    g_list_free (empty_list);
    gtk_combo_set_popdown_strings (combo,g_list_copy(list));
  }
}


void get_border_size (GtkWidget *win, gint *bx, gint *by) {
  GdkRectangle rect;
  gint wx,wy;
  gdk_window_get_frame_extents (GDK_WINDOW (win->window),&rect);
  gdk_window_get_origin (GDK_WINDOW (win->window), &wx, &wy);
  *bx=wx-rect.x;
  *by=wy-rect.y;
}



gint hextodec (const gchar *string) {
  int i;
  gint tot=0;
  gchar test[2];

  memset (test+1,0,1);

  for (i=0;i<strlen (string);i++) {
    tot*=16;
    w_memcpy (test,&string[i],1);
    tot+=get_hex_digit (test);
  }
  return tot;
}

gint get_hex_digit (const gchar *c) {
  if (!strcmp (c,"a")||!strcmp (c,"A")) return 10;
  if (!strcmp (c,"b")||!strcmp (c,"B")) return 11;
  if (!strcmp (c,"c")||!strcmp (c,"C")) return 12;
  if (!strcmp (c,"d")||!strcmp (c,"D")) return 13;
  if (!strcmp (c,"e")||!strcmp (c,"E")) return 14;
  if (!strcmp (c,"f")||!strcmp (c,"F")) return 15;
  return(atoi (c));
}



static guint32 fastrand_val;

LIVES_INLINE guint32 fastrand(void)
{
#define rand_a 1073741789L
#define rand_c 32749L

  return (fastrand_val= rand_a * fastrand_val + rand_c);
}

void fastsrand(guint32 seed)
{
  fastrand_val = seed;
}




GdkPixmap*
gdk_pixmap_copy (GdkPixmap *pixmap)
{
  GdkPixmap *pixmap_out;
  GdkGC *gc;
  gint width, height, depth;
  
  if (pixmap==NULL) return NULL;
  
  gdk_drawable_get_size (pixmap, &width, &height);
  depth = gdk_drawable_get_depth (pixmap);
  pixmap_out = gdk_pixmap_new (NULL, width, height, depth);
  gc = gdk_gc_new (pixmap);
  gdk_draw_drawable (pixmap_out, gc, pixmap, 0, 0, 0, 0, width, height);
  g_object_unref (gc);
  return pixmap_out;
}


void set_fg_colour(gint red, gint green, gint blue) {
  GdkColor col;
  col.red=red*255;
  col.green=green*255;
  col.blue=blue*255;
  if (mainw->general_gc==NULL) {
    if (mainw->multitrack==NULL) mainw->general_gc=gdk_gc_new(GDK_DRAWABLE(mainw->LiVES->window));
    else mainw->general_gc=gdk_gc_new(GDK_DRAWABLE(mainw->multitrack->window->window));
  }
  gdk_gc_set_rgb_fg_color(mainw->general_gc,&col);
}


gboolean
label_act_toggle (GtkWidget *widget, GdkEventButton *event, GtkToggleButton *togglebutton) {
  if (!GTK_WIDGET_IS_SENSITIVE(togglebutton)) return FALSE;
  gtk_toggle_button_set_active (togglebutton, !gtk_toggle_button_get_active(togglebutton));
  return FALSE;
}

gboolean
widget_act_toggle (GtkWidget *widget, GtkToggleButton *togglebutton) {
  gtk_toggle_button_set_active (togglebutton, TRUE);
  return FALSE;
}


void gtk_tooltips_copy(GtkWidget *dest, GtkWidget *source) {
  GtkTooltipsData *td=gtk_tooltips_data_get(source);
  if (td==NULL) return;
  gtk_tooltips_set_tip (td->tooltips, dest, td->tip_text, td->tip_private);
}



gchar *text_view_get_text(GtkTextView *textview) {
  GtkTextIter siter,eiter;
  GtkTextBuffer *textbuf=gtk_text_view_get_buffer (textview);
  gtk_text_buffer_get_start_iter(textbuf,&siter);
  gtk_text_buffer_get_end_iter(textbuf,&eiter);

  return gtk_text_buffer_get_text(textbuf,&siter,&eiter,TRUE);
}


void text_view_set_text(GtkTextView *textview, const gchar *text) {
  GtkTextBuffer *textbuf=gtk_text_view_get_buffer (textview);
  gtk_text_buffer_set_text(textbuf,text,-1);
}


gint get_box_child_index (GtkBox *box, GtkWidget *tchild) {
  GList *list=box->children;
  GtkBoxChild *child;
  int i=0;

  while (list!=NULL) {
    child=(GtkBoxChild *)list->data;
    if (child->widget==tchild) return i;
    list=list->next;
    i++;
  }
  return -1;
}



void adjustment_configure(GtkAdjustment *adjustment,
		     gdouble value,
		     gdouble lower,
		     gdouble upper,
		     gdouble step_increment,
		     gdouble page_increment,
		     gdouble page_size) {
  g_object_freeze_notify (G_OBJECT(adjustment));

#ifdef HAVE_GTK_NICE_VERSION
  gtk_adjustment_configure(adjustment,value,lower,upper,step_increment,page_increment,page_size);
  g_object_thaw_notify (G_OBJECT(adjustment));
  return;
#else


  adjustment->upper=upper;
  adjustment->lower=lower;
  adjustment->value=value;
  adjustment->step_increment=step_increment;
  adjustment->page_increment=page_increment;
  adjustment->page_size=page_size;

  g_object_thaw_notify (G_OBJECT(adjustment));
#endif
}


gboolean is_writeable_dir(const gchar *dir) {
  // get free space in bytes for volume containing directory dir
  // return 0 if we cannot create/write to dir

  // dir should be in locale encoding

  gchar *com;
  struct statvfs sbuf;

  if (!g_file_test(dir,G_FILE_TEST_IS_DIR)) {
    com=g_strdup_printf("/bin/mkdir -p \"%s\"",dir);
    lives_system(com,TRUE);
    g_free(com);
    if (!g_file_test(dir,G_FILE_TEST_IS_DIR)) {
      return FALSE;
    }
  }

  // use statvfs to get fs details
  if (statvfs(dir,&sbuf)==-1) return FALSE;
  if (sbuf.f_flag&ST_RDONLY) return FALSE;

  return TRUE;
}




gulong get_fs_free(const char *dir) {
  // get free space in bytes for volume containing directory dir
  // return 0 if we cannot create/write to dir

  // caller should test with is_writeable_dir() first before calling this
  // since 0 is a valid return value

  // dir should be in locale encoding

  gulong bytes=0;
  gboolean must_delete=FALSE;
  struct statvfs sbuf;

  if (!g_file_test(dir,G_FILE_TEST_IS_DIR)) must_delete=TRUE;
  if (!is_writeable_dir(dir)) goto getfserr;

  // use statvfs to get fs details
  if (statvfs(dir,&sbuf)==-1) goto getfserr;
  if (sbuf.f_flag&ST_RDONLY) goto getfserr;

  // result is block size * blocks available
  bytes=sbuf.f_bsize*sbuf.f_bavail;

getfserr:
  if (must_delete) rmdir(dir);

  return bytes;
}




LIVES_INLINE LiVESInterpType get_interp_value(gshort quality) {
  if (quality==PB_QUALITY_HIGH) return LIVES_INTERP_BEST;
  else if (quality==PB_QUALITY_MED) return LIVES_INTERP_NORMAL;
  return LIVES_INTERP_FAST;
}



LIVES_INLINE GList *g_list_move_to_first(GList *list, GList *item) {
  // move item to first in list
  GList *xlist=g_list_remove_link(list,item); // item becomes standalone list
  return g_list_concat(item,xlist); // concat rest of list after item
}


GList *g_list_delete_string(GList *list, char *string) {
  // remove string from list, using strcmp

  GList *xlist=list;
  while (xlist!=NULL) {
    if (!strcmp((char *)xlist->data,string)) {
      if (xlist->prev!=NULL) xlist->prev->next=xlist->next;
      else list=xlist->next;
      if (xlist->next!=NULL) xlist->next->prev=xlist->prev;
      xlist->next=xlist->prev=NULL;
      g_free(xlist->data);
      g_list_free(xlist);
      return list;
    }
    xlist=xlist->next;
  }
  return list;
}


GList *g_list_copy_strings(GList *list) {
  // copy a list, copying the strings too

  GList *xlist=NULL,*olist=list;

  while (olist!=NULL) {
    xlist=g_list_append(xlist,g_strdup((gchar *)olist->data));
    olist=olist->next;
  }

  return xlist;
}




gboolean string_lists_differ(GList *alist, GList *blist) {
  // compare 2 lists of strings and see if they are different (ignoring ordering)
  // for long lists this would be quicker if we sorted the lists first; however this function 
  // is designed to deal with short lists only


  GList *plist;

  if (g_list_length(alist)!=g_list_length(blist)) return TRUE; // check the simple case first

  // run through alist and see if we have a mismatch

  plist=alist;
  while (plist!=NULL) {
    GList *qlist=blist;
    gboolean matched=FALSE;
    while (qlist!=NULL) {
      if (!(strcmp((char *)plist->data,(char *)qlist->data))) {
	matched=TRUE;
	break;
      }
      qlist=qlist->next;
    }
    if (!matched) return TRUE;
    plist=plist->next;
  }

  // since both lists were of the same length, there is no need to check blist

  return FALSE;
}



