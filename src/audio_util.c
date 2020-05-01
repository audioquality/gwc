/*****************************************************************************
*   GTK Wave Cleaner Version 0.19
*   Copyright (C) 2001 Jeffrey J. Welty
*   
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2
*   of the License, or (at your option) any later version.
*   
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*   
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*******************************************************************************/

/* audio_util.c */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <memory.h>
#include <endian.h>
#include <fcntl.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <sndfile.h>

#include "gwc.h"
#include "fmtheaders.h"
#include "audio_device.h"
#include "soundfile.h"

int audio_state = AUDIO_IS_IDLE ;
int wavefile_fd = -1 ;
long audio_bytes_written ;
int rate = 44100 ;
int stereo = 1 ;
int channels = 2 ;
int audio_bits = 16 ;
int BYTESPERSAMPLE = 2 ;
int MAXSAMPLEVALUE = 1 ;
int PLAYBACK_FRAMESIZE = 4 ;
int FRAMESIZE = 4 ;
int nonzero_seek;
long zeros_needed;
/*  int dump_sample = 0 ;  */
long wavefile_data_start ;

SNDFILE      *sndfile = NULL ;
SF_INFO      sfinfo ;

int audiofileisopen = 0 ;

#define SNDFILE_TYPE 0x01

int audio_type ;

extern struct view audio_view ;
extern struct sound_prefs prefs ;
extern struct encoding_prefs encoding_prefs;

int current_sample ;

void position_wavefile_pointer(long sample_number) ;

unsigned long BUFSIZE = 0;
unsigned char audio_buffer[MAXBUFSIZE] ;
unsigned char audio_buffer2[MAXBUFSIZE] ;

extern long playback_startplay_position;
long playback_start_position;
long playback_end_position;
long playback_samples_remaining = 0;
long playback_samples_total = 0;
long playback_bytes_per_block ;
long looped_count = 0;


void audio_normalize(int flag)
{
    if(audio_type == SNDFILE_TYPE) {
	if(flag == 0)
	    sf_command(sndfile, SFC_SET_NORM_DOUBLE, NULL, SF_FALSE) ;
	else
	    sf_command(sndfile, SFC_SET_NORM_DOUBLE, NULL, SF_TRUE) ;
    }
}

void write_wav_header(int thefd, int speed, long bcount, int bits, int stereo)
{
    /* Spit out header here... */
	    wavhead header;

	    char *riff = "RIFF";
	    char *wave = "WAVE";
	    char *fmt = "fmt ";
	    char *data = "data";

	    memcpy(&(header.main_chunk), riff, 4);
	    header.length = sizeof(wavhead) - 8 + bcount;
	    memcpy(&(header.chunk_type), wave, 4);

	    memcpy(&(header.sub_chunk), fmt, 4);
	    header.sc_len = 16;
	    header.format = 1;
	    header.modus = stereo + 1;
	    header.sample_fq = speed;
	    header.byte_p_sec = ((bits > 8)? 2:1)*(stereo+1)*speed;
/* Correction by J.A. Bezemer: */
	    header.byte_p_spl = ((bits > 8)? 2:1)*(stereo+1);
    /* was: header.byte_p_spl = (bits > 8)? 2:1; */

	    header.bit_p_spl = bits;

	    memcpy(&(header.data_chunk), data, 4);
	    header.data_length = bcount;
	    write(thefd, &header, sizeof(header));
}

