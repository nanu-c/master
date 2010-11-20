// audio.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2010
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "audio.h"
#include "events.h"
#include "callbacks.h"
#include "effects-weed.h"
#include "support.h"

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-host.h"
#include "weed/weed-palettes.h"
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-palettes.h"
#endif


// keep first 16 audio_in filesysten handles open - mulitrack only
#define NSTOREDFDS 16
static gchar *storedfnames[NSTOREDFDS];
static int storedfds[NSTOREDFDS];
static gboolean storedfdsset=FALSE;

static void audio_reset_stored_fnames(void) {
  int i;
  for (i=0;i<NSTOREDFDS;i++) {
    storedfnames[i]=NULL;
    storedfds[i]=-1;
  }
  storedfdsset=TRUE;
}



void audio_free_fnames(void) {
  // cleanup stored filehandles after playback/fade/render
  
  int i;

  if (!storedfdsset) return;

  for (i=0;i<NSTOREDFDS;i++) {
    if (storedfnames!=NULL) {
      g_free(storedfnames[i]);
      storedfnames[i]=NULL;
      if (storedfds[i]>-1) close(storedfds[i]);
      storedfds[i]=-1;
    }
  }
}




inline void sample_silence_dS (float *dst, unsigned long nsamples) {
  memset(dst,0,nsamples*sizeof(float));
}