int config_audio_device(int rate_set, int bits_set, int stereo_set)
{
    AUDIO_FORMAT format,format_set;
/*      int fragset = 0x7FFF000F ;  */

    bits_set = 16 ;
    /* play everything as 16 bit, signed integers */
    /* using the appropriate endiannes */

#if __BYTE_ORDER == __BIG_ENDIAN
    format_set = GWC_S16_BE ;
#else
    format_set = GWC_S16_LE ;
#endif

    rate = rate_set ;
    audio_bits = bits_set ;
    stereo = stereo_set ;
    format = format_set ;

/*      if(ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &fragset) == -1) {  */
/*  	warning("error setting buffer size on audio device") ;  */
/*      }  */

    channels = stereo + 1 ;
    rate = rate_set ;

    if (audio_device_set_params(&format_set, &channels, &rate) == -1) {
	d_print("Audio: error setting device parameters, channels %i, rate: %i\n", channels, rate);
    }

    if(format != format_set) {
	char *buf_fmt_str ;
	char buf[85] ;
	switch(format_set) {
	    case GWC_U8 : buf_fmt_str = "8 bit (unsigned)" ; bits_set = 8 ; break ;
	    case GWC_S8 : buf_fmt_str = "8 bit (signed)" ; bits_set = 8 ; break ;
	    case GWC_S16_BE :
	    case GWC_S16_LE : buf_fmt_str = "16 bit" ; bits_set = 16 ; break ;
	    default : buf_fmt_str = "unknown!" ; bits_set = 8 ; break ;
	}
        snprintf(buf, sizeof(buf), "Set bits to %s - does your soundcard support what you requested?\n", buf_fmt_str) ;
	warning(buf) ;
	printf(buf);
    }

    if(channels < stereo + 1) {
	char buf[100] ;
	snprintf(buf, sizeof(buf), "Failed to set stereo mode\nYour sound card may not support stereo\n") ;
	warning(buf) ;
	printf(buf);
    }
    //NOTE: allow "stereo"(refers to input data) and "channels"(refers to audio hardware) to be different
    //stereo_set = channels - 1 ;

    if(ABS(rate_set - rate) > 10) {
	char buf[80] ;
	snprintf(buf, sizeof(buf), "Rate set to %d instead of %d\nYour sound card may not support the desired rate\n",
	             rate_set, rate) ;
	warning(buf) ;
	printf(buf);
    }
    
    rate = rate_set ;
    audio_bits = bits_set ;
    
    return 0;
}

/*
 * return number of processed samples since opening the audio device
 */
long get_processed_samples(void) {
    return audio_device_processed_bytes() / PLAYBACK_FRAMESIZE;
}

/*
 * return number of processed frames since opening the audio device
 */
long get_processed_frames(void) {
    return get_processed_samples() / (stereo + 1);
}

/*
 * return playback position (current sample)
 */
 /*
long get_playback_position(void) {
  
  extern int audio_is_looping;
  long current_position;
  long processed_frames = get_processed_frames();
  //d_print("get_playback_position processed_frames = %ld\n", processed_frames);
  
  if ((playback_end_position - playback_startplay_position) >= processed_frames) {
    // 1st-loop run, from startplay to end
    current_position = playback_startplay_position + processed_frames;
  } else {
    if (audio_is_looping) {
      // consecutive loops
      if (playback_startplay_position > playback_start_position) {
	// we started in the middle
	current_position = playback_start_position + (processed_frames -
			   (playback_end_position - playback_startplay_position)) % playback_samples_total;
      } else {
	// we started at the beginning of the region_of_interest
	current_position = playback_startplay_position + processed_frames % playback_samples_total;
      }
    } else {
      // no looping, we processed all the frames including zeros => we are at the end!
      current_position = playback_end_position;
    }
  }

  //d_print("get_playback_position: %ld, processed_samples: %ld\n", current_position, processed_samples);
  
  return current_position;
}
*/

/*
 * call single time, from gwc.c: start_gwc_playback()
 * 
 * prepare for playback
 * set global variables, open audio device
 */