void sample_move_d8_d16(short *dst, unsigned char *src,
			unsigned long nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign) {
  // convert 8 bit audio to 16 bit audio

  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount;
  unsigned char *ptr;
  unsigned char *src_end;

  // take care of rounding errors
  src_end=src+tbytes-nSrcChannels;

  if(!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  while(nsamples--) {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;
    ccount=0;

    /* loop until all of our destination channels are filled */
    while(nDstCount) {
      nSrcCount--;
      nDstCount--;

      ptr=src+ccount+src_offset_i;
      ptr=ptr>src?(ptr<(src_end+ccount)?ptr:(src_end+ccount)):src;
      
      if (!swap_sign) *(dst++) = *(ptr)<<8;
      else if (swap_sign==SWAP_U_TO_S) *(dst++)=((short)(*(ptr))-128)<<8;
      else *((unsigned short *)(dst++))=((short)(*(ptr))+128)<<8;
      ccount++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if(!nSrcCount && nDstCount) {
	ccount=0;
	nSrcCount = nSrcChannels;
      }
    }
    
    /* advance the the position */
    src_offset_i=(int)(src_offset_f+=scale)*nSrcChannels;
  }
}



/* convert from any number of source channels to any number of destination channels */
void sample_move_d16_d16(short *dst, short *src,
			 unsigned long nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_endian, int swap_sign) {
  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount=0;
  short *ptr;
  short *src_end;

  if(!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  // take care of rounding errors
  src_end=src+tbytes/sizeof(short)-nSrcChannels;

  while (nsamples--) {

    if ((nSrcCount = nSrcChannels)==(nDstCount = nDstChannels)&&!swap_endian&&!swap_sign) {
      // same number of channels

      ptr=src+src_offset_i;
      ptr=ptr>src?(ptr<src_end?ptr:src_end):src;

      w_memcpy(dst,ptr,nSrcChannels*sizeof(short));
      dst+=nDstCount;
    } 
    else {
      ccount=0;

      /* loop until all of our destination channels are filled */
      while (nDstCount) {
	nSrcCount--;
	nDstCount--;
	
	ptr=src+ccount+src_offset_i;
	ptr=ptr>src?(ptr<(src_end+ccount)?ptr:(src_end+ccount)):src;

	/* copy the data over */
	if (!swap_endian) {
	  if (!swap_sign) *(dst++) = *ptr;
	  else if (swap_sign==SWAP_S_TO_U) *((uint16_t *)dst++) = (uint16_t)(*ptr+SAMPLE_MAX_16BITI);
	  else *(dst++)=*ptr-SAMPLE_MAX_16BITI;
	}
	else if (swap_endian==SWAP_X_TO_L) {
	  if (!swap_sign) *(dst++)=(((*ptr)&0x00FF)<<8)+((*ptr)>>8);
	  else if (swap_sign==SWAP_S_TO_U) *((uint16_t *)dst++)=(uint16_t)(((*ptr&0x00FF)<<8)+(*ptr>>8)+SAMPLE_MAX_16BITI);
	  else *(dst++)=((*ptr&0x00FF)<<8)+(*ptr>>8)-SAMPLE_MAX_16BITI;
	}
	else {
	  if (!swap_sign) *(dst++)=(((*ptr)&0x00FF)<<8)+((*ptr)>>8);
	  else if (swap_sign==SWAP_S_TO_U) *((uint16_t *)dst++)=(uint16_t)(((((uint16_t)(*ptr+SAMPLE_MAX_16BITI))&0x00FF)<<8)+(((uint16_t)(*ptr+SAMPLE_MAX_16BITI))>>8));
	  else *(dst++)=((((int16_t)(*ptr-SAMPLE_MAX_16BITI))&0x00FF)<<8)+(((int16_t)(*ptr-SAMPLE_MAX_16BITI))>>8);
	}

	ccount++;
	
	/* if we ran out of source channels but not destination channels */
	/* then start the src channels back where we were */
	if(!nSrcCount && nDstCount) {
	  ccount=0;
	  nSrcCount = nSrcChannels;
	}
      }
    }
    /* advance the the position */
    src_offset_i=(int)(src_offset_f+=scale)*nSrcChannels;
  }
}


/* convert from any number of source channels to any number of destination channels - 8 bit output */
void sample_move_d16_d8(uint8_t *dst, short *src,
			 unsigned long nsamples, size_t tbytes, float scale, int nDstChannels, int nSrcChannels, int swap_sign) {
  register int nSrcCount, nDstCount;
  register float src_offset_f=0.f;
  register int src_offset_i=0;
  register int ccount=0;
  short *ptr;
  short *src_end;

  if(!nSrcChannels) return;

  if (scale<0.f) {
    src_offset_f=((float)(nsamples)*(-scale)-1.f);
    src_offset_i=(int)src_offset_f*nSrcChannels;
  }

  src_end=src+tbytes/sizeof(short)-nSrcChannels;

  while (nsamples--) {
    nSrcCount = nSrcChannels;
    nDstCount = nDstChannels;

    ccount=0;
    
    /* loop until all of our destination channels are filled */
    while(nDstCount) {
      nSrcCount--;
      nDstCount--;
      
      ptr=src+ccount+src_offset_i;
      ptr=ptr>src?(ptr<(src_end+ccount)?ptr:src_end+ccount):src;

      /* copy the data over */
      if (!swap_sign) *(dst++) = (*ptr>>8);
      else if (swap_sign==SWAP_S_TO_U) *(dst++) = (uint8_t)((int8_t)(*ptr>>8)+128);
      else *((int8_t *)dst++) = (int8_t)((uint8_t)(*ptr>>8)-128);
      ccount++;

      /* if we ran out of source channels but not destination channels */
      /* then start the src channels back where we were */
      if(!nSrcCount && nDstCount) {
	ccount=0;
	  nSrcCount = nSrcChannels;
      }
    }
    
    /* advance the the position */
    src_offset_i=(int)(src_offset_f+=scale)*nSrcChannels;
  }
}



void sample_move_d16_float (float *dst, short *src, unsigned long nsamples, unsigned long src_skip, int is_unsigned, float vol) {
  // convert 16 bit audio to float audio


  register float svolp,svoln;

#ifdef ENABLE_OIL
  float val=0.;  // set a value to stop valgrind complaining
  double xn,xp,xa;
  double y=0.f;
#else
  register float val;
  register short valss;
#endif

  if (vol==0.) vol=0.0000001f;
  svolp=SAMPLE_MAX_16BIT_P/vol;
  svoln=SAMPLE_MAX_16BIT_N/vol;

#ifdef ENABLE_OIL
  xp=1./svolp;
  xn=1./svoln;
  xa=2.*vol/(SAMPLE_MAX_16BIT_P+SAMPLE_MAX_16BIT_N);
#endif

  while (nsamples--) {

    if (!is_unsigned) {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_s16(&val,src,1,&y,val>0?&xp:&xn);
#else
      if ((val = (float)((float)(*src) / (*src>0?svolp:svoln) ))>1.0f) val=1.0f;
      else if (val<-1.0f) val=-1.0f;
#endif
    }
    else {
#ifdef ENABLE_OIL
      oil_scaleconv_f32_u16(&val,(unsigned short *)src,1,&y,&xa);
      val-=vol;
#else
      valss=(unsigned short)*src-SAMPLE_MAX_16BITI;
      if ((val = (float)((float)(valss) / (valss>0?svolp:svoln) ))>1.0f) val=1.0f;
      else if (val<-1.0f) val=-1.0f;
#endif
    }
    *(dst++)=val;
    src += src_skip;
  }

}









long sample_move_float_int(void *holding_buff, float **float_buffer, int nsamps, float scale, int chans, int asamps, int usigned, gboolean swap_endian, float vol) {
  // convert float samples back to int
  long frames_out=0l;
  register int i;
  register int offs=0,coffs=0;
  register float coffs_f=0.f;
  short *hbuffs=(short *)holding_buff;
  unsigned short *hbuffu=(unsigned short *)holding_buff;
  unsigned char *hbuffc=(guchar *)holding_buff;

#ifdef ENABLE_OIL
  double x=(SAMPLE_MAX_16BIT_N+SAMPLE_MAX_16BIT_P)/2.;
  double y=0.f;
  short val=0;
  unsigned short valu=0;
#else
  register short val;
  register unsigned short valu=0;
  register float valf;
#endif

  while ((nsamps-coffs)>0) {
    frames_out++;
    for (i=0;i<chans;i++) {
#ifdef ENABLE_OIL
      oil_scaleconv_s16_f32(&val,float_buffer[i]+coffs,1,&y,&x);
#else
      valf=*(float_buffer[i]+coffs);
      val=(short)(valf*(valf>0.?SAMPLE_MAX_16BIT_P:SAMPLE_MAX_16BIT_N));
#endif
      if (usigned) valu=(val+SAMPLE_MAX_16BITI)*vol;
      val*=vol;

      if (asamps==16) {
	if (!swap_endian) {
	  if (usigned) *(hbuffu+offs)=valu;
	  else *(hbuffs+offs)=val;
	}
	else {
	  if (usigned) {
	    *(hbuffc+offs)=valu&0x00FF;
	    *(hbuffc+(++offs))=(valu&0xFF00)>>8;
	  }
	  else {
	    *(hbuffc+offs)=val&0x00FF;
	    *(hbuffc+(++offs))=(val&0xFF00)>>8;
	  }
	}
      }
      else {
	*(hbuffc+offs)=(guchar)((float)val/256.);
      }
      offs++;
    }
    coffs=(gint)(coffs_f+=scale);
  }
  return frames_out;
}




// play from memory buffer

long sample_move_abuf_float (float **obuf, int nchans, int nsamps, int out_arate, float vol) {

  int samples_out=0;

#ifdef ENABLE_JACK

  int samps=0;

  lives_audio_buf_t *abuf;
  int in_arate;
  register size_t offs=0,ioffs,xchan;

  register float src_offset_f=0.f;
  register int src_offset_i=0;

  register int i,j;

  register double scale;

  size_t curval;

  pthread_mutex_lock(&mainw->abuf_mutex);
  if (mainw->jackd->read_abuf==-1) {
    pthread_mutex_unlock(&mainw->abuf_mutex);
    return 0;
  }
  abuf=mainw->jackd->abufs[mainw->jackd->read_abuf];
  in_arate=abuf->arate;
  pthread_mutex_unlock(&mainw->abuf_mutex);
  scale=(double)in_arate/(double)out_arate;

  while (nsamps>0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    if (mainw->jackd->read_abuf==-1) {
      pthread_mutex_unlock(&mainw->abuf_mutex);
      return 0;
    }

    ioffs=abuf->start_sample;
    pthread_mutex_unlock(&mainw->abuf_mutex);
    samps=0;

    src_offset_i=0;
    src_offset_f=0.;

    for (i=0;i<nsamps;i++) {
      // process each sample

      if ((curval=ioffs+src_offset_i)>=abuf->samples_filled) {
	// current buffer is consumed
	break;
      }
      xchan=0;
      for (j=0;j<nchans;j++) {
	// copy channel by channel (de-interleave)
	pthread_mutex_lock(&mainw->abuf_mutex);
	if (mainw->jackd->read_abuf<0) {
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	  return samples_out;
	}
	if (xchan>=abuf->out_achans) xchan=0; 
	obuf[j][offs+i]=abuf->bufferf[xchan][curval]*vol;
	pthread_mutex_unlock(&mainw->abuf_mutex);
	xchan++;
      }
      // resample on the fly
      src_offset_i=(int)(src_offset_f+=scale);
      samps++;
      samples_out++;
    }
    
    abuf->start_sample=ioffs+src_offset_i;
    nsamps-=samps;
    offs+=samps;

    if (nsamps>0) {
      // buffer was consumed, move on to next buffer

      pthread_mutex_lock(&mainw->abuf_mutex);
      // request main thread to fill another buffer
      mainw->abufs_to_fill++;
      if (mainw->jackd->read_abuf<0) {
	// playback ended while we were processing
	pthread_mutex_unlock(&mainw->abuf_mutex);
	return samples_out;
      }

      mainw->jackd->read_abuf++;

      if (mainw->jackd->read_abuf>=prefs->num_rtaudiobufs) mainw->jackd->read_abuf=0;

      abuf=mainw->jackd->abufs[mainw->jackd->read_abuf];
      
      pthread_mutex_unlock(&mainw->abuf_mutex);
    }

  }
#endif

  return samples_out;
}




long sample_move_abuf_int16 (short *obuf, int nchans, int nsamps, int out_arate) {

  int samples_out=0;

#ifdef HAVE_PULSE_AUDIO

  int samps=0;

  lives_audio_buf_t *abuf;
  int in_arate,nsampsx;
  register size_t offs=0,ioffs,xchan;

  register float src_offset_f=0.f;
  register int src_offset_i=0;

  register int i,j;

  register double scale;
  size_t curval;

  pthread_mutex_lock(&mainw->abuf_mutex);
  if (mainw->pulsed->read_abuf==-1) {
    pthread_mutex_unlock(&mainw->abuf_mutex);
    return 0;
  }

  abuf=mainw->pulsed->abufs[mainw->pulsed->read_abuf];
  in_arate=abuf->arate;
  pthread_mutex_unlock(&mainw->abuf_mutex);
  scale=(double)in_arate/(double)out_arate;

  while (nsamps>0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    if (mainw->pulsed->read_abuf==-1) {
      pthread_mutex_unlock(&mainw->abuf_mutex);
      return 0;
    }

    ioffs=abuf->start_sample;
    pthread_mutex_unlock(&mainw->abuf_mutex);
    samps=0;

    src_offset_i=0;
    src_offset_f=0.;
    nsampsx=nsamps*nchans;

    for (i=0;i<nsampsx;i+=nchans) {
      // process each sample

      if ((curval=ioffs+src_offset_i)>=abuf->samples_filled) {
	// current buffer is drained
	break;
      }
      xchan=0;
      curval*=abuf->out_achans;
      for (j=0;j<nchans;j++) {
	pthread_mutex_lock(&mainw->abuf_mutex);
	if (mainw->pulsed->read_abuf<0) {
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	  return samps;
	}
	if (xchan>=abuf->out_achans) xchan=0;
	obuf[(offs++)]=abuf->buffer16[0][curval+xchan];
	pthread_mutex_unlock(&mainw->abuf_mutex);
	xchan++;
      }
      src_offset_i=(int)(src_offset_f+=scale);
      samps++;
    }
    
    abuf->start_sample=ioffs+src_offset_i;
    nsamps-=samps;
    samples_out+=samps;

    if (nsamps>0) {
      // buffer was drained, move on to next buffer

      pthread_mutex_lock(&mainw->abuf_mutex);
      // request main thread to fill another buffer
      mainw->abufs_to_fill++;

      if (mainw->pulsed->read_abuf<0) {
	// playback ended while we were processing
	pthread_mutex_unlock(&mainw->abuf_mutex);
	return samples_out;
      }

      mainw->pulsed->read_abuf++;

      if (mainw->pulsed->read_abuf>=prefs->num_rtaudiobufs) mainw->pulsed->read_abuf=0;

      abuf=mainw->pulsed->abufs[mainw->pulsed->read_abuf];
      
      pthread_mutex_unlock(&mainw->abuf_mutex);
    }

  }
#endif

  return samples_out;
}





/// copy a memory chunk into an audio buffer, applying overall volume control


static size_t chunk_to_float_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int chans=abuf->out_achans;
  register size_t offs=abuf->samples_filled;
  register int i;

  for (i=0;i<chans;i++) {
    w_memcpy(&(abuf->bufferf[i][offs]),float_buffer[i],nsamps*sizeof(float));
  }
  return (size_t)nsamps;
}


// for pulse audio we use int16, and let it apply the volume

static size_t chunk_to_int16_abuf(lives_audio_buf_t *abuf, float **float_buffer, int nsamps) {
  int frames_out=0;
  int chans=abuf->out_achans;
  register size_t offs=abuf->samples_filled*chans;
  register int i;
  register float valf;

  while (frames_out<nsamps) {
    for (i=0;i<chans;i++) {
      valf=float_buffer[i][frames_out];
      abuf->buffer16[0][offs+i]=(short)(valf*(valf>0.?SAMPLE_MAX_16BIT_P:SAMPLE_MAX_16BIT_N));
    }
    frames_out++;
    offs+=chans;
  }
  return (size_t)frames_out;
}





//#define DEBUG_ARENDER

static void pad_with_silence(int out_fd, off64_t oins_size, long ins_size, int asamps, int aunsigned, gboolean big_endian) {
  // fill to ins_pt with zeros (or 0x80.. for unsigned)
  guchar *zero_buff;
  size_t sblocksize=SILENCE_BLOCK_SIZE;
  gint sbytes=ins_size-oins_size;
  register int i;

#ifdef DEBUG_ARENDER
  g_print("sbytes is %d\n",sbytes);
#endif
  if (sbytes>0) {
    lseek64(out_fd,oins_size,SEEK_SET);
    if (!aunsigned) zero_buff=g_malloc0(SILENCE_BLOCK_SIZE);
    else {
      zero_buff=g_malloc(SILENCE_BLOCK_SIZE);
      if (asamps==1) memset(zero_buff,0x80,SILENCE_BLOCK_SIZE);
      else {
	for (i=0;i<SILENCE_BLOCK_SIZE;i+=2) {
	  if (big_endian) {
	    memset(zero_buff+i,0x80,1);
	    memset(zero_buff+i+1,0x00,1);
	  }
	  else {
	    memset(zero_buff+i,0x00,1);
	    memset(zero_buff+i+1,0x80,1);
	  }
	}
      }
    }

    for (i=0;i<sbytes;i+=SILENCE_BLOCK_SIZE) {
      if (sbytes-i<SILENCE_BLOCK_SIZE) sblocksize=sbytes-i;
      dummyvar=write (out_fd,zero_buff,sblocksize);
    }
    g_free(zero_buff);
  }
  else if (sbytes<=0) {
    lseek64(out_fd,oins_size+sbytes,SEEK_SET);
  }
}




long render_audio_segment(gint nfiles, gint *from_files, gint to_file, gdouble *avels, gdouble *fromtime, weed_timecode_t tc_start, weed_timecode_t tc_end, gdouble *chvol, gdouble opvol_start, gdouble opvol_end, lives_audio_buf_t *obuf) {
  // called during multitrack rendering to create the actual audio file
  // (or in-memory buffer for preview playback in multitrack)

  // also used for fade-in/fade-out in the clip editor (opvol_start, opvol_end)

  // in multitrack, chvol is taken from the audio mixer; opvol is always 1.

  // what we will do here:
  // calculate our target samples (= period * out_rate)

  // calculate how many in_samples for each track (= period * in_rate / ABS (vel) )

  // read in the relevant number of samples for each track and convert to float

  // write this into our float buffers (1 buffer per channel per track)

  // we then send small chunks at a time to our audio volume/pan effect; this is to allow for parameter interpolation

  // the small chunks are processed and mixed, converted from float to int, and then written to the outfile

  // if obuf != NULL we write to obuf instead


  // TODO - allow MAX_AUDIO_MEM to be configurable; currently this is fixed at 8 MB
  // 16 or 32 may be a more sensible default for realtime previewing


  // return (audio) frames rendered

  file *outfile=to_file>-1?mainw->files[to_file]:NULL;


  size_t tbytes;
  guchar *in_buff;

  gint out_asamps=to_file>-1?outfile->asampsize/8:0;
  gint out_achans=to_file>-1?outfile->achans:obuf->out_achans;
  gint out_arate=to_file>-1?outfile->arate:obuf->arate;
  gint out_unsigned=to_file>-1?outfile->signed_endian&AFORM_UNSIGNED:0;
  gint out_bendian=to_file>-1?outfile->signed_endian&AFORM_BIG_ENDIAN:0;

  short *holding_buff;
  float *float_buffer[out_achans*nfiles];

  float *chunk_float_buffer[out_achans*nfiles];

  int c,x;
  register int i,j;
  size_t bytes_read;
  int in_fd[nfiles],out_fd=-1;
  gulong nframes;
  gboolean in_reverse_endian[nfiles],out_reverse_endian=FALSE;
  off64_t seekstart[nfiles];
  gchar *infilename,*outfilename;
  weed_timecode_t tc=tc_start;
  gdouble ins_pt=tc/U_SEC;
  long ins_size=0l,cur_size;
  gdouble time=0.;
  gdouble opvol=opvol_start;
  long frames_out=0;

  int track;

  gint in_asamps[nfiles];
  gint in_achans[nfiles];
  gint in_arate[nfiles];
  gint in_unsigned[nfiles];
  gint in_bendian;

  gboolean is_silent[nfiles];
  gint first_nonsilent=-1;

  long tsamples=((tc_end-tc_start)/U_SEC*out_arate+.5);

  long blocksize,zsamples,xsamples;

  void *finish_buff;

  weed_plant_t *shortcut=NULL;

  size_t max_aud_mem,bytes_to_read,aud_buffer;
  gint max_segments;
  gdouble zavel,zavel_max=0.;

  long tot_frames=0l;

  int render_block_size=RENDER_BLOCK_SIZE;

  if (out_achans==0) return 0l;

  if (!storedfdsset) audio_reset_stored_fnames();

  if (mainw->multitrack==NULL) render_block_size*=100;

  if (to_file>-1) {
    // prepare outfile stuff
    outfilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,outfile->handle);
#ifdef DEBUG_ARENDER
    g_print("writing to %s\n",outfilename);
#endif
    out_fd=open(outfilename,O_WRONLY|O_CREAT|O_SYNC,S_IRUSR|S_IWUSR);
    g_free(outfilename);
    
    cur_size=get_file_size(out_fd);

    ins_pt*=out_achans*out_arate*out_asamps;
    ins_size=((long)(ins_pt/out_achans/out_asamps+.5))*out_achans*out_asamps;

    if ((!out_bendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(out_bendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) out_reverse_endian=TRUE;
    else out_reverse_endian=FALSE;
    
    // fill to ins_pt with zeros
    pad_with_silence(out_fd,cur_size,ins_size,out_asamps,out_unsigned,out_bendian);

  }
  else {

    if (mainw->event_list!=NULL) cfile->aseek_pos=fromtime[0];

    tc_end-=tc_start;
    tc_start=0;

    if (tsamples>obuf->samp_space-obuf->samples_filled) tsamples=obuf->samp_space-obuf->samples_filled;

  }

#ifdef DEBUG_ARENDER
  g_print("here %d %lld %lld %d\n",nfiles,tc_start,tc_end,to_file);
#endif

  for (track=0;track<nfiles;track++) {
    // prepare infile stuff
    file *infile;

#ifdef DEBUG_ARENDER
    g_print(" track %d %d %.4f %.4f\n",track,from_files[track],fromtime[track],avels[track]);
#endif

    if (avels[track]==0.) {
      is_silent[track]=TRUE;
      continue;
    }

    is_silent[track]=FALSE;
    
    infile=mainw->files[from_files[track]];

    in_asamps[track]=infile->asampsize/8;
    in_achans[track]=infile->achans;
    in_arate[track]=infile->arate;
    in_unsigned[track]=infile->signed_endian&AFORM_UNSIGNED;
    in_bendian=infile->signed_endian&AFORM_BIG_ENDIAN;

    if (G_UNLIKELY(in_achans[track]==0)) is_silent[track]=TRUE;
    else {
      if ((!in_bendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(in_bendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) in_reverse_endian[track]=TRUE;
      else in_reverse_endian[track]=FALSE;
      
      seekstart[track]=(off64_t)(fromtime[track]*in_arate[track])*in_achans[track]*in_asamps[track];
      seekstart[track]=((off64_t)(seekstart[track]/in_achans[track]/(in_asamps[track])))*in_achans[track]*in_asamps[track];
      
      zavel=avels[track]*(gdouble)in_arate[track]/(gdouble)out_arate*in_asamps[track]*in_achans[track]/sizeof(float);
      if (ABS(zavel)>zavel_max) zavel_max=ABS(zavel);
      
      infilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,infile->handle);


      // try to speed up access by keeping some files open
      if (track<NSTOREDFDS&&storedfnames[track]!=NULL&&!strcmp(infilename,storedfnames[track])) {
	in_fd[track]=storedfds[track];
      }
      else {
	if (track<NSTOREDFDS&&storedfds[track]>-1) close(storedfds[track]);
	in_fd[track]=open(infilename,O_RDONLY);
	
	if (track<NSTOREDFDS) {
	  storedfds[track]=in_fd[track];
	  storedfnames[track]=g_strdup(infilename);
	}
      }

      lseek64(in_fd[track],seekstart[track],SEEK_SET);
      
      g_free(infilename);
    }
  }

  for (track=0;track<nfiles;track++) {
    if (!is_silent[track]) {
      first_nonsilent=track;
      break;
    }
  }

  if (first_nonsilent==-1) {
    // all in tracks are empty
    // output silence
    if (to_file>-1) {
      long oins_size=ins_size;
      ins_pt=tc_end/U_SEC;
      ins_pt*=out_achans*out_arate*out_asamps;
      ins_size=((long)(ins_pt/out_achans/out_asamps)+.5)*out_achans*out_asamps;
      pad_with_silence(out_fd,oins_size,ins_size,out_asamps,out_unsigned,out_bendian);
      //sync();
      close (out_fd);
    }
    else {
      for (i=0;i<out_achans;i++) {
	for (j=obuf->samples_filled;j<obuf->samples_filled+tsamples;j++) {
	  if (prefs->audio_player==AUD_PLAYER_JACK) {
	    obuf->bufferf[i][j]=0.;
	  }
	  else {
	    if (!out_unsigned) obuf->buffer16[0][j*out_achans+i]=0;
	    else {
	      if (out_bendian) obuf->buffer16[0][j*out_achans+i]=0x8000;
	      else obuf->buffer16[0][j*out_achans+i]=0x80;
	    }
	  }
	}
      }
      obuf->samples_filled+=tsamples;
    }

    return tsamples;
  }

  // we don't want to use more than MAX_AUDIO_MEM bytes
  // (numbers will be much larger than examples given)

  max_aud_mem=MAX_AUDIO_MEM/(1.5+zavel_max); // allow for size of holding_buff and in_buff

  max_aud_mem=(max_aud_mem>>7)<<7; // round to a multiple of 128 

  max_aud_mem=max_aud_mem/out_achans/nfiles; // max mem per channel/track

  bytes_to_read=tsamples*(sizeof(float)); // eg. 120 (20 samples)

  max_segments=(int)((gdouble)bytes_to_read/(gdouble)max_aud_mem+1.); // max segments (rounded up) [e.g ceil(120/45)==3]
  aud_buffer=bytes_to_read/max_segments;  // estimate of buffer size (e.g. 120/3 = 40)

  zsamples=(int)(aud_buffer/sizeof(float)+.5); // ensure whole number of samples (e.g 40 == 10 samples)

  xsamples=zsamples+(tsamples-(max_segments*zsamples)); // e.g 30 - 3 * 10 == 0

  holding_buff=(short *)g_malloc(xsamples*sizeof(short)*out_achans);
  
  for (i=0;i<out_achans*nfiles;i++) {
    float_buffer[i]=g_malloc(xsamples*sizeof(float));
  }

  finish_buff=g_malloc(render_block_size*out_achans*out_asamps);

  for (i=0;i<nfiles;i++) {
    for (c=0;c<out_achans;c++) {
      chunk_float_buffer[i*out_achans+c]=g_malloc(render_block_size*sizeof(float));
    }
  }

#ifdef DEBUG_ARENDER
  g_print("  rendering %ld samples\n",tsamples);
#endif

  while (tsamples>0) {
    tsamples-=xsamples;

    for (track=0;track<nfiles;track++) {
      if (is_silent[track]) {
	// zero float_buff
	for(c=0;c<out_achans;c++) {
	  for (x=0;x<xsamples;x++) {
	    float_buffer[track*out_achans+c][x]=0.;
	  }
	}
	continue;
      }

      // calculate tbytes for xsamples

      zavel=avels[track]*(gdouble)in_arate[track]/(gdouble)out_arate;

      tbytes=(gint)((gdouble)xsamples*ABS(zavel)+((gdouble)fastrand()/(gdouble)G_MAXUINT32))*in_asamps[track]*in_achans[track];

      in_buff=g_malloc(tbytes);

      if (zavel<0.) lseek64(in_fd[track],seekstart[track]-tbytes,SEEK_SET);

      bytes_read=read(in_fd[track],in_buff,tbytes);
      //g_print("read %ld bytes\n",bytes_read);

      if (zavel<0.) seekstart[track]-=bytes_read;

      if (bytes_read<tbytes) {
	if (zavel>0) memset(in_buff+bytes_read,0,tbytes-bytes_read);
	else {
	  memmove(in_buff+tbytes-bytes_read,in_buff,bytes_read);
	  memset(in_buff,0,tbytes-bytes_read);
	}
      }

      nframes=(tbytes/(in_asamps[track])/in_achans[track]/ABS(zavel)+.001);

      // convert to float
      if (in_asamps[track]==1) {
	sample_move_d8_d16 (holding_buff,(guchar *)in_buff,nframes,tbytes,zavel,out_achans,in_achans[track],0);
      }
      else {
	sample_move_d16_d16(holding_buff,(short*)in_buff,nframes,tbytes,zavel,out_achans,in_achans[track],in_reverse_endian[track]?SWAP_X_TO_L:0,0);
      }

      g_free(in_buff);

      for(c=0;c<out_achans;c++) {
	sample_move_d16_float(float_buffer[c+track*out_achans],holding_buff+c,nframes,out_achans,in_unsigned[track],chvol[track]);
      }
    }

    // now we send small chunks at a time to the audio vol/pan effect
    shortcut=NULL;
    blocksize=render_block_size;

    for (i=0;i<xsamples;i+=render_block_size) {
      if (i+render_block_size>xsamples) blocksize=xsamples-i;

      for (track=0;track<nfiles;track++) {
	for (c=0;c<out_achans;c++) {
	  //g_print("xvals %.4f\n",*(float_buffer[track*out_achans+c]+i));
	  w_memcpy(chunk_float_buffer[track*out_achans+c],float_buffer[track*out_achans+c]+i,blocksize*sizeof(float));
	}
      }

      // apply audio filter(s)
      if (mainw->multitrack!=NULL) {
	// we work out the "visibility" of each track at tc
	gdouble *vis=get_track_visibility_at_tc(mainw->multitrack->event_list,nfiles,mainw->multitrack->opts.back_audio_tracks,tc,&shortcut,mainw->multitrack->opts.audio_bleedthru);
	
	// locate the master volume parameter, and multiply all values by vis[track]
	weed_apply_audio_effects(mainw->filter_map,chunk_float_buffer,mainw->multitrack->opts.back_audio_tracks,out_achans,blocksize,out_arate,tc,vis);
	
	g_free(vis);
      }
      
      if (to_file>-1) {
	// output to file
	// convert back to int; use out_scale of 1., since we did our resampling in sample_move_*_d16
	frames_out=sample_move_float_int((void *)finish_buff,chunk_float_buffer,blocksize,1.,out_achans,out_asamps*8,out_unsigned,out_reverse_endian,opvol);
	dummyvar=write (out_fd,finish_buff,frames_out*out_asamps*out_achans);
#ifdef DEBUG_ARENDER
	g_print(".");
#endif
      }
      else {
	// output to memory buffer
	if (prefs->audio_player==AUD_PLAYER_JACK) {
	  frames_out=chunk_to_float_abuf(obuf,chunk_float_buffer,blocksize);
	}
	else {
	  frames_out=chunk_to_int16_abuf(obuf,chunk_float_buffer,blocksize);
	}
	obuf->samples_filled+=frames_out;
      }

      if (mainw->multitrack==NULL) {
	time+=(gdouble)frames_out/(gdouble)out_arate;
	opvol=opvol_start+(opvol_end-opvol_start)*(time/((tc_end-tc_start)/U_SEC));
      }
      else tc+=(gdouble)blocksize/(gdouble)out_arate*U_SEC;
    }
    xsamples=zsamples;

    tot_frames+=frames_out;
  }

  for (i=0;i<out_achans*nfiles;i++) {
    if (float_buffer[i]!=NULL) g_free(float_buffer[i]);
  }

  if (finish_buff!=NULL) g_free(finish_buff);
  if (holding_buff!=NULL) g_free(holding_buff);

  // close files
  for (track=0;track<nfiles;track++) {
    if (!is_silent[track]) {
      if (track>=NSTOREDFDS) close (in_fd[track]);
    }
    for (c=0;c<out_achans;c++) {
      if (chunk_float_buffer[track*out_achans+c]!=NULL) g_free(chunk_float_buffer[track*out_achans+c]);
    }
  }

  if (to_file>-1) {
#ifdef DEBUG_ARENDER
    g_print("fs is %ld\n",get_file_size(out_fd));
#endif
    close (out_fd);
  }

  return tot_frames;
}


inline void aud_fade(gint fileno, gdouble startt, gdouble endt, gdouble startv, gdouble endv) {
  gdouble vel=1.,vol=1.;
  render_audio_segment(1,&fileno,fileno,&vel,&startt,startt*U_SECL,endt*U_SECL,&vol,startv,endv,NULL);
}



#ifdef ENABLE_JACK
void jack_rec_audio_to_clip(gint fileno, gint old_file, gshort rec_type) {
  // open audio file for writing
  file *outfile=mainw->files[fileno];
  gchar *outfilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,outfile->handle);
  gint out_bendian=outfile->signed_endian&AFORM_BIG_ENDIAN;

  mainw->aud_rec_fd=open(outfilename,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
  g_free(outfilename);

  mainw->jackd_read=jack_get_driver(0,FALSE);
  mainw->jackd_read->playing_file=fileno;
  mainw->jackd_read->frames_written=0;

  if ((!out_bendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(out_bendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) mainw->jackd_read->reverse_endian=TRUE;
  else mainw->jackd_read->reverse_endian=FALSE;

  // start jack recording
  jack_open_device_read(mainw->jackd_read);
  jack_read_driver_activate(mainw->jackd_read);

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (rec_type==RECA_WINDOW_GRAB) return;

  mainw->cancelled=CANCEL_NONE;
  mainw->cancel_type=CANCEL_SOFT;
  // show countdown/stop dialog
  mainw->suppress_dprint=FALSE;
  d_print(_("Recording audio..."));
  mainw->suppress_dprint=TRUE;
  if (rec_type==RECA_NEW_CLIP) {
    do_auto_dialog(_("Recording audio"),1);
  }
  else {
    gint current_file=mainw->current_file;
    mainw->current_file=old_file;
    on_playsel_activate(NULL,NULL);
    mainw->current_file=current_file;
  }
  jack_rec_audio_end();
}



void jack_rec_audio_end(void) {
  // recording ended

  // stop recording
  jack_close_device(mainw->jackd_read);
  mainw->jackd_read=NULL;

  // close file
  close(mainw->aud_rec_fd);
  mainw->aud_rec_fd=-1;
  mainw->cancel_type=CANCEL_KILL;
}

#endif




#ifdef HAVE_PULSE_AUDIO
void pulse_rec_audio_to_clip(gint fileno, gint old_file, gshort rec_type) {
  // open audio file for writing
  file *outfile=mainw->files[fileno];
  gchar *outfilename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,outfile->handle);
  gint out_bendian=outfile->signed_endian&AFORM_BIG_ENDIAN;

  mainw->aud_rec_fd=open(outfilename,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
  g_free(outfilename);

  mainw->pulsed_read=pulse_get_driver(FALSE);
  mainw->pulsed_read->playing_file=fileno;
  mainw->pulsed_read->frames_written=0;

  if ((!out_bendian&&(G_BYTE_ORDER==G_BIG_ENDIAN))||(out_bendian&&(G_BYTE_ORDER==G_LITTLE_ENDIAN))) mainw->pulsed_read->reverse_endian=TRUE;
  else mainw->pulsed_read->reverse_endian=FALSE;

  // start pulse recording
  pulse_driver_activate(mainw->pulsed_read);

  // in grab window mode, just return, we will call rec_audio_end on playback end
  if (rec_type==RECA_WINDOW_GRAB) return;

  mainw->cancelled=CANCEL_NONE;
  mainw->cancel_type=CANCEL_SOFT;
  // show countdown/stop dialog
  mainw->suppress_dprint=FALSE;
  d_print(_("Recording audio..."));
  mainw->suppress_dprint=TRUE;
  if (rec_type==RECA_NEW_CLIP) do_auto_dialog(_("Recording audio"),1);
  else {
    gint current_file=mainw->current_file;
    mainw->current_file=old_file;
    on_playsel_activate(NULL,NULL);
    mainw->current_file=current_file;
  }
  pulse_rec_audio_end();
}



void pulse_rec_audio_end(void) {
  // recording ended

  // stop recording

  pa_threaded_mainloop_lock(mainw->pulsed->mloop);
  pulse_flush_read_data(mainw->pulsed_read,0,NULL);
  pulse_close_client(mainw->pulsed_read);
  pa_threaded_mainloop_unlock(mainw->pulsed->mloop);

  mainw->pulsed_read=NULL;

  // close file
  close(mainw->aud_rec_fd);
  mainw->aud_rec_fd=-1;
  mainw->cancel_type=CANCEL_KILL;
}

#endif




/////////////////////////////////////////////////////////////////

// playback via memory buffers (e.g. in multitrack)




////////////////////////////////////////////////////////////////


static lives_audio_track_state_t *resize_audstate(lives_audio_track_state_t *ostate, int nostate, int nstate) {
  // increase the element size of the audstate array (ostate)
  // from nostate elements to nstate elements

  lives_audio_track_state_t *audstate=(lives_audio_track_state_t *)g_malloc(nstate*sizeof(lives_audio_track_state_t));
  int i;

  for (i=0;i<nstate;i++) {
    if (i<nostate) {
      audstate[i].afile=ostate[i].afile;
      audstate[i].seek=ostate[i].seek;
      audstate[i].vel=ostate[i].vel;
    }
    else {
      audstate[i].afile=0;
      audstate[i].seek=audstate[i].vel=0.;
    }
  }

  if (ostate!=NULL) g_free(ostate);

  return audstate;
}
	





static lives_audio_track_state_t *aframe_to_atstate(weed_plant_t *event) {
  // parse an audio frame, and set the track file, seek and velocity values


  int error,atrack;
  int num_aclips=weed_leaf_num_elements(event,"audio_clips");
  int *aclips=weed_get_int_array(event,"audio_clips",&error);
  double *aseeks=weed_get_double_array(event,"audio_seeks",&error);
  int naudstate=0;
  lives_audio_track_state_t *atstate=NULL;
  
  register int i;

  int btoffs=mainw->multitrack!=NULL?mainw->multitrack->opts.back_audio_tracks:1;

  for (i=0;i<num_aclips;i+=2) {
    if (aclips[i+1]>0) { // else ignore
      atrack=aclips[i];
      if (atrack+btoffs>=naudstate-1) {
	atstate=resize_audstate(atstate,naudstate,atrack+btoffs+2);
	naudstate=atrack+btoffs+1;
	atstate[naudstate].afile=-1;
      }
      atstate[atrack+btoffs].afile=aclips[i+1];
      atstate[atrack+btoffs].seek=aseeks[i];
      atstate[atrack+btoffs].vel=aseeks[i+1];
    }
  }

  weed_free(aclips);
  weed_free(aseeks);

  return atstate;
}




lives_audio_track_state_t *get_audio_and_effects_state_at(weed_plant_t *event_list, weed_plant_t *st_event, gboolean get_audstate, gboolean exact) {

  // if exact is set, we must rewind back to first active stateful effect, 
  // and play forwards from there (not yet implemented - TODO)

  weed_plant_t *nevent=get_first_event(event_list),*event;
  lives_audio_track_state_t *atstate=NULL,*audstate=NULL;
  weed_plant_t *deinit_event;
  int error,nfiles,nnfiles;
  weed_timecode_t last_tc=0,fill_tc;
  int i;

  // gets effects state (initing any effects which should be active)

  // optionally: gets audio state at audio frame prior to st_event, sets atstate[0].tc
  // and initialises audio buffers



  fill_tc=get_event_timecode(st_event);

  do {
    event=nevent;
    if (WEED_EVENT_IS_FILTER_MAP(event)) {
      mainw->filter_map=event;
    }
    else if (WEED_EVENT_IS_FILTER_INIT(event)) {
      deinit_event=weed_get_voidptr_value(event,"deinit_event",&error);
      if (get_event_timecode(deinit_event)>=fill_tc) {
	// this effect should be activated
	process_events(event,get_event_timecode(event));
      }
    }
    else if (get_audstate&&weed_plant_has_leaf(event,"audio_clips")) {

      atstate=aframe_to_atstate(event);

      if (audstate==NULL) audstate=atstate;
      else {
	// have an existing audio state, update with current
	for (nfiles=0;audstate[nfiles].afile!=-1;nfiles++);

	for (i=0;i<nfiles;i++) {
	  // increase seek values up to current frame
	  audstate[i].seek+=audstate[i].vel*(get_event_timecode(event)-last_tc)/U_SEC;
	}

	for (nnfiles=0;atstate[nnfiles].afile!=-1;nnfiles++);

	if (nnfiles>nfiles) {
	  audstate=resize_audstate(audstate,nfiles,nnfiles+1);
	  audstate[nnfiles].afile=-1;
	}

	for (i=0;i<nnfiles;i++) {
	  if (atstate[i].afile>0) {
	    audstate[i].afile=atstate[i].afile;
	    audstate[i].seek=atstate[i].seek;
	    audstate[i].vel=atstate[i].vel;
	  }
	}                                                             
	g_free(atstate);
      }

      last_tc=get_event_timecode(event);
    }
    nevent=get_next_event(event);
  } while (event!=st_event);


  if (audstate!=NULL) {
    for (nfiles=0;audstate[nfiles].afile!=-1;nfiles++);

    for (i=0;i<nfiles;i++) {
      // increase seek values
      audstate[i].seek+=audstate[i].vel*(fill_tc-last_tc)/U_SEC;
    }

  }


  return audstate;

}



void fill_abuffer_from(lives_audio_buf_t *abuf, weed_plant_t *event_list, weed_plant_t *st_event, gboolean exact) {
  // fill audio buffer with audio samples, using event_list as a guide
  // if st_event!=NULL, that is our start event, and we will calculate the audio state at that
  // point

  // otherwise, we continue from where we left off the last time


  // all we really do here is set from_files,aseeks and avels arrays and call render_audio_segment

  lives_audio_track_state_t *atstate;
  int nnfiles,i;
  gdouble chvols[65536]; // TODO - use list

  static weed_timecode_t last_tc;
  static weed_timecode_t fill_tc;
  static weed_plant_t *event;
  static int nfiles;

  static int *from_files=NULL;
  static double *aseeks=NULL,*avels=NULL;

  gboolean is_cont=FALSE;
  if (abuf==NULL) return;

  abuf->samples_filled=0; // write fill level of buffer
  abuf->start_sample=0; // read level

  if (st_event!=NULL) {
    // this is only called for the first buffered read

    event=st_event;
    last_tc=get_event_timecode(event);

    if (from_files!=NULL) g_free(from_files);
    if (avels!=NULL) g_free(avels);
    if (aseeks!=NULL) g_free(aseeks);

    if (mainw->multitrack!=NULL) nfiles=weed_leaf_num_elements(mainw->multitrack->avol_init_event,"in_tracks");

    else nfiles=1;

    from_files=(int *)g_malloc(nfiles*sizint);
    avels=(double *)g_malloc(nfiles*sizdbl);
    aseeks=(double *)g_malloc(nfiles*sizdbl);

    for (i=0;i<nfiles;i++) {
      from_files[i]=0;
      avels[i]=aseeks[i]=0.;
    }

    // TODO - actually what we should do here is get the audio state for
    // the *last* frame in the buffer and then adjust the seeks back to the
    // beginning of the buffer, in case an audio track starts during the
    // buffering period. The current way is fine for a preview, but when we 
    // implement rendering of partial event lists we will need to do this

    // a negative seek value would mean that we need to pad silence at the 
    // start of the track buffer


    atstate=get_audio_and_effects_state_at(event_list,event,TRUE,exact);
    
    if (atstate!=NULL) {
      
      for (nnfiles=0;atstate[nnfiles].afile!=-1;nnfiles++);
      
      for (i=0;i<nnfiles;i++) {
	if (atstate[i].afile>0) {
	  from_files[i]=atstate[i].afile;
	  avels[i]=atstate[i].vel;
	  aseeks[i]=atstate[i].seek;
	}
      }
      
      g_free(atstate);
    }
  }
  else {
    is_cont=TRUE;
  }

  if (mainw->multitrack!=NULL) {
    // get channel volumes from the mixer
    for (i=0;i<nfiles;i++) {
      if (mainw->multitrack!=NULL&&mainw->multitrack->audio_vols!=NULL) {
	chvols[i]=(gdouble)GPOINTER_TO_INT(g_list_nth_data(mainw->multitrack->audio_vols,i))/1000000.;
      }
    }
  }
  else chvols[0]=1.;

  fill_tc=last_tc+(gdouble)(abuf->samp_space)/(gdouble)abuf->arate*U_SEC;

  // continue until either we have a full buffer, or we reach next audio frame
  while (event!=NULL&&get_event_timecode(event)<=fill_tc) {
    if (!is_cont) event=get_next_frame_event(event);
    if (event!=NULL&&weed_plant_has_leaf(event,"audio_clips")) {
      // got next audio frame
      weed_timecode_t tc=get_event_timecode(event);
      if (tc>=fill_tc) break;

      tc+=(U_SEC/cfile->fps*!is_blank_frame(event,FALSE));

      render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, tc, chvols, 1., 1., abuf);

      for (i=0;i<nfiles;i++) {
	// increase seek values
	aseeks[i]+=avels[i]*(tc-last_tc)/U_SEC;
      }

      last_tc=tc;

      // process audio updates at this frame
      atstate=aframe_to_atstate(event);

      if (atstate!=NULL) {
	for (nnfiles=0;atstate[nnfiles].afile!=-1;nnfiles++);

	for (i=0;i<nnfiles;i++) {
	  if (atstate[i].afile>0) {
	    from_files[i]=atstate[i].afile;
	    avels[i]=atstate[i].vel;
	    aseeks[i]=atstate[i].seek;
	  }
	}
	g_free(atstate);
      }
    }
    is_cont=FALSE;
  }
  
  if (last_tc<fill_tc) {
    // flush the rest of the audio
    render_audio_segment(nfiles, from_files, -1, avels, aseeks, last_tc, fill_tc, chvols, 1., 1., abuf);
    for (i=0;i<nfiles;i++) {
      // increase seek values
      aseeks[i]+=avels[i]*(fill_tc-last_tc)/U_SEC;
    }
  }


  mainw->write_abuf++;
  if (mainw->write_abuf>=prefs->num_rtaudiobufs) mainw->write_abuf=0;

  last_tc=fill_tc;

  if (mainw->abufs_to_fill>0) {
    pthread_mutex_lock(&mainw->abuf_mutex);
    mainw->abufs_to_fill--;
    pthread_mutex_unlock(&mainw->abuf_mutex);
  }

}




void init_jack_audio_buffers (gint achans, gint arate, gboolean exact) {
#ifdef ENABLE_JACK

  int i,chan;

  mainw->jackd->abufs=(lives_audio_buf_t **)g_malloc(prefs->num_rtaudiobufs*sizeof(lives_audio_buf_t *));
  
  for (i=0;i<prefs->num_rtaudiobufs;i++) {
    mainw->jackd->abufs[i]=(lives_audio_buf_t *)g_malloc(sizeof(lives_audio_buf_t));
    
    mainw->jackd->abufs[i]->out_achans=achans;
    mainw->jackd->abufs[i]->arate=arate;
    mainw->jackd->abufs[i]->samp_space=XSAMPLES/prefs->num_rtaudiobufs;
    mainw->jackd->abufs[i]->bufferf=g_malloc(achans*sizeof(float *));
    for (chan=0;chan<achans;chan++) {
      mainw->jackd->abufs[i]->bufferf[chan]=g_malloc(XSAMPLES/prefs->num_rtaudiobufs*sizeof(float));
    }
  }
#endif
}


void init_pulse_audio_buffers (gint achans, gint arate, gboolean exact) {
#ifdef HAVE_PULSE_AUDIO

  int i;

  mainw->pulsed->abufs=(lives_audio_buf_t **)g_malloc(prefs->num_rtaudiobufs*sizeof(lives_audio_buf_t *));
  
  for (i=0;i<prefs->num_rtaudiobufs;i++) {
    mainw->pulsed->abufs[i]=(lives_audio_buf_t *)g_malloc(sizeof(lives_audio_buf_t));
    
    mainw->pulsed->abufs[i]->out_achans=achans;
    mainw->pulsed->abufs[i]->arate=arate;
    mainw->pulsed->abufs[i]->samp_space=XSAMPLES/prefs->num_rtaudiobufs;  // samp_space here is in stereo samples
    mainw->pulsed->abufs[i]->buffer16=g_malloc(sizeof(short *));
    mainw->pulsed->abufs[i]->buffer16[0]=g_malloc(XSAMPLES/prefs->num_rtaudiobufs*achans*sizeof(short));
  }
#endif
}



void free_jack_audio_buffers(void) {
#ifdef ENABLE_JACK

  int i,chan;

  if (mainw->jackd==NULL) return;

  if (mainw->jackd->abufs==NULL) return;

  for (i=0;i<prefs->num_rtaudiobufs;i++) {
    if (mainw->jackd->abufs[i]!=NULL) {
      for (chan=0;chan<mainw->jackd->abufs[i]->out_achans;chan++) {
	g_free(mainw->jackd->abufs[i]->bufferf[chan]);
      }
      g_free(mainw->jackd->abufs[i]->bufferf);
      g_free(mainw->jackd->abufs[i]);
   }
  }
  g_free(mainw->jackd->abufs);
#endif
}


void free_pulse_audio_buffers(void) {
#ifdef HAVE_PULSE_AUDIO

  int i;

  if (mainw->pulsed==NULL) return;

  if (mainw->pulsed->abufs==NULL) return;

  for (i=0;i<prefs->num_rtaudiobufs;i++) {
    if (mainw->pulsed->abufs[i]!=NULL) {
      g_free(mainw->pulsed->abufs[i]->buffer16[0]);
      g_free(mainw->pulsed->abufs[i]->buffer16);
      g_free(mainw->pulsed->abufs[i]);
    }
  }
#endif
}




gboolean resync_audio(gint frameno) {
  // if we are using a realtime audio player, resync to frameno
  // and return TRUE

  // otherwise return FALSE


  // this is called for example when the play position jumps, either due
  // to external transport changes, (jack transport, osc retrigger or goto)
  // or if we are looping a video selection

  // this is only active if "audio follows video rate/fps changes" is set

  if (!(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)) return FALSE;

#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
    if (!mainw->is_rendering) {
      jack_audio_seek_frame(mainw->jackd,frameno);
      mainw->rec_aclip=mainw->current_file;
      mainw->rec_avel=cfile->pb_fps/cfile->fps;
      mainw->rec_aseek=(gdouble)mainw->jackd->seek_pos/(gdouble)(cfile->arate*cfile->achans*cfile->asampsize/8);
    }
    return TRUE;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
    if (!mainw->is_rendering) {
      pulse_audio_seek_frame(mainw->pulsed,frameno);
      mainw->rec_aclip=mainw->current_file;
      mainw->rec_avel=cfile->pb_fps/cfile->fps;
      mainw->rec_aseek=(gdouble)mainw->pulsed->seek_pos/(gdouble)(cfile->arate*cfile->achans*cfile->asampsize/8);
    }
    return TRUE;
  }
#endif
  return FALSE;

}




//////////////////////////////////////////////////////////////////////////
static lives_audio_buf_t *cache_buffer=NULL;
static pthread_t athread;


static void *cache_my_audio(void *arg) {
  lives_audio_buf_t *cbuffer = (lives_audio_buf_t *)arg;
  char *filename;
  register int i;

  cbuffer->is_ready=TRUE;

  while (!cbuffer->die) {

    // wait for request from client
    while (cbuffer->is_ready&&!cbuffer->die) {
      sched_yield();
      g_usleep(prefs->sleep_time);
    }

    if (cbuffer->die) {
      if (cbuffer->_fd!=-1) close(cbuffer->_fd);
      return cbuffer;
    }

    // read from file and process data
    //g_printerr("got buffer request !\n");

    if (cbuffer->operation!=LIVES_READ_OPERATION) {
      cbuffer->is_ready=TRUE;
      continue;
    }

    cbuffer->eof=FALSE;

    // TODO - if out_asamps changed, we need to free all buffers and set _cachans==0

    if (cbuffer->out_asamps!=cbuffer->_casamps) {
      if (cbuffer->bufferf!=NULL) {
	// free float channels
	for (i=0;i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans);i++) {
	  free(cbuffer->bufferf[i]);
	}
	free (cbuffer->bufferf);
	cbuffer->bufferf=NULL;
      }

      if (cbuffer->buffer16!=NULL) {
	// free 16bit channels
	for (i=0;i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans);i++) {
	  free(cbuffer->buffer16[i]);
	}
	free (cbuffer->buffer16);
	cbuffer->buffer16=NULL;
      }

      cbuffer->_cachans=0;
      cbuffer->_cout_interleaf=FALSE;
      cbuffer->_csamp_space=0;
    }


    // do we need to allocate output buffers ?
    switch (cbuffer->out_asamps) {
    case 8:
    case 24:
    case 32:
      // not yet implemented
      break;
    case 16:
      // we need 16 bit buffer(s) only
      if (cbuffer->bufferf!=NULL) {
	// free float channels
	for (i=0;i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans);i++) {
	  free(cbuffer->bufferf[i]);
	}
	free (cbuffer->bufferf);
	cbuffer->bufferf=NULL;
      }

      if ( (cbuffer->out_interleaf?1:cbuffer->out_achans) != (cbuffer->_cout_interleaf?1:cbuffer->_cachans) 
	   || (cbuffer->samp_space/(cbuffer->out_interleaf?1:cbuffer->out_achans) != 
	       (cbuffer->_csamp_space/(cbuffer->_cout_interleaf?1:cbuffer->_cachans) ) ) ) {
	// channels or samp_space changed

	if ( (cbuffer->out_interleaf?1:cbuffer->out_achans) > (cbuffer->_cout_interleaf?1:cbuffer->_cachans) ) {
	  // ouput channels increased
	  cbuffer->buffer16 = 
	    realloc(cbuffer->buffer16,(cbuffer->out_interleaf?1:cbuffer->out_achans)*sizeof(short *));
	  for (i=(cbuffer->_cout_interleaf?1:cbuffer->_cachans);i<cbuffer->out_achans;i++) {
	    cbuffer->buffer16[i]=NULL;
	  }
	}

	for (i=0;i<(cbuffer->out_interleaf?1:cbuffer->out_achans);i++) {
	  // realloc existing channels and add new ones
	  cbuffer->buffer16[i]=realloc(cbuffer->buffer16[i], cbuffer->samp_space*sizeof(short)*
				       (cbuffer->out_interleaf?cbuffer->out_achans:1));
	}

	// free any excess channels

	for (i=(cbuffer->out_interleaf?1:cbuffer->out_achans);i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans);i++) {
	  free(cbuffer->buffer16[i]);
	}

	// output channels decreased

	cbuffer->buffer16 = 
	  realloc(cbuffer->buffer16,(cbuffer->out_interleaf?1:cbuffer->out_achans)*sizeof(short *));

      }

      break;
    case -32:
      // we need 16 bit buffer(s) and float buffer(s)

      // 16 bit buffers follow in out_achans but in_interleaf...

      if ( (cbuffer->in_interleaf?1:cbuffer->out_achans) != (cbuffer->_cin_interleaf?1:cbuffer->_cachans) 
	   || (cbuffer->samp_space/(cbuffer->in_interleaf?1:cbuffer->out_achans) != 
	       (cbuffer->_csamp_space/(cbuffer->_cin_interleaf?1:cbuffer->_cachans) ) ) ) {
	// channels or samp_space changed

	if ( (cbuffer->in_interleaf?1:cbuffer->out_achans) > (cbuffer->_cin_interleaf?1:cbuffer->_cachans) ) {
	  // ouput channels increased
	  cbuffer->buffer16 = 
	    realloc(cbuffer->buffer16,(cbuffer->in_interleaf?1:cbuffer->out_achans)*sizeof(short *));
	  for (i=(cbuffer->_cin_interleaf?1:cbuffer->_cachans);i<cbuffer->out_achans;i++) {
	    cbuffer->buffer16[i]=NULL;
	  }
	}

	for (i=0;i<(cbuffer->in_interleaf?1:cbuffer->out_achans);i++) {
	  // realloc existing channels and add new ones
	  cbuffer->buffer16[i]=realloc(cbuffer->buffer16[i], cbuffer->samp_space*sizeof(short)*
				       (cbuffer->in_interleaf?cbuffer->out_achans:1));
	}

	// free any excess channels

	for (i=(cbuffer->in_interleaf?1:cbuffer->out_achans);i<(cbuffer->_cin_interleaf?1:cbuffer->_cachans);i++) {
	  free(cbuffer->buffer16[i]);
	}

	// output channels decreased

	cbuffer->buffer16 = 
	  realloc(cbuffer->buffer16,(cbuffer->in_interleaf?1:cbuffer->out_achans)*sizeof(short *));

      }



      if ( (cbuffer->out_interleaf?1:cbuffer->out_achans) != (cbuffer->_cout_interleaf?1:cbuffer->_cachans) 
	   || (cbuffer->samp_space/(cbuffer->out_interleaf?1:cbuffer->out_achans) != 
	       (cbuffer->_csamp_space/(cbuffer->_cout_interleaf?1:cbuffer->_cachans) ) ) ) {
	// channels or samp_space changed

	if ( (cbuffer->out_interleaf?1:cbuffer->out_achans) > (cbuffer->_cout_interleaf?1:cbuffer->_cachans) ) {
	  // ouput channels increased
	  cbuffer->bufferf = 
	    realloc(cbuffer->bufferf,(cbuffer->out_interleaf?1:cbuffer->out_achans)*sizeof(float *));
	  for (i=(cbuffer->_cout_interleaf?1:cbuffer->_cachans);i<cbuffer->out_achans;i++) {
	    cbuffer->bufferf[i]=NULL;
	  }
	}

	for (i=0;i<(cbuffer->out_interleaf?1:cbuffer->out_achans);i++) {
	  // realloc existing channels and add new ones
	  cbuffer->bufferf[i]=realloc(cbuffer->bufferf[i], cbuffer->samp_space*sizeof(float)*
				       (cbuffer->out_interleaf?cbuffer->out_achans:1));
	}

	// free any excess channels

	for (i=(cbuffer->out_interleaf?1:cbuffer->out_achans);i<(cbuffer->_cout_interleaf?1:cbuffer->_cachans);i++) {
	  free(cbuffer->bufferf[i]);
	}

	// output channels decreased

	cbuffer->bufferf = 
	  realloc(cbuffer->bufferf,(cbuffer->out_interleaf?1:cbuffer->out_achans)*sizeof(float *));

      }

      break;
    default:
      break;
    }
	  
    // update _cinterleaf, etc.
    cbuffer->_cin_interleaf=cbuffer->in_interleaf;
    cbuffer->_cout_interleaf=cbuffer->out_interleaf;
    cbuffer->_csamp_space=cbuffer->samp_space;
    cbuffer->_cachans=cbuffer->out_achans;
    cbuffer->_casamps=cbuffer->out_asamps;

    // open new file if necessary

    if (cbuffer->fileno!=cbuffer->_cfileno) {
      file *afile=mainw->files[cbuffer->fileno];

      if (cbuffer->_fd!=-1) close(cbuffer->_fd);

	if (afile->opening) 
	  filename=g_strdup_printf("%s/%s/audiodump.pcm",prefs->tmpdir,mainw->files[cbuffer->fileno]->handle);
	else filename=g_strdup_printf("%s/%s/audio",prefs->tmpdir,mainw->files[cbuffer->fileno]->handle);

	cbuffer->_fd=open(filename,O_RDONLY);
	if (cbuffer->_fd==-1) {
	  g_printerr("audio cache thread: error opening %s\n",filename);
	  cbuffer->in_achans=0;
	  cbuffer->fileno=-1;  ///< let client handle this
	  cbuffer->is_ready=TRUE;
	  continue;
	}

	g_free(filename);
    }

    if (cbuffer->fileno!=cbuffer->_cfileno||cbuffer->seek!=cbuffer->_cseek) {
      lseek64(cbuffer->_fd, cbuffer->seek, SEEK_SET);
    }

    cbuffer->_cfileno=cbuffer->fileno;

    // prepare file read buffer

    if (cbuffer->bytesize!=cbuffer->_cbytesize) {
      cbuffer->_filebuffer=realloc(cbuffer->_filebuffer,cbuffer->bytesize);

      if (cbuffer->_filebuffer==NULL) {
	cbuffer->_cbytesize=cbuffer->bytesize=0;
	cbuffer->in_achans=0;
	cbuffer->is_ready=TRUE;
	continue;
      }

    }

    // read from file
    cbuffer->_cbytesize=read(cbuffer->_fd, cbuffer->_filebuffer, cbuffer->bytesize);

    if (cbuffer->_cbytesize<0) {
      cbuffer->bytesize=cbuffer->_cbytesize=0;
      cbuffer->in_achans=0;
      cbuffer->is_ready=TRUE;
      continue;
    }
    
    if (cbuffer->_cbytesize<cbuffer->bytesize) {
      cbuffer->eof=TRUE;
      cbuffer->_csamp_space=(long)((double)cbuffer->samp_space/(double)cbuffer->bytesize*(double)cbuffer->_cbytesize);
      cbuffer->samp_space=cbuffer->_csamp_space;
    }

    cbuffer->bytesize=cbuffer->_cbytesize;
    cbuffer->_cseek=(cbuffer->seek+=cbuffer->bytesize);

    // do conversion


    // convert from 8 bit to 16 bit and mono to stereo if necessary
    // resample as we go 
    if(cbuffer->in_asamps==8) {

      // TODO - error on non-interleaved
      sample_move_d8_d16 (cbuffer->buffer16[0],(guchar *)cbuffer->_filebuffer, cbuffer->samp_space, cbuffer->bytesize, 
			  cbuffer->shrink_factor, cbuffer->out_achans, cbuffer->in_achans, 0);
	}
    // 16 bit input samples 
    // resample as we go 
	else {
	  sample_move_d16_d16(cbuffer->buffer16[0], (short*)cbuffer->_filebuffer, cbuffer->samp_space, cbuffer->bytesize, 
			      cbuffer->shrink_factor, cbuffer->out_achans, cbuffer->in_achans, 
			      cbuffer->swap_endian?SWAP_X_TO_L:0, 0);
	}
	

    // if our out_asamps is 16, we are done


    cbuffer->is_ready=TRUE;
  }
  return cbuffer;
}