long start_playback(char *output_device, struct view *v, struct sound_prefs *p, double seconds_per_block, double seconds_to_preload)
{
    long first, last ;
    long samples_per_block ;

    if(audio_type == SNDFILE_TYPE && sndfile == NULL) return 1 ;

    audio_device_close(1) ;

    if (audio_device_open(output_device) == -1) {
	char buf[255] ;
	#ifdef HAVE_ALSA
	snprintf(buf, sizeof(buf), "Failed to open alsa output device %s.\nCheck Settings->Preferences for device information.", output_device);
	#elif HAVE_PULSE_AUDIO
	snprintf(buf, sizeof(buf), "Failed to open PulseAudio output device.\n") ;
	#else
	snprintf(buf, sizeof(buf), "Failed to open OSS audio output device %s.\nCheck Settings->Preferences for device information.", output_device);
	#endif
	warning(buf);
	d_print("Error start_playback: %s\n",buf);
	return 0 ;
    }
    //g_print("audio device %s opened, processed_bytes: %ld\n", output_device, audio_device_processed_bytes());
    
    get_region_of_interest(&first, &last, v) ;

    playback_start_position = first;
    playback_end_position = last + 1;

    if ((playback_startplay_position < playback_start_position) || 
        (playback_startplay_position > audio_view.last_sample) ||
        (playback_startplay_position >= (playback_end_position - 900))) {
      playback_startplay_position = playback_start_position;
    }
    d_print("playback_startplay_position: %ld, start: %ld, end: %ld\n", playback_startplay_position, playback_start_position, playback_end_position);    
    
    samples_per_block = p->rate * seconds_per_block;
    playback_bytes_per_block = samples_per_block * PLAYBACK_FRAMESIZE;
    
    // stereo 1 = stereo, 0 = mono
    // playback_bits is the number of bits per sample
    // rate is the number of samples per second
    config_audio_device(p->rate, p->playback_bits, p->stereo);	//Set up the audio device.

    BUFSIZE = audio_device_best_buffer_size(playback_bytes_per_block);
    d_print("playback_bytes_per_block requested: %ld, best: %lu\n", playback_bytes_per_block, BUFSIZE);
    if (BUFSIZE > MAXBUFSIZE)
	BUFSIZE = MAXBUFSIZE;
    playback_bytes_per_block = BUFSIZE ;
    d_print("playback_bytes_per_block set: %ld\n", playback_bytes_per_block);
    
    playback_samples_total = playback_end_position - playback_start_position;
    playback_samples_remaining = playback_end_position - playback_startplay_position;
    
    // process_audio: align buffer at the end of loop with zeros
    zeros_needed = playback_bytes_per_block - ((playback_samples_remaining * PLAYBACK_FRAMESIZE) % playback_bytes_per_block) ;
    d_print("playback zeros_needed: %ld\n", zeros_needed);
    if (zeros_needed < PLAYBACK_FRAMESIZE)
      zeros_needed = PLAYBACK_FRAMESIZE ;
    
    d_print("playback_samples_total: %ld, remaining: %ld\n", playback_samples_total, playback_samples_remaining);
    
    audio_state = AUDIO_IS_PLAYBACK;

    position_wavefile_pointer(playback_startplay_position);
    
    v->prev_cursor_position = -1 ;
    looped_count = 0 ;

    return samples_per_block ;
}

/* process_audio for mac_os_x is found in the audio_osx.c */
#ifndef MAC_OS_X
/*
 * run during playback from gwc.c: play_a_block
 * to load audio data from file into buffer
 * 
 * return: 0 - playing, buffering in progress
 *         1 - finished buffering
 *         2 - error, buffer underflow!
 */
int process_audio()
{
    int i, frame;
    unsigned char *p_char, *p_char2;
    long len = 0, n_samples_to_read = 0, n_read = 0;
    short *p_short, *p_short2 ;
    int *p_int, *p_int2 ;
    
    if (audio_state == AUDIO_IS_IDLE) {
	d_print("process_audio says NOTHING is going on.\n") ;
	return 1;
    }
    if (audio_state != AUDIO_IS_PLAYBACK) {
	d_print("process_audio: unsupported audio state: %i\n", audio_state);
	return 1;
    }
    
    if (playback_samples_remaining > 0)
	len = audio_device_nonblocking_write_buffer_size(BUFSIZE, playback_samples_remaining * PLAYBACK_FRAMESIZE);
    if (len == 0) {
	//g_print("process_audio: buffering done\n");
	return 2;
    } else if (len < 0) {
	d_print("process_audio: len == %ld, buffer underflow\n", len);
	return 3;
    }

    n_samples_to_read = len/PLAYBACK_FRAMESIZE ;

    if (n_samples_to_read*PLAYBACK_FRAMESIZE != len)
	d_print("process_audio: ACK!!\n") ;

    p_char = (unsigned char *)audio_buffer ;
    p_short = (short *)audio_buffer ;
    p_int = (int *)audio_buffer ;
    p_char2 = (unsigned char *)audio_buffer2;
    p_short2 = (short *)audio_buffer2;
    p_int2 = (int *)audio_buffer2;
    

    /* for now force playback to 16 bit... */
    #define BYTESPERSAMPLE 2

    if(audio_type == SNDFILE_TYPE) {
	if(BYTESPERSAMPLE < 3) {
	    n_read = sf_readf_short(sndfile, p_short, n_samples_to_read) ;
	} else {
	    n_read = sf_readf_int(sndfile, p_int, n_samples_to_read) ;
	}
    }

    // single selected channel, playback to both stereo outputs
    if (stereo && (audio_view.channel_selection_mask != 0x03)) {
	for(frame = 0  ; frame < n_read ; frame++) {
	    i = frame * (stereo + 1) ;
	    if(BYTESPERSAMPLE < 3) {
		// playback for left, right, or both channels
		if (audio_view.channel_selection_mask == 0x01) {
		    // play LEFT channel in both L and R outputs
		    p_short[i+1] = p_short[i];
		} else if (audio_view.channel_selection_mask == 0x02) {
		    // play RIGHT channel in both L and R outputs
		    p_short[i] = p_short[i+1];
		}
	    } else {
		// playback for left, right, or both channels
		if (audio_view.channel_selection_mask == 0x01) {
		    // play LEFT channel in both L and R outputs
		    p_int[i+1] = p_int[i];
		} else if (audio_view.channel_selection_mask == 0x02) {
		    // play RIGHT channel in both L and R outputs
		    p_int[i] = p_int[i+1];
		}
	    }
	}
    }
    
    // playback - send data to hw device
    if (!stereo && (channels == 2)) {
	// play mono file on stereo-only hardware
	// copy single-channel source to both L and R channels of a stereo buffer
	for (frame = 0  ; frame < n_read/2 ; frame++) {
	    if(BYTESPERSAMPLE < 3) {
		p_short2[frame << 1] = p_short[frame];
		p_short2[(frame << 1) + 1] = p_short[frame];
	    } else {
		p_int2[frame << 1] = p_int[frame];
		p_int2[(frame << 1) + 1] = p_int[frame];
	    }
	}
	audio_device_write(p_char2, len);
	
	for (frame = n_read>>1  ; frame < n_read ; frame++) {
	    if(BYTESPERSAMPLE < 3) {
		p_short2[(frame - (n_read >> 1)) << 1] = p_short[frame];
		p_short2[((frame - (n_read >> 1)) << 1) + 1] = p_short[frame];
	    } else {
		p_int2[(frame - (n_read >> 1)) << 1] = p_int[frame];
		p_int2[((frame - (n_read >> 1)) << 1) + 1] = p_int[frame];
	    }
	}
	audio_device_write(p_char2, len);
    } else {
	// play stereo
	audio_device_write(p_char, len);
    }
    
    #undef BYTESPERSAMPLE
    
    playback_samples_remaining -= n_read;
    
    //d_print("playback bytes written: %ld, samples_remaining: %ld\n", len, playback_samples_remaining);
    if (playback_samples_remaining <= 0) {
	extern int audio_is_looping;
	if (audio_is_looping) {
	    playback_samples_remaining = playback_end_position - playback_start_position;
	    position_wavefile_pointer(playback_start_position);
	    looped_count++;
	    //g_print("process_audio loop %lu: zeros needed = %ld\n", looped_count, zeros_needed);
	} else {
	    // write zeros to align buffer
	    unsigned char zeros[1024];
	    memset(zeros,0,sizeof(zeros)) ;
	    do {
		len = audio_device_write(zeros, MIN(zeros_needed, sizeof(zeros))) ;
		zeros_needed -= len;
		//d_print("playback zero bytes written: %ld\n", len);
	    } while (len >= 0 && zeros_needed > 0) ;
	    return 1 ;
	}
    }

    return 0 ;
}
#endif

void stop_playback(unsigned int force)
{
    // remember current playback position to know where to start playback next
    extern int audio_is_looping;
    if ((get_processed_samples() == 0) && (!audio_is_looping)) {
      // The playback buffer is already empty. Since we cannot determine the proper playback position,
      // just assume the playback stopped at the end of the region_of_interest:
      long last;
      get_region_of_interest(&playback_startplay_position, &last, &audio_view);
    } else {
      playback_startplay_position = audio_view.cursor_position;
    }
    
    if (force > 0)
      force = 1;
    /*
    if (!force) {
	long new_playback = get_processed_samples();
	long old_playback, cycles = 0;

	while (playback_samples_remaining > 0) {
	    usleep(1000);
	    old_playback = new_playback;
	    new_playback = get_processed_samples();

	    // check if more samples have been processed. quit if stale
	    if (old_playback == new_playback) {
	      force = 1;
	      d_print("Playback appears frozen, force stop\n");
	      break;
	    }
	    
	    // force break anyway, if this takes too long
	    if (++cycles > 300) {
		force = 1;
		break;
	    }
	}
    }
    usleep(100);
    if (force)
	d_print("stop_playback audio_device_close, force = 1\n");
    */
    audio_device_close(1-force) ;
    audio_state = AUDIO_IS_IDLE ;
}