lives_audio_buf_t *audio_cache_init (void) {

  cache_buffer=(lives_audio_buf_t *)calloc(1,sizeof(lives_audio_buf_t));
  cache_buffer->is_ready=FALSE;
  cache_buffer->in_achans=0;

  // NULL all pointers of cache_buffer

  cache_buffer->buffer8=NULL;
  cache_buffer->buffer16=NULL;
  cache_buffer->buffer24=NULL;
  cache_buffer->buffer32=NULL;
  cache_buffer->bufferf=NULL;
  cache_buffer->_filebuffer=NULL;
  cache_buffer->_cbytesize=0;
  cache_buffer->_csamp_space=0;
  cache_buffer->_cachans=0;
  cache_buffer->_casamps=0;
  cache_buffer->_cout_interleaf=FALSE;
  cache_buffer->_cin_interleaf=FALSE;
  cache_buffer->eof=FALSE;
  cache_buffer->die=FALSE;

  cache_buffer->_cfileno=-1;
  cache_buffer->_cseek=-1;
  cache_buffer->_fd=-1;


  // init the audio caching thread for rt playback
  pthread_create(&athread,NULL,cache_my_audio,cache_buffer);

  return cache_buffer;
}



void audio_cache_end (void) {
  int i;
  lives_audio_buf_t *xcache_buffer;

  cache_buffer->die=TRUE;  ///< tell cache thread to exit when possible
  pthread_join(athread,NULL);

  // free all buffers
  
  for (i=0;i<cache_buffer->_cachans;i++) {
    if (cache_buffer->buffer8!=NULL&&cache_buffer->buffer8[i]!=NULL) free(cache_buffer->buffer8[i]);
    if (cache_buffer->buffer16!=NULL&&cache_buffer->buffer16[i]!=NULL) free(cache_buffer->buffer16[i]);
    if (cache_buffer->buffer24!=NULL&&cache_buffer->buffer24[i]!=NULL) free(cache_buffer->buffer24[i]);
    if (cache_buffer->buffer32!=NULL&&cache_buffer->buffer32[i]!=NULL) free(cache_buffer->buffer32[i]);
    if (cache_buffer->bufferf!=NULL&&cache_buffer->bufferf[i]!=NULL) free(cache_buffer->bufferf[i]);
  }

  if (cache_buffer->buffer8!=NULL) free(cache_buffer->buffer8);
  if (cache_buffer->buffer16!=NULL) free(cache_buffer->buffer16);
  if (cache_buffer->buffer24!=NULL) free(cache_buffer->buffer24);
  if (cache_buffer->buffer32!=NULL) free(cache_buffer->buffer32);
  if (cache_buffer->bufferf!=NULL) free(cache_buffer->bufferf);

  if (cache_buffer->_filebuffer!=NULL) free(cache_buffer->_filebuffer);

  // make this threadsafe
  xcache_buffer=cache_buffer;
  cache_buffer=NULL;
  free(xcache_buffer);
}


lives_audio_buf_t *audio_cache_get_buffer(void) {
  return cache_buffer;
}