void sndfile_truncate(long n_samples)
{
    sf_count_t total_samples =  n_samples ;
    if(sf_command(sndfile, SFC_FILE_TRUNCATE, &total_samples, sizeof(total_samples)))
	warning("Libsndfile reports truncation of audio file failed") ;
}

int close_wavefile(struct view *v)
{
    if(audio_type == SNDFILE_TYPE) {
	if(sndfile != NULL) {
	    sf_close(sndfile) ;
	}
	audio_device_close(0) ;
	sndfile = NULL ;
    }

    return 1 ;
}

void save_as_wavfile(char *filename_new, long first_sample, long last_sample)
{
    int fd_new ;
    SNDFILE *sndfile_new ;
    SF_INFO sfinfo_new ;

    long total_samples ;
    long total_bytes ;

    total_samples =  last_sample-first_sample+1 ;

    if(total_samples < 0) {
	warning("Invalid selection") ;
	return ;
    }

    total_bytes = total_samples*FRAMESIZE ;

    fd_new = open(filename_new, O_RDONLY) ;

    if(fd_new > -1) {
	char buf[PATH_MAX] ;
	close(fd_new) ;
	snprintf(buf, sizeof(buf), "%s exists, overwrite ?", filename_new) ;
	if(yesno(buf))  {
	    return ;
	}
    }


    sfinfo_new = sfinfo ;
    sfinfo_new.frames = total_samples ;

    if (! (sndfile_new = sf_open (filename_new, SFM_WRITE, &sfinfo_new))) {
	/* Open failed so print an error message. */

	char buf[PATH_MAX] ;
	snprintf(buf, sizeof(buf), "Failed to save selection %s", filename_new) ;
	warning(buf) ;
	return ;
    } ;

    push_status_text("Saving selection") ;


    /* something like this, gotta buffer this or the disk head will
       burn a hole in the platter */

    position_wavefile_pointer(first_sample) ;

    {
	long n_copied  ;

#define TMPBUFSIZE     (SBW*1000)
	unsigned char buf[TMPBUFSIZE] ;
	long framebufsize = (TMPBUFSIZE/FRAMESIZE) * FRAMESIZE ;

	update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

	for(n_copied = 0 ; n_copied < total_bytes ; n_copied += framebufsize) {
	    long n_to_copy = framebufsize ;

#ifdef MAC_OS_X /* MacOSX */
	    usleep(2) ; // prevents segfault on OSX, who knows, something to do with status bar update...
#endif

	    update_status_bar((gfloat)(n_copied)/(gfloat)(total_bytes),STATUS_UPDATE_INTERVAL,FALSE) ;

	    if(n_copied + n_to_copy > total_bytes) n_to_copy = total_bytes - n_copied ;

	    n_to_copy = sf_read_raw(sndfile, buf, n_to_copy) ;
	    sf_write_raw(sndfile_new, buf, n_to_copy) ;
	}

    }

    update_status_bar((gfloat)0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    sf_close(sndfile_new) ;

    pop_status_text() ;

}

void save_selection_as_wavfile(char *filename_new, struct view *v)
{
    //int fd_new ;
    //SNDFILE *sndfile_new ;
    //SF_INFO sfinfo_new ;

    long total_samples =  v->selected_last_sample-v->selected_first_sample+1 ;

    if(total_samples < 0 || total_samples > v->n_samples) {
	warning("Invalid selection") ;
	return ;
    }

    save_as_wavfile(filename_new, v->selected_first_sample, v->selected_last_sample) ;
}

int is_valid_audio_file(char *filename)
{
    SNDFILE *sndfile ;
    SF_INFO sfinfo ;

    sfinfo.format = 0 ;

    if((sndfile = sf_open(filename, SFM_RDWR, &sfinfo)) != NULL) {
	sf_close(sndfile) ;
	audio_type = SNDFILE_TYPE ;
	return 1 ;
    }

    return 0 ;
}


struct sound_prefs open_wavefile(char *filename, struct view *v)
{
    struct sound_prefs wfh ;

    /* initialize all wfh structure members to defaults.  Will be overwritten on succesful file open */
    wfh.rate = 44100 ;
    wfh.n_channels = 2 ;
    wfh.stereo = 1 ;
    wfh.n_samples = 2 ;
    wfh.playback_bits = wfh.bits = 16 ;
    wfh.max_allowed = MAXSAMPLEVALUE-1 ;
    wfh.wavefile_fd = wavefile_fd ;
    wfh.sample_buffer_exists = FALSE ;

    if(close_wavefile(v)) {
	wfh.successful_open = TRUE ;
    } else {
	wfh.successful_open = FALSE ;
	return wfh ;
    }


    if(audio_type == SNDFILE_TYPE) {
	if (! (sndfile = sf_open (filename, SFM_RDWR, &sfinfo))) {
	    /* Open failed so print an error message. */

	    char buf[80+PATH_MAX] ;
	    snprintf(buf, sizeof(buf), "Failed to open  %s, no permissions or unknown audio format", filename) ;
	    warning(buf) ;
	    wfh.successful_open = FALSE ;
	    return wfh ;

	    /* Print the error message from libsndfile. */
    /*          sf_perror (NULL) ;  */
    /*          return  1 ;  */
	} ;
    }

    wfh.wavefile_fd = 1 ;


    if(audio_type == SNDFILE_TYPE) {
	/* determine soundfile properties */
	wfh.rate = sfinfo.samplerate ;
	wfh.n_channels = sfinfo.channels ;
	wfh.stereo = stereo = sfinfo.channels-1 ;

	wfh.n_samples = sfinfo.frames ;
	
	switch(sfinfo.format & 0x00000F) {
	    case SF_FORMAT_PCM_U8 : BYTESPERSAMPLE=1 ; MAXSAMPLEVALUE = 1 << 8 ; break ;
	    case SF_FORMAT_PCM_S8 : BYTESPERSAMPLE=1 ; MAXSAMPLEVALUE = 1 << 7 ; break ;
	    case SF_FORMAT_PCM_16 : BYTESPERSAMPLE=2 ; MAXSAMPLEVALUE = 1 << 15 ; break ;
	    case SF_FORMAT_PCM_24 : BYTESPERSAMPLE=3 ; MAXSAMPLEVALUE = 1 << 23 ; break ;
	    case SF_FORMAT_PCM_32 : BYTESPERSAMPLE=4 ; MAXSAMPLEVALUE = 1 << 31 ; break ;
	    default : warning("Soundfile format not allowed") ; break ;
	}

	/* do some simple error checking on the wavfile header , so we don't seek data where it isn't */
	if(wfh.n_samples < 2) {
	    char tmp[140] ;
	    snprintf(tmp, sizeof(tmp), "Audio file is possibly corrupt, only %ld samples reported by audio header", wfh.n_samples) ;
	    info(tmp) ;
	    if(sndfile != NULL) {
		sf_close(sndfile) ;
	    }
	    wfh.successful_open = FALSE ;
	    return wfh ;
	}
    }
    FRAMESIZE = BYTESPERSAMPLE*wfh.n_channels ;
    PLAYBACK_FRAMESIZE = 2*wfh.n_channels ;

    wfh.playback_bits = audio_bits = wfh.bits = BYTESPERSAMPLE*8 ;

    wfh.max_allowed = MAXSAMPLEVALUE-1 ;

    gwc_window_set_title(filename) ;

    return wfh ;
}

void position_wavefile_pointer(long sample_number)
{
    if(audio_type == SNDFILE_TYPE) {
	sf_seek(sndfile, sample_number, SEEK_SET) ;
    }
}

int read_raw_wavefile_data(char buf[], long first, long last)
{
    long n = last - first + 1 ;
    int n_read = 0 ;
    int n_bytes_read = 0 ;
    int bufsize = n * FRAMESIZE ;

    position_wavefile_pointer(first) ;

    if(audio_type == SNDFILE_TYPE) {
	n_bytes_read = sf_read_raw(sndfile, buf, bufsize) ;
	return n_bytes_read/FRAMESIZE ;
    }

    return n_read ;
}

int write_raw_wavefile_data(char buf[], long first, long last)
{
    long n = last - first + 1 ;
    int n_read ;

    position_wavefile_pointer(first) ;

    n_read = sf_write_raw(sndfile, buf, n*FRAMESIZE) ;

    return n_read/FRAMESIZE ;
}

int read_wavefile_data(long left[], long right[], long first, long last)
{
    long n = last - first + 1 ;
    long s_i = 0 ;
    long bufsize_long ;
    long j ;
    int *p_int ;

    position_wavefile_pointer(first) ;
    p_int = (int *)audio_buffer ;
    bufsize_long = sizeof(audio_buffer) / sizeof(long) ;

    while(s_i < n) {
	long n_read = 0;

#define TRY_NEW_ABSTRACTION_NOT
#ifdef TRY_NEW_ABSTRACTION
	n_read = read_raw_wavefile_data((char *)p_int, first, last) ;
#else
	long n_this = MIN((n-s_i)*(stereo+1), bufsize_long) ;
	if(audio_type == SNDFILE_TYPE) {
	    n_read = sf_read_int(sndfile, p_int, n_this) ;
	}

#endif


	for(j = 0  ; j < n_read ; ) {

	    left[s_i] = p_int[j] ;
	    j++ ;

	    if(stereo) {
		right[s_i] = p_int[j] ;
		j++ ;
	    } else {
		right[s_i] = left[s_i] ;
	    }

	    s_i++ ;
	}

	if(n_read == 0) {
	    char tmp[100] ;
	    snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first, last) ;
	    warning(tmp) ;
	    exit(1) ;
	}
    }

    return s_i ;
}

int read_fft_real_wavefile_data(fftw_real left[], fftw_real right[], long first, long last)
{
    long n = last - first + 1 ;
    long s_i = 0 ;
    long j ;

    double *buffer = (double *)audio_buffer ;

    int bufsize_double = sizeof(audio_buffer) / sizeof(double) ;

    position_wavefile_pointer(first) ;

    if(audio_type != SNDFILE_TYPE) {
	long pos = first ;
	short *buffer2 ;
	int bufsize_short ;

	buffer2 = (short *)audio_buffer2 ;
	bufsize_short = sizeof(audio_buffer2) / sizeof(short) ;

	while(s_i < n) {
	    long n_this = MIN((n-s_i), bufsize_short) ;
	    int n_read = read_raw_wavefile_data((char *)audio_buffer2, pos, pos+n_this-1) ;

	    pos += n_read ;

	    for(j = 0  ; j < n_read ; j++) {
		left[s_i] = buffer2[j] ;
		if(stereo) {
		    j++ ;
		    right[s_i] = buffer2[j] ;
		} else {
		    right[s_i] = left[s_i] ;
		}
		left[s_i] /= MAXSAMPLEVALUE ;
		right[s_i] /= MAXSAMPLEVALUE ;
		if(first == 0) {
/*  		    fprintf(stderr, "%4d %d %d\n", (int)s_i, (int)left[s_i], (int)right[s_i]) ;  */
		}
		s_i++ ;
	    }

	    if(n_read == 0) {
		char tmp[100] ;
		snprintf(tmp, sizeof(tmp), "read_fft_real Attempted to read past end of audio, first=%ld, last=%ld", first, last) ;
		warning(tmp) ;
		//exit(1) ;
	    }

	}
    } else {
	while(s_i < n) {
	    long n_this = MIN((n-s_i)*(stereo+1), bufsize_double) ;
	    long n_read = sf_read_double(sndfile, buffer, n_this) ;

	    for(j = 0  ; j < n_read ; j++) {
		left[s_i] = buffer[j] ;
		if(stereo) {
		    j++ ;
		    right[s_i] = buffer[j] ;
		} else {
		    right[s_i] = left[s_i] ;
		}
		s_i++ ;
	    }

	    if(n_read == 0) {
		char tmp[100] ;
		snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first, last) ;
		warning(tmp) ;
		exit(1) ;
	    }

	}
    }

    return s_i ;
}

int read_float_wavefile_data(float left[], float right[], long first, long last)
{
    long n = last - first + 1 ;
    long s_i = 0 ;
    long j ;

    float *buffer = (float *)audio_buffer ;

    int bufsize_float = sizeof(audio_buffer) / sizeof(float) ;

    position_wavefile_pointer(first) ;

    while(s_i < n) {
	long n_this = MIN((n-s_i)*(stereo+1), bufsize_float) ;
	long n_read = sf_read_float(sndfile, buffer, n_this) ;

	for(j = 0  ; j < n_read ; j++) {
	    left[s_i] = buffer[j] ;
	    if(stereo) {
		j++ ;
		right[s_i] = buffer[j] ;
	    } else {
		right[s_i] = left[s_i] ;
	    }
	    s_i++ ;
	}

	if(n_read == 0) {
	    char tmp[100] ;
	    snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first, last) ;
	    warning(tmp) ;
	    exit(1) ;
	}

    }

    return s_i ;
}

int sf_write_values(void *ptr, int n_samples)
{
    int n = 0 ;

    if(n_samples > 0) {
	n = sf_write_int(sndfile, ptr, n_samples) ;
    }

    return n * BYTESPERSAMPLE ;
}

long n_in_buf = 0 ;

int WRITE_VALUE_TO_AUDIO_BUF(char *ivalue)
{
    int i ;
    int n_written = 0 ;


    if(BYTESPERSAMPLE+n_in_buf > MAXBUFSIZE) {
	n_written = sf_write_values(audio_buffer, n_in_buf/BYTESPERSAMPLE) ;
	n_in_buf = 0 ;
    }

    for(i = 0 ; i < BYTESPERSAMPLE ; i++, n_in_buf++)
	audio_buffer[n_in_buf] = ivalue[i] ;

    return n_written ;
}

#define FLUSH_AUDIO_BUF(n_written) {\
	n_written += sf_write_values(audio_buffer, n_in_buf/BYTESPERSAMPLE) ;\
	n_in_buf = 0 ;\
    }

    

int write_fft_real_wavefile_data(fftw_real left[], fftw_real right[], long first, long last)
{
    long n = last - first + 1 ;
    long j ;
    long n_written = 0 ;

/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    double buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first) ;

    for(j = 0  ; j < n ; j++) {
	buf[n_in_buf] = left[j] ;
	n_in_buf++ ;
	if(stereo) {
	    buf[n_in_buf] = right[j] ;
	    n_in_buf++ ;
	}

	if(n_in_buf == MAXBUF) {
	    n_written += sf_write_double(sndfile, buf, n_in_buf) ;
	    n_in_buf = 0 ;
	}
    }

    if(n_in_buf > 0) {
	n_written += sf_write_double(sndfile, buf, n_in_buf) ;
	n_in_buf = 0 ;
    }

    return n_written/2 ;
#undef MAXBUF
}

int write_float_wavefile_data(float left[], float right[], long first, long last)
{
    long n = last - first + 1 ;
    long j ;
    long n_written = 0 ;

/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    float buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first) ;

    for(j = 0  ; j < n ; j++) {
	buf[n_in_buf] = left[j] ;
	n_in_buf++ ;
	if(stereo) {
	    buf[n_in_buf] = right[j] ;
	    n_in_buf++ ;
	}

	if(n_in_buf == MAXBUF) {
	    n_written += sf_write_float(sndfile, buf, n_in_buf) ;
	    n_in_buf = 0 ;
	}
    }

    if(n_in_buf > 0) {
	n_written += sf_write_float(sndfile, buf, n_in_buf) ;
	n_in_buf = 0 ;
    }

    return n_written/2 ;
#undef MAXBUF
}

int write_wavefile_data(long left[], long right[], long first, long last)
{
    long n = last - first + 1 ;
    long j ;
    long n_written = 0 ;
/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    int buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first) ;

    for(j = 0  ; j < n ; j++) {
	buf[n_in_buf] = left[j] ;
	n_in_buf++ ;
	if(stereo) {
	    buf[n_in_buf] = right[j] ;
	    n_in_buf++ ;
	}

	if(n_in_buf == MAXBUF) {
	    n_written += sf_write_int(sndfile, buf, n_in_buf) ;
	    n_in_buf = 0 ;
	}
    }

    if(n_in_buf > 0) {
	n_written += sf_write_int(sndfile, buf, n_in_buf) ;
	n_in_buf = 0 ;
    }

    return n_written/2 ;
#undef MAXBUF
}

void flush_wavefile_data(void)
{
    fsync(wavefile_fd) ;
}

