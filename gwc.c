/*****************************************************************************
*   Gnome Wave Cleaner Version 0.21
*   Copyright (C) 2001,2002,2003,2004,2005,2006 Jeffrey J. Welty
*
*   Gnome Wave Cleaner Version 0.30
*   2017 clixt.net
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

/* gwc.c */

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "gwc.h"
#include <gtk/gtk.h>

#include "soundfile.h"
#include "audio_edit.h"
#include <sndfile.h>

#include "icons/amplify.xpm"
#include "icons/pinknoise.xpm"
#include "icons/declick.xpm"
#include "icons/declick_w.xpm"
#include "icons/declick_m.xpm"
#include "icons/decrackle.xpm"
#include "icons/estimate.xpm"
#include "icons/filter.xpm"
#include "icons/noise_sample.xpm"
#include "icons/remove_noise.xpm"
#include "icons/start.xpm"
#include "icons/stop.xpm"
#include "icons/zoom_sel.xpm"
#include "icons/zoom_in.xpm"
#include "icons/zoom_out.xpm"
#include "icons/undo.xpm"
#include "icons/view_all.xpm"
#include "icons/select_all.xpm"
#include "icons/spectral.xpm"
#include "icons/silence.xpm"

GtkWidget *main_window;

char pathname[PATH_MAX+1] = "./";
GtkWidget *dial[2];
GtkWidget *audio_canvas_w;

GtkWidget *audio_drawing_area;
GdkPixmap *audio_pixmap = NULL;
GdkPixmap *highlight_pixmap = NULL;
GdkPixmap *cursor_pixmap = NULL;
GtkObject *scroll_pos;
GtkWidget *hscrollbar;
GtkWidget *detect_only_widget;
GtkWidget *leave_click_marks_widget;

GtkWidget *l_file_time;
GtkWidget *l_file_samples;
GtkWidget *l_first_time;
GtkWidget *l_selected_time;
GtkWidget *l_last_time;
GtkWidget *l_samples;
GtkWidget *l_samples_perpixel;

/* toolbars */
GtkWidget *status_bar;
GtkWidget *edit_toolbar;
GtkWidget *transport_toolbar;
/* The file selection widget */
GtkWidget *file_selector;

struct common_prefs app_prefs;
struct sound_prefs prefs;
struct denoise_prefs denoise_prefs;
struct view audio_view;
struct click_data click_data;
int audio_playback = FALSE;
int audio_is_looping = FALSE;
int batch_mode = 0 ;
gint playback_timer = -1 ;
gint cursor_timer = -1;
gint spectral_view_flag = FALSE;
gint repair_clicks = 1;
double view_scale = 1.0;
double declick_sensitivity = 0.75;
double weak_declick_sensitivity = 1.00;
double strong_declick_sensitivity = 0.75;
double weak_fft_declick_sensitivity = 3.0 ;
double strong_fft_declick_sensitivity = 5.0 ;
int declick_detector_type = FFT_DETECT ;
double stop_key_highlight_interval;
double song_key_highlight_interval;
double song_mark_silence;
int sonogram_log;
gint declick_iterate_flag = 0;
double decrackle_level = 0.2;
gint decrackle_window = 2000;
gint decrackle_average = 3;

extern double spectral_amp;

#ifdef HAVE_ALSA
char audio_device[256]="hw:0,0";
#else
char audio_device[256]="/dev/dsp";
#endif

gint doing_statusbar_update = FALSE;

DENOISE_DATA denoise_data = { 0, 0, 0, 0, FALSE };

gint debug = 0;
static int audio_debug = 0;

/* string to store the chosen filename */
gchar *selected_filename;
gchar save_selection_filename[PATH_MAX+1];
gchar wave_filename[PATH_MAX+1];
gchar last_filename[PATH_MAX+1];

long markers[MAX_MARKERS];
long n_markers = 0;
long num_song_markers = 0;
long song_markers[MAX_MARKERS];

gint file_is_open = FALSE;
gint file_processing = FALSE;
long playback_samples_per_block = 0;
long playback_startplay_position;

int count = 0;

void d_print(char *fmt, ...)
{
    if (debug) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
    }
}

void usage(char *prog)
{
	fprintf(stderr, "Usages:\n\%s\n\%s [file]\n",prog, prog);
	exit(EXIT_FAILURE);
}

void audio_debug_print(char *fmt, ...)
{
    if (audio_debug) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
    }
}

char *sample_to_time_text(long i, int rate, char *prefix, char *buf)
{
    int m, s, ms;

    m = i / (rate * 60);
    i -= m * rate * 60;

    s = i / rate;
    i -= s * rate;
    ms = 1000 * i / rate;

    sprintf(buf, "%s%d:%02d:%03d", prefix, m, s, ms);
    return buf;
}

void shellsort_long(long a[], int n)
{
    int gap, i, j;
    long tmp;

    for (gap = n / 2; gap > 0; gap /= 2) {
	for (i = gap; i < n; i++) {
	    for (j = i - gap; j >= 0 && a[j] > a[j + gap]; j -= gap) {
		tmp = a[j];
		a[j] = a[j + gap];
		a[j + gap] = tmp;
	    }
	}
    }

}

static int is_region_selected(void)
{
    if (!audio_view.selection_region) {
        info("Please select a region of audio data.");
        return 0;
    }
    return 1;
}

void display_times(void)
{
    char buf[50];
    long first, last;
    get_region_of_interest(&first, &last, &audio_view);

    gtk_label_set_text(GTK_LABEL(l_file_time),
		       sample_to_time_text(prefs.n_samples, prefs.rate, "Total ", buf));
    gtk_label_set_text(GTK_LABEL(l_first_time),
		       sample_to_time_text(first, prefs.rate, "First ", buf));
    gtk_label_set_text(GTK_LABEL(l_last_time),
		       sample_to_time_text(last, prefs.rate, "Last ", buf));
    gtk_label_set_text(GTK_LABEL(l_selected_time),
		       sample_to_time_text(last-first-1, prefs.rate, "Selected ", buf));
    sprintf(buf, "Samples: %ld", last - first + 1);
    gtk_label_set_text(GTK_LABEL(l_samples), buf);
    
    sprintf(buf, "Samples per pixel: %ld", (audio_view.last_sample - audio_view.first_sample) / audio_view.canvas_width);
    gtk_label_set_text(GTK_LABEL(l_samples_perpixel), buf);
    
    sprintf(buf, "Track samples: %ld", audio_view.n_samples);
    gtk_label_set_text(GTK_LABEL(l_file_samples), buf);
}

void set_scroll_bar(long n, long first, long last)
{
    GtkAdjustment *a = (GtkAdjustment *) scroll_pos;
    double dn = n;
    double df = first;
    double dl = last;
    double ps = dl - df;

    a->lower = 0;
    a->upper = dn;
    a->value = df;
    a->page_size = ps;
    a->step_increment = ps / 8.0;
    a->page_increment = ps * 0.95;

    gtk_adjustment_changed(a);
}

void scroll_bar_changed(GtkWidget * widget, gpointer data)
{
    GtkAdjustment *a = (GtkAdjustment *) scroll_pos;

    audio_view.first_sample = MAX(0, MIN(prefs.n_samples - 1, a->value));
    audio_view.last_sample = MAX(0, MIN(prefs.n_samples - 1, a->value + a->page_size));

    main_redraw(FALSE, TRUE);
    /* pause 1/3 second to allow the user to release the mouse button */
    usleep(333);
}

void get_region_of_interest(long *first, long *last, struct view *v)
{
    if (v->selection_region == FALSE) {
	*first = v->first_sample;
	//*last = v->last_sample;
	// if nothing is selected,
	// consider everything from beginning of the view to the end of the file:
	*last = v->n_samples - 1;
    } else {
	*first = v->selected_first_sample;
	*last = v->selected_last_sample;
    }
}

void load_preferences(void)
{
    gnome_config_push_prefix(APPNAME"/config/");
    strcpy(pathname, gnome_config_get_string("pathname=./"));
    strcpy(last_filename, gnome_config_get_string("last_filename=./"));
    app_prefs.window_width = gnome_config_get_int("window_width=1200");
    app_prefs.window_height = gnome_config_get_int("window_height=800");
    prefs.rate = gnome_config_get_int("rate=44100");
    prefs.bits = gnome_config_get_int("bits=16");
    prefs.stereo = gnome_config_get_int("stereo=1");
    audio_view.first_sample = gnome_config_get_int("first_sample_viewed=-1");
    audio_view.last_sample = gnome_config_get_int("last_sample_viewed=-1");
    num_song_markers = 0;
    audio_view.channel_selection_mask = gnome_config_get_int("channel_selection_mask=0");
    weak_declick_sensitivity = gnome_config_get_float("weak_declick_sensitivity=1.0");
    strong_declick_sensitivity = gnome_config_get_float("strong_declick_sensitivity=0.75");
    declick_iterate_flag = gnome_config_get_int("declick_iterate=0");
    weak_fft_declick_sensitivity = gnome_config_get_float("weak_fft_declick_sensitivity=3.0");
    strong_fft_declick_sensitivity = gnome_config_get_float("strong_fft_declick_sensitivity=5.0");
    declick_detector_type = gnome_config_get_int("declick_detector_type=0");
    decrackle_level = gnome_config_get_float("decrackle_level=0.2");
    decrackle_window = gnome_config_get_int("decrackle_window=2000");
    decrackle_average = gnome_config_get_int("decrackle_average=3");
    stop_key_highlight_interval = gnome_config_get_float("stop_key_highlight_interval=0.5");
    song_key_highlight_interval = gnome_config_get_float("song_key_highlight_interval=15");
    song_mark_silence = gnome_config_get_float("song_mark_silence=2.0");
    sonogram_log = gnome_config_get_float("sonogram_log=0");
    strcpy(audio_device, gnome_config_get_string("audio_device=hw:0,0"));
    gnome_config_pop_prefix();
}

void save_preferences(void)
{
    gnome_config_push_prefix(APPNAME"/config/");
    gnome_config_set_string("pathname", pathname);
    gnome_config_set_string("last_filename", last_filename);
    gnome_config_set_int("window_width", app_prefs.window_width);
    gnome_config_set_int("window_height", app_prefs.window_height);
    gnome_config_set_int("rate", prefs.rate);
    gnome_config_set_int("bits", prefs.bits);
    gnome_config_set_int("stereo", prefs.stereo);
    gnome_config_set_int("first_sample_viewed", audio_view.first_sample);
    gnome_config_set_int("last_sample_viewed", audio_view.last_sample);
    gnome_config_set_int("channel_selection_mask", audio_view.channel_selection_mask);
    gnome_config_set_float("weak_declick_sensitivity", weak_declick_sensitivity);
    gnome_config_set_float("strong_declick_sensitivity", strong_declick_sensitivity);
    gnome_config_set_int("declick_iterate", declick_iterate_flag);
    gnome_config_set_float("weak_fft_declick_sensitivity", weak_fft_declick_sensitivity);
    gnome_config_set_float("strong_fft_declick_sensitivity", strong_fft_declick_sensitivity);
    gnome_config_set_int("declick_detector_type", declick_detector_type);
    gnome_config_set_float("decrackle_level", decrackle_level);
    gnome_config_set_int("decrackle_window", decrackle_window);
    gnome_config_set_int("decrackle_average", decrackle_average);
    gnome_config_set_float("stop_key_highlight_interval", stop_key_highlight_interval);
    gnome_config_set_float("song_key_highlight_interval", song_key_highlight_interval);
    gnome_config_set_float("song_mark_silence", song_mark_silence);
    gnome_config_set_int("sonogram_log", sonogram_log);
    gnome_config_set_string("audio_device", audio_device);
    gnome_config_sync();
    gnome_config_pop_prefix();
}

/*  void main_set_preferences(GtkWidget * widget, gpointer data)  */
/*  {  */
/*      preferences_dialog(prefs);  */
/*  }  */

void display_message(char *msg, char *title) 
{
    GtkWidget *dlg, *txt;

    dlg = gtk_dialog_new_with_buttons(title,
    				      GTK_WINDOW(main_window),
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_STOCK_OK,
				      GTK_RESPONSE_NONE,
				      NULL);

    txt = gtk_label_new(msg);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), txt, TRUE, TRUE, 0) ;

    gtk_widget_show_all(dlg) ;

    gtk_dialog_run(GTK_DIALOG(dlg)) ;

    gtk_widget_destroy(txt) ;
    gtk_widget_destroy(dlg) ;

    main_redraw(FALSE, TRUE);
}

void warning(char *msg)
{
    display_message(msg, "WARNING");
}

void info(char *msg)
{
    display_message(msg, "");
}

int yesnocancel(char *msg)
{
    GtkWidget *dlg, *text;
    gint dres;

    dlg = gtk_dialog_new_with_buttons(
	"Question",
	NULL,
	GTK_DIALOG_DESTROY_WITH_PARENT,
	GTK_STOCK_YES,
	GTK_RESPONSE_YES,
	GTK_STOCK_NO,
	GTK_RESPONSE_NO,
	GTK_STOCK_CANCEL,
	GTK_RESPONSE_CANCEL,
	NULL);

    text = gtk_label_new(msg);
    gtk_widget_show(text);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), text, TRUE, TRUE, 0);

    gtk_widget_show_all(dlg) ;

    dres = gtk_dialog_run(GTK_DIALOG(dlg));

    gtk_widget_destroy(dlg) ;


    if (dres == GTK_RESPONSE_NONE || dres == GTK_RESPONSE_CANCEL) {
	dres = 2 ;		/* return we clicked cancel */
    } else if (dres == GTK_RESPONSE_YES) {
	dres = 0 ; 		/* return we clicked yes */
    } else {
	dres = 1 ;
    }

    main_redraw(FALSE, TRUE);

    return dres;
}

int yesno(char *msg)
{
    GtkWidget *dlg, *text;
    int dres;

    dlg = gtk_dialog_new_with_buttons("Question",
    				        GTK_WINDOW(main_window),
				        GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_YES,
					GTK_RESPONSE_YES,
					 GTK_STOCK_NO,
					 GTK_RESPONSE_NO,
					 NULL);

    text = gtk_label_new(msg);
    gtk_widget_show(text);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), text, TRUE, TRUE, 0);

    gtk_widget_show_all(dlg) ;

    dres = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg) ;

    if (dres == GTK_RESPONSE_NONE || dres == GTK_RESPONSE_NO) {
	dres = 1 ;		/* return we clicked no */
    } else {
	dres = 0 ; 		/* return we clicked yes */
    }

    main_redraw(FALSE, TRUE);

    return dres;
}

int prompt_user(char *msg, char *s, int maxlen)
{
    GtkWidget *dlg, *text, *entry ;
    int dres;

    dlg = gtk_dialog_new_with_buttons("Input Requested",
    				        GTK_WINDOW(main_window),
				        GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_OK,
					GTK_RESPONSE_OK,
					 GTK_STOCK_CANCEL,
					 GTK_RESPONSE_CANCEL,
					 NULL);

    text = gtk_label_new(msg);
    gtk_widget_show(text);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), text, TRUE, TRUE, 0);

    entry = gtk_entry_new_with_max_length(maxlen);

    if(strlen(s) > 0)
	gtk_entry_set_text(GTK_ENTRY(entry), s);

    gtk_widget_show(entry);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), entry, TRUE, TRUE, 0);

    gtk_widget_show_all(dlg) ;

    dres = gtk_dialog_run(GTK_DIALOG(dlg));

    if (dres == GTK_RESPONSE_NONE || dres == GTK_RESPONSE_NO) {
	dres = 1 ;		/* return we clicked cancel */
    } else {
	dres = 0 ; 		/* return we clicked yes */
	strcpy(s, gtk_entry_get_text(GTK_ENTRY(entry)));
    }

    gtk_widget_destroy(dlg) ;

    main_redraw(FALSE, TRUE);

    return dres;
}

void show_help(const char *filename)
{
    GError *err = NULL ;
    gboolean r ;

    r = gnome_help_display(filename, NULL, &err) ;

    if(!r) {
	char buf[256] ;
	strcpy(buf, "C/") ;
	strcat(buf, filename) ;

	r = gnome_help_display(buf, NULL, &err) ;

	if(!r) {
	    fprintf(stderr, "gnome_help_display failed: %s\n", err->message) ;
	    g_error_free(err) ;
	}
    }

    main_redraw(FALSE, TRUE);
}

void help(GtkWidget * widget, gpointer data)
{
    show_help("gwc.html") ;
}

void quickstart_help(GtkWidget * widget, gpointer data)
{
    show_help("gwc_qs.html") ;
}

void declick_with_sensitivity(double sensitivity)
{
    long first, last;
    char *result_msg;
    gint leave_click_marks ;

    repair_clicks =
	gtk_toggle_button_get_active((GtkToggleButton *)detect_only_widget) == TRUE ? FALSE : TRUE;

    leave_click_marks =
	gtk_toggle_button_get_active((GtkToggleButton *)leave_click_marks_widget) == TRUE ? TRUE : FALSE ;

    if (repair_clicks == TRUE)
	start_save_undo("Undo declick", &audio_view);

    get_region_of_interest(&first, &last, &audio_view);

    push_status_text(repair_clicks ==
		     TRUE ? "Declicking selection" : "Detecting clicks");

    click_data.max_clicks = MAX_CLICKS;

    result_msg =
	do_declick(&prefs, first, last, audio_view.channel_selection_mask,
		   sensitivity, repair_clicks, &click_data,
		   declick_iterate_flag,leave_click_marks);

    if (repair_clicks == TRUE) {
	resample_audio_data(&prefs, first, last);
	save_sample_block_data(&prefs);
    }

    pop_status_text();
    push_status_text(result_msg);

    if (repair_clicks == TRUE) {
	close_undo();
    }

    main_redraw(FALSE, TRUE);
}

void declick(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {

	file_processing = TRUE;

	if(declick_detector_type == HPF_DETECT)
	    declick_with_sensitivity(strong_declick_sensitivity);
	else
	    declick_with_sensitivity(strong_fft_declick_sensitivity);

	file_processing = FALSE;
    }
}

void declick_weak(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {

	file_processing = TRUE;

	if(declick_detector_type == HPF_DETECT)
	    declick_with_sensitivity(weak_declick_sensitivity);
	else
	    declick_with_sensitivity(weak_fft_declick_sensitivity);

	file_processing = FALSE;
    }
}

void estimate(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	long first, last;
	file_processing = TRUE;
	get_region_of_interest(&first, &last, &audio_view);
	dethunk(&prefs, first, last, audio_view.channel_selection_mask);
	main_redraw(FALSE, TRUE);
	set_status_text("Estimate done.");
	file_processing = FALSE;
    }
}

void manual_declick(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	int doit = TRUE;
	long first, last;
	file_processing = TRUE;
	get_region_of_interest(&first, &last, &audio_view);

	if (last - first > 499) {
	    char msg_buf[1000] ;
	    double n = last-first+1 ;
	    double a = 100 ;
	    double elements = ( n + (n-a) * n * 3 + (n-a) * (n-a) * 2 + n * n ) ;
	    double bytes = elements * sizeof(double) / (double) (1 << 20) ;
	    char *units = "Megabytes" ;

	    if(bytes > 1000) {
		bytes /= (double) 1024 ;
		units = "Gigabytes" ;
	    }

	    if(bytes > 1000) {
		bytes /= (double) 1024 ;
		units = "Terabytes" ;
	    }

	    sprintf(msg_buf, "Repairing > 500 samples may cause a crash\nYou have selected %lg samples, which will require about %8.0lf %s of memory and a long time.",
			    n, bytes, units ) ;
	    doit = FALSE;
	    if (!yesno(msg_buf))
		doit = TRUE;
	}

	if (doit == TRUE) {

	    start_save_undo("Undo declick", &audio_view);

	    push_status_text("Declicking selection");
	    declick_a_click(&prefs, first, last,
			    audio_view.channel_selection_mask);
	    resample_audio_data(&prefs, first, last);
	    save_sample_block_data(&prefs);
	    pop_status_text();
	    set_status_text("Manual declick done.");

	    close_undo();

	    main_redraw(FALSE, TRUE);
	}

	file_processing = FALSE;
    }
}

void decrackle(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	int cancel;
	long first, last;

	file_processing = TRUE;
	push_status_text("Saving undo information");

	start_save_undo("Undo decrackle", &audio_view);
	get_region_of_interest(&first, &last, &audio_view);
	cancel = save_undo_data(first, last, &prefs, TRUE);

	close_undo();

	pop_status_text();

	if (cancel != 1) {
	    push_status_text("Decrackling selection");
	    do_decrackle(&prefs, first, last,
			 audio_view.channel_selection_mask,
			 decrackle_level, decrackle_window,
			 decrackle_average);
	    resample_audio_data(&prefs, first, last);
	    save_sample_block_data(&prefs);
	    pop_status_text();
	    set_status_text("Decrackle done.");
	}

	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void noise_sample(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	if (audio_view.selection_region == TRUE) {
	  file_processing = TRUE;
	  get_region_of_interest(&denoise_data.noise_start, &denoise_data.noise_end, &audio_view);
	  denoise_data.ready = TRUE;
	  load_denoise_preferences() ;
	  //print_noise_sample(&prefs, &denoise_prefs, denoise_data.noise_start, denoise_data.noise_end) ;
	  file_processing = FALSE;
	  set_status_text("Noise sample taken");
	} else {
	  set_status_text("First you have to select a portion of the audio with pure noise");
	}
    }
}

void remove_noise(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	if (denoise_data.ready == FALSE) {
	    warning("Please select the noise sample first");
	} else {
	    get_region_of_interest(&denoise_data.denoise_start,
				   &denoise_data.denoise_end, &audio_view);
	    load_denoise_preferences();
	    print_denoise("remove_noise", &denoise_prefs);
	    {
		int cancel;

		if (denoise_prefs.FFT_SIZE >
		    (denoise_data.noise_end - denoise_data.noise_start +
		     1)) {
		    warning
			("FFT_SIZE must be <= # samples in noise sample!");
		    main_redraw(FALSE, TRUE);
		    file_processing = FALSE ;
		    return;
		}

		push_status_text("Saving undo information");

		start_save_undo("Undo denoise", &audio_view);
		cancel =
		    save_undo_data(denoise_data.denoise_start,
				   denoise_data.denoise_end, &prefs, TRUE);
		close_undo();

		pop_status_text();

		if (cancel != 1) {
		    push_status_text("Denoising selection");
		    denoise(&prefs, &denoise_prefs,
			    denoise_data.noise_start,
			    denoise_data.noise_end,
			    denoise_data.denoise_start,
			    denoise_data.denoise_end,
			    audio_view.channel_selection_mask);
		    resample_audio_data(&prefs, denoise_data.denoise_start,
					denoise_data.denoise_end);
		    save_sample_block_data(&prefs);
		    pop_status_text();
		}
		set_status_text("Denoise done.");

		main_redraw(FALSE, TRUE);
	    }
	}
	file_processing = FALSE;
    }
}

void undo_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	undo(&audio_view, &prefs);
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    } else {
	if (file_is_open == FALSE) {
	    warning("Nothing to Undo Yet.");
	} else
	    warning("Please try Undo when processing or Audio Playback has stopped");
    }
}


void scale_up_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	view_scale *= 1.25;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void cut_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
        if (is_region_selected()) {
            long first, last;
            get_region_of_interest(&first, &last, &audio_view);

            if (first == 0 && last == prefs.n_samples - 1) {
                info("Can't cut ALL audio data from file.");
            } else {
                file_processing = TRUE;
                audioedit_cut_selection(&audio_view);
                main_redraw(FALSE, TRUE);
                file_processing = FALSE;
            }
        }
    }
}

void copy_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
        if (is_region_selected()) {
            file_processing = TRUE;
            audioedit_copy_selection(&audio_view);
            file_processing = FALSE;
        }
    }
}

void paste_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
        if (is_region_selected()) {
            if (audioedit_has_clipdata()) {
                file_processing = TRUE;
                audioedit_paste_selection(&audio_view);
                main_redraw(FALSE, TRUE);
                file_processing = FALSE;
            } else {
                info("No audio data in internal clipboard.");
            }
        }
    }
}

void delete_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
        if (is_region_selected()) {
            long first, last;
            get_region_of_interest(&first, &last, &audio_view);

            if (first == 0 && last == prefs.n_samples - 1) {
                info("Can't delete ALL audio data from file.");
            } else {
                file_processing = TRUE;
                audioedit_delete_selection(&audio_view);
                main_redraw(FALSE, TRUE);
                file_processing = FALSE;
            }
        }
    }
}

void silence_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
        if (is_region_selected()) {

	    if(!yesno("Insert silence can take a long time, continue?")) {
		file_processing = TRUE;
		audioedit_insert_silence(&audio_view);
		main_redraw(FALSE, TRUE);
		file_processing = FALSE;
	    }
        }
    }
}

void scale_reset_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	view_scale = 1.00;
	spectral_amp = 1.00;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void scale_down_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	view_scale /= 1.25;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void stop_all_playback_functions(GtkWidget * widget, gpointer data)
{
    audio_playback = FALSE;
    audio_is_looping = FALSE;
    
    if (playback_timer != -1) {
	gtk_timeout_remove(playback_timer);
	playback_timer = -1 ;
    }
    if (cursor_timer != -1) {
        gtk_timeout_remove(cursor_timer);    
	cursor_timer = -1 ;
    }
    stop_playback(0);

    // show current playback position, if outside of the view
    if (audio_view.cursor_position > audio_view.last_sample) {
	long view_size_half = (audio_view.last_sample - audio_view.first_sample) >> 1;
	audio_view.first_sample = audio_view.cursor_position - view_size_half;
	audio_view.last_sample = audio_view.cursor_position + view_size_half;
	if (audio_view.first_sample < 0)
	    audio_view.first_sample = 0;
	if (audio_view.last_sample > prefs.n_samples - 1)
	    audio_view.last_sample = prefs.n_samples - 1;
	set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample, audio_view.last_sample);
    }
}

void gnome_flush(void)
{
    while (gtk_events_pending())
	gtk_main_iteration();
}

/*
 * timer callback
 * 
 * run from start_gwc_playback()
 * 
 * call audio_util.c: set_playback_cursor_position() to set cursor_position in audio_view
 * repaint the cursor in the waveform display by main_redraw()
 */
gint update_cursor(gpointer data)
{
    // find out where the playback is right now and update audio_view.cursor_position
    set_playback_cursor_position(&audio_view);
    
    // autoscroll on playback, if no audio selected,
    // and not zoomed in too much
    //
    long cursor_samples_per_pixel = (audio_view.last_sample - audio_view.first_sample) / audio_view.canvas_width;
    if ((audio_view.selection_region == FALSE) && (cursor_samples_per_pixel > 150) &&
      (audio_view.cursor_position > (audio_view.last_sample - 
      ((audio_view.last_sample - audio_view.first_sample) / 30)))) {
      
      long sample_shift = (audio_view.last_sample - audio_view.first_sample) * 0.9;
      //audio_debug_print("autoscrolling %d samples\n", sample_shift) ;
      if ((prefs.n_samples - 1 - audio_view.last_sample) > sample_shift) {
	audio_view.first_sample += sample_shift;
	audio_view.last_sample += sample_shift;
      } else {
	audio_view.first_sample += audio_view.n_samples - audio_view.last_sample - 1;
	audio_view.last_sample = audio_view.n_samples - 1;
      }
      set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample, audio_view.last_sample);
    } else {
      // repaint cursor
      main_redraw(TRUE, TRUE);
    }

    // stop playback
    long first, last, playback_samples_left;
    if (!audio_is_looping) {
      get_region_of_interest(&first, &last, &audio_view) ;
      playback_samples_left = last - get_playback_position();
      if (playback_samples_left < 900) {
	  stop_all_playback_functions(NULL, NULL);
	  audio_debug_print("update_cursor stopped playback, playback_samples_left = %ld\n", playback_samples_left);
      } 
    }
    
    return (TRUE);
}

/*
 * timer callback
 * 
 * init from start_gwc_playback()
 * 
 * call audio_utils.c: process_audio() to load audio data from file into buffer
 */
gint play_a_block(gpointer data)
{
  if (audio_playback)
    process_audio();
  return (TRUE);
}

/* Play audio 
 * 
 * call start_playback() to prepare audio device, buffer & positions
 * start play_a_block() periodically as timer callback
 * start update_cursor() periodically as timer callback
 */
void start_gwc_playback(GtkWidget * widget, gpointer data)
{
    long millisec_per_block;
    long cursor_samples_per_pixel;
    long playback_millisec, cursor_millisec;

    //audio_debug_print("entering start_gwc_playback with audio_playback = %d\n", audio_playback) ;

    if (file_is_open == TRUE && file_processing == FALSE && audio_playback == FALSE) {
	audio_debug_print("\nplayback device: %s\n", audio_device) ;  
	
	playback_samples_per_block = start_playback(audio_device, &audio_view, &prefs, 0.1, 2);
	audio_debug_print("playback_samples_per_block = %ld\n", playback_samples_per_block) ;
	
	if (playback_samples_per_block < 1)
	    return ; // an error occured

	//prefs.rate = 44100
	millisec_per_block = playback_samples_per_block * 1000 / prefs.rate;
	playback_millisec = millisec_per_block / 4;
	    
	cursor_samples_per_pixel = (audio_view.last_sample - audio_view.first_sample) / audio_view.canvas_width;
	cursor_millisec = (cursor_samples_per_pixel * 1000) / prefs.rate;
	/* lower limit of 1/100th second on screen redraws */
	if (cursor_millisec < 20)
	    cursor_millisec = 19;
	    
	audio_playback = TRUE;
	audio_debug_print("play_a_block timer: %ld ms\n", playback_millisec);
	audio_debug_print("update_cursor timer: %ld ms\n", cursor_millisec);
	audio_debug_print("cursor_samples_per_pixel: %ld\n", cursor_samples_per_pixel);
	playback_timer = gtk_timeout_add(playback_millisec, play_a_block, NULL);
	cursor_timer = gtk_timeout_add(cursor_millisec, update_cursor, NULL);

	//play_a_block(NULL);
	//update_cursor(NULL);
    }

    audio_debug_print("leaving start_gwc_playback with audio_playback=%d\n", audio_playback);
    
}

void detect_only_func(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	repair_clicks =
	    gtk_toggle_button_get_active((GtkToggleButton *) widget) ==
	    TRUE ? FALSE : TRUE;
	g_print("detect_only_func called: %s\n", (char *) data);
	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void amplify(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	if (amplify_dialog(prefs, &audio_view)) {
	    long first, last;
	    int cancel;
	    get_region_of_interest(&first, &last, &audio_view);

	    push_status_text("Saving undo information");
	    start_save_undo("Undo amplify", &audio_view);
	    cancel = save_undo_data(first, last, &prefs, TRUE);
	    close_undo();
	    pop_status_text();

	    if (cancel != 1) {
		amplify_audio(&prefs, first, last,
			      audio_view.channel_selection_mask);
		save_sample_block_data(&prefs);
		set_status_text("Amplify done.");
	    }
	    main_redraw(FALSE, TRUE);
	}
	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void reverb(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	if (reverb_dialog(prefs, &audio_view)) {
	    long first, last;
	    int cancel;
	    get_region_of_interest(&first, &last, &audio_view);

	    push_status_text("Saving undo information");
	    start_save_undo("Undo reverb", &audio_view);
	    cancel = save_undo_data(first, last, &prefs, TRUE);
	    close_undo();
	    pop_status_text();

	    if (cancel != 1) {
		reverb_audio(&prefs, first, last,
			      audio_view.channel_selection_mask);
		save_sample_block_data(&prefs);
		set_status_text("Reverb done.");
	    }
	    main_redraw(FALSE, TRUE);
	}
	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void pinknoise_cb(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;

	if (pinknoise_dialog(prefs, &audio_view)) {
	    long first, last;
	    int cancel;
	    get_region_of_interest(&first, &last, &audio_view);

	    push_status_text("Saving undo information");
	    start_save_undo("Undo pinknoise", &audio_view);
	    cancel = save_undo_data(first, last, &prefs, TRUE);
	    close_undo();
	    pop_status_text();

	    if (cancel != 1) {
		pinknoise(&prefs, first, last,
			      audio_view.channel_selection_mask);
		save_sample_block_data(&prefs);
		set_status_text("Amplify done.");
	    }
	    main_redraw(FALSE, TRUE);
	}

	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void filter_cb(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;

	if (filter_dialog(prefs, &audio_view)) {
	    long first, last;
	    int cancel;
	    get_region_of_interest(&first, &last, &audio_view);

	    push_status_text("Saving undo information");
	    start_save_undo("Undo filter", &audio_view);
	    cancel = save_undo_data(first, last, &prefs, TRUE);
	    close_undo();
	    pop_status_text();

	    if (cancel != 1) {
		filter_audio(&prefs, first, last,
			      audio_view.channel_selection_mask);
		save_sample_block_data(&prefs);
		set_status_text("Filter done.");
	    }
	    main_redraw(FALSE, TRUE);
	}

	file_processing = FALSE;
    }
}

void zoom_select(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;

	if(audio_view.selected_first_sample == audio_view.selected_last_sample) {
	    audio_view.first_sample = audio_view.selected_first_sample - prefs.rate+1 ;
	    audio_view.last_sample = audio_view.selected_last_sample + prefs.rate ;
	} else {
	    audio_view.first_sample = audio_view.selected_first_sample;
	    audio_view.last_sample = audio_view.selected_last_sample;
	}

	if(audio_view.selected_first_sample < 0) audio_view.selected_first_sample = 0 ;
	if(audio_view.selected_last_sample > prefs.n_samples-1) audio_view.selected_last_sample = prefs.n_samples-1 ;

	audio_view.selection_region = FALSE;
	set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample,
		       audio_view.last_sample);
	/* set_scroll_bar redraws */
	/*main_redraw(FALSE, TRUE) ; */
	file_processing = FALSE;
    }
}

void select_all(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	audio_view.selected_first_sample = audio_view.first_sample;
	audio_view.selected_last_sample = audio_view.last_sample;
	audio_view.selection_region = TRUE;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void select_markers(GtkWidget * widget, gpointer data)
{
    int i ;
    long new_first = -1 ;
    long new_last = -1 ;

    for (i = 0; i < n_markers; i++) {
	if(markers[i] < audio_view.selected_first_sample) {
	    if(new_first == -1)
		new_first = markers[i] ;
	    else
		new_first = MAX(new_first, markers[i]) ;
	}
	if(markers[i] > audio_view.selected_last_sample) {
	    if(new_last == -1)
		new_last = markers[i] ;
	    else
		new_last = MIN(new_last, markers[i]) ;
	}
    }

    if(new_first != -1) audio_view.selected_first_sample = new_first ;
    if(new_last != -1) audio_view.selected_last_sample = new_last ;

    main_redraw(FALSE, TRUE);
}

void toggle_marker_at(long sample)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	int i;
	int j;

	file_processing = TRUE;

	for (i = 0; i < n_markers; i++) {
	    if (markers[i] == sample) {
		n_markers--;
		for (j = i; j < n_markers; j++)
		    markers[j] = markers[j + 1];
		file_processing = FALSE;
		return;
	    }
	}

	if (n_markers < MAX_MARKERS) {
	    markers[n_markers] = sample;
	    n_markers++;
	} else {
	    warning("Maximum number of markers already set");
	}

	file_processing = FALSE;
    }
}

void split_audio_on_markers(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	int i = 0 ;
	//int first = -1 ;
	unsigned int trackno = 1 ;

	long first_sample = 0 ;
	long last_sample = song_markers[0] ;

/*  	for(i = 0 ; i < num_song_markers ; i++) {  */
/*  	    printf("marker %2d at sample %d\n", i, song_markers[i]) ;  */
/*  	}  */
/*    */
/*  	i=0 ;  */
	

	while(last_sample < 10000 && i < num_song_markers) {
	    first_sample = last_sample ;
	    i++ ;
	    last_sample = song_markers[i] ;
	}

	file_processing = TRUE;

	while(last_sample <= prefs.n_samples-1 && i <= num_song_markers) {

	    char filename[100] ;

	    if(trackno < 10) {
		snprintf(filename,99,"track0%u.cdda.wav",trackno) ;
	    } else {
		snprintf(filename,99,"track%u.cdda.wav",trackno) ;
	    }

	    if(last_sample-first_sample >= 10000) {
		save_as_wavfile(filename, first_sample, last_sample) ;
		printf("Save as wavfile %s %ld->%ld\n", filename, first_sample, last_sample) ;
		trackno++ ;
	    }

	    first_sample=last_sample+1 ;
	    i++ ;

	    if(i < num_song_markers) {
		last_sample = song_markers[i] ;
	    } else {
		last_sample = prefs.n_samples-1 ;
	    }

	}

	file_processing = FALSE;
    }
}

void adjust_marker_positions(long pos, long delta)
{
    int i,j;

    i = 0;
    while (i < n_markers) {
        if (markers[i] >= pos) {
            markers[i] += delta;
            if (markers[i] <= pos || markers[i] >= prefs.n_samples) {
                for (j = i; j < n_markers - 1; j++) {
                    markers[j] = markers[j+1];
                }
                n_markers--;
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void view_all(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	audio_view.first_sample = 0;
	audio_view.last_sample = prefs.n_samples - 1;
	audio_view.selection_region = FALSE;
	set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample,
		       audio_view.last_sample);
	/* set_scroll_bar redraws */
	/*main_redraw(FALSE, TRUE) ; */
	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void zoom_out(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	long w_old = audio_view.last_sample - audio_view.first_sample;
	long w_new = w_old * 1.5;
	
	file_processing = TRUE;

	audio_view.first_sample -= (w_new - w_old) / 2;
	if (audio_view.first_sample < 0)
	    audio_view.first_sample = 0;
	audio_view.last_sample = audio_view.first_sample + w_new;
	if (audio_view.last_sample > prefs.n_samples - 1)
	    audio_view.last_sample = prefs.n_samples - 1;
	set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample, audio_view.last_sample);
	/* set_scroll_bar redraws */
	/*main_redraw(FALSE, TRUE) ; */
	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void zoom_in(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	long w_old = audio_view.last_sample - audio_view.first_sample;
	long w_new = w_old / 1.5;
	
	file_processing = TRUE;

	audio_view.first_sample += (w_old - w_new) / 2;
	if (audio_view.first_sample < 0)
	    audio_view.first_sample = 0;
	audio_view.last_sample = audio_view.first_sample + w_new;
	if (audio_view.last_sample > prefs.n_samples - 1)
	    audio_view.last_sample = prefs.n_samples - 1;
	set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample, audio_view.last_sample);
	/* set_scroll_bar redraws */
	/*main_redraw(FALSE, TRUE) ; */
	file_processing = FALSE;
    }
}

void toggle_start_marker(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	long first, last;
	get_region_of_interest(&first, &last, &audio_view);
	toggle_marker_at(first);
	main_redraw(FALSE, TRUE);
    }
}

void toggle_end_marker(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	long first, last;
	get_region_of_interest(&first, &last, &audio_view);
	toggle_marker_at(last);
	main_redraw(FALSE, TRUE);
    }
}

void clear_markers_in_view(GtkWidget * widget, gpointer data)
{

    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	long first, last;
	int i;


	get_region_of_interest(&first, &last, &audio_view);

	for (i = 0; i < n_markers; i++) {
	    if (markers[i] >= first && markers[i] <= last) {
		toggle_marker_at(markers[i]);
		i--;		/* current marker deleted, next marker dropped into position i */
	    }
	}
	main_redraw(FALSE, TRUE);
    }
}

gboolean  key_press_cb(GtkWidget * widget, GdkEventKey * event, gpointer data)
{
    extern double spectral_amp;
    gboolean handled = TRUE ;
    long sample_shift;

    /* GDK_b, GDK_c, GDK_e, GDK_n, GDK_z used through menus */
    switch (event->keyval) {
	case GDK_space:
	// play/stop playback
	    if (audio_playback == FALSE) {
		start_gwc_playback(widget, data);
		if ((event->state & GDK_CONTROL_MASK) || (event->state & GDK_SHIFT_MASK))
		  audio_is_looping = TRUE;
	    } else {
	      stop_all_playback_functions(widget, data);
	      // repaint cursor
	      main_redraw(TRUE, TRUE);
	    }
	    break;
	case GDK_s:
	// select last S seconds of audio
	    if (audio_playback == TRUE) {
		//set_playback_cursor_position(&audio_view);
		//gtk_timeout_remove(cursor_timer);
		stop_all_playback_functions(widget, data);
		audio_view.selected_last_sample = audio_view.cursor_position;
		audio_view.selected_first_sample =
		    audio_view.selected_last_sample -
		    prefs.rate * stop_key_highlight_interval;
		if (audio_view.selected_first_sample < 0)
		    audio_view.selected_first_sample = 0;
		audio_view.selection_region = TRUE;
		main_redraw(FALSE, TRUE);
	    }
	    break;
	case GDK_Escape:
	// deselect audio
	    if (audio_playback == FALSE) {
		audio_view.selected_first_sample = 0;
		audio_view.selected_last_sample = prefs.n_samples - 1;
		audio_view.selection_region = FALSE;
		main_redraw(FALSE, TRUE);
	    }
	    break;
	case GDK_KEY_Right:
	// go foward
	    if (event->state & GDK_CONTROL_MASK) {
	      // by 2 revolution of a 33 1/3 rpm record
	      sample_shift = 2 * prefs.rate * 60.0 / 33.333333333 ;
	    } else if (event->state & GDK_SHIFT_MASK) {
	      // by 4 revolutions of a 33 1/3 rpm record
	      sample_shift = 4 * prefs.rate * 60.0 / 33.333333333 ;
	    } else {
	      // by 0.5 revolution of a 33 1/3 rpm record
	      sample_shift = 0.5 * prefs.rate * 60.0 / 33.333333333 ;
	    }
	    //audio_debug_print("shift is %d samples\n", sample_shift) ;
	    if ((audio_view.n_samples - audio_view.last_sample) > sample_shift) {
	      audio_view.first_sample += sample_shift;
	      audio_view.last_sample += sample_shift;
	    } else {
	      audio_view.first_sample += audio_view.n_samples - audio_view.last_sample - 1;
	      audio_view.last_sample = audio_view.n_samples - 1;
	    }
	    set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample, audio_view.last_sample);
	    break;
	case GDK_KEY_Left:
	// go backward
	    if (event->state & GDK_CONTROL_MASK) {
	      // by 2 revolution of a 33 1/3 rpm record
	      sample_shift = 2 * prefs.rate * 60.0 / 33.333333333 ;
	    } else if (event->state & GDK_SHIFT_MASK) {
	      // by 4 revolutions of a 33 1/3 rpm record
	      sample_shift = 4 * prefs.rate * 60.0 / 33.333333333 ;
	    } else {
	      // by 0.5 revolution of a 33 1/3 rpm record
	      sample_shift = 0.5 * prefs.rate * 60.0 / 33.333333333 ;
	    }
	    if (audio_view.first_sample > sample_shift) {
	      audio_view.last_sample -= sample_shift;
	      audio_view.first_sample -= sample_shift;
	    } else {
	      audio_view.last_sample -= audio_view.first_sample;
	      audio_view.first_sample = 0;
	    }
	    set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample, audio_view.last_sample);
	    break;
	case GDK_KEY_Up:
	// zoom in time scale
	    zoom_in(widget, data);
	    break;
	case GDK_KEY_Down:
	// zoom out time scale
	    zoom_out(widget, data);
	    break;
	case GDK_f:
	// zoom in Y scale
	    scale_up_callback(widget, data);
	    break;
	case GDK_d:
	// zoom out Y scale
	    scale_down_callback(widget, data);
	    break;
	case GDK_g:
	// reset Y scale
	    scale_reset_callback(widget, data);
	    break;
	case GDK_b:
	// amplify sonogram
	    spectral_amp *= 2;
	    main_redraw(FALSE, TRUE);
	    break;
	case GDK_v:
	// attenuate sonogram
	    spectral_amp /= 2;
	    main_redraw(FALSE, TRUE);
	    break;
	case GDK_w:
	// select markers
	    select_markers(widget, data);
	    break;
	default:
	    handled = FALSE ;
	    break ;
    }

    return handled ;
}


int cleanup_and_close(struct view *v, struct sound_prefs *p)
{
    if (audio_playback)
      stop_all_playback_functions(NULL, NULL);

    if(batch_mode == 0 && file_is_open && get_undo_levels() > 0) {
	int r = yesnocancel("Save changes to the audio file?") ;

	if(r == 2)
	    return 0 ;

	if(r == 1) {
	    fprintf(stderr, "Undoing all changes\n") ;
	    while(undo(v, p) > 0) ;
	}
    }

    if (file_is_open)
	save_sample_block_data(&prefs);

    if (close_wavefile(&audio_view)) {
	save_preferences();
	undo_purge();
    }

    return 1 ;
}


gint delete_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
	if( file_processing == TRUE )
	{
		warning("Can't quit while file is being processed.");
		return TRUE;
	}
	if( batch_mode == TRUE )
	{
		warning("Can't quit while in batch mode. (Will automatically close.)");
		return TRUE;
	}
    return FALSE;
}

void destroy(GtkWidget * widget, gpointer data)
{
    if(cleanup_and_close(&audio_view, &prefs))
	gtk_main_quit();
}

void about(GtkWidget * widget, gpointer data)
{
    const gchar *authors[] = { "Jeffrey J. Welty", "James Tappin", "Ian Leonard", "Bill Jetzer", "Charles Morgon", "Frank Freudenberg", "Thiemo Gehrke", "Rob Frohne", "clixt.net",  NULL };
    gtk_widget_show(gnome_about_new("Gnome Wave Cleaner",
				    VERSION,
				    "Copyright 2001,2002,2003,2004,2005 Redhawk.org, 2017 clixt.net",
				    "An application to aid in denoising (hiss & clicks) of audio files",
				    authors,
				    NULL,
				    NULL,NULL));
}

void main_redraw(int cursor_flag, int redraw_data)
{
    if (doing_statusbar_update == TRUE)
	return;

    if (file_is_open == TRUE)
	redraw(&audio_view, &prefs, audio_drawing_area, cursor_flag, redraw_data, spectral_view_flag);

    if (file_is_open == TRUE && cursor_flag == FALSE)
	display_times();
}

void display_sonogram(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {
	file_processing = TRUE;
	spectral_view_flag = !spectral_view_flag;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void open_wave_filename(void)
{
    char tmp[PATH_MAX+1];

    //int l;
    struct sound_prefs tmp_prefs;

    gnome_flush();

       /* Close writes to wave_filename so put back the name of the
	  current open file */
    strcpy(tmp,wave_filename);
    strcpy(wave_filename, last_filename);

    if(!cleanup_and_close(&audio_view, &prefs)) 
	return ;

    strcpy(wave_filename,tmp);

       /* all this to store the last directory where a file was opened */
    strcpy(tmp, wave_filename);
    strcpy(pathname, dirname(tmp));
    strcat(pathname, "/");

    strcpy(tmp, wave_filename);
    strcat(pathname, basename(tmp));
    
    if (is_valid_audio_file(wave_filename)) {
	tmp_prefs = open_wavefile((char *) wave_filename, &audio_view);
	if (tmp_prefs.successful_open) {
	    prefs = tmp_prefs;
	    spectral_view_flag = FALSE;
	    if (prefs.wavefile_fd != -1) {
		audio_view.n_samples = prefs.n_samples;
		if (audio_view.first_sample == -1) {
		    audio_view.first_sample = 0;
		    audio_view.last_sample = (prefs.n_samples - 1);
		} else {
		    audio_view.first_sample =
			MIN(prefs.n_samples - 1,
			    audio_view.first_sample);
		    audio_view.first_sample =
			MAX(0, audio_view.first_sample);
		    audio_view.last_sample =
			MIN(prefs.n_samples - 1,
			    audio_view.last_sample);
		    audio_view.last_sample =
			MAX(0, audio_view.last_sample);
		}
		audio_view.selection_region = FALSE;
		file_is_open = TRUE;
		fill_sample_buffer(&prefs);

		/* display entire file data if this file changed since last edit session */
		if (strcmp(wave_filename, last_filename)) {
		    audio_view.first_sample = 0;
		    audio_view.last_sample = prefs.n_samples - 1;
		    strcpy(last_filename, wave_filename);
		    num_song_markers = 0;
		}

		set_scroll_bar(prefs.n_samples - 1,
			       audio_view.first_sample,
			       audio_view.last_sample);
		main_redraw(FALSE, TRUE);
	    } else {
		file_is_open = FALSE;
		warning("failed to open audio file");
	    }
	} else {
	    strcpy(wave_filename, last_filename);
	}
    } else {
	file_is_open = FALSE;
	warning("No file selected, or file format not recognized");
    }
}

void store_filename(GtkFileSelection * selector, gpointer user_data)
{
    strncpy(wave_filename,
	   gtk_file_selection_get_filename(GTK_FILE_SELECTION
					   (file_selector)), PATH_MAX);

    open_wave_filename();

    }

    void store_selection_filename(GtkFileSelection * selector,
			      gpointer user_data)
    {

    strncpy(save_selection_filename,
	   gtk_file_selection_get_filename(GTK_FILE_SELECTION
					   (file_selector)), PATH_MAX);

    if (strcmp(save_selection_filename, wave_filename)) {
	save_selection_as_wavfile(save_selection_filename, &audio_view);
    } else {
	warning("Cannot save selection over the currently open file!");
    }

}

void open_file_selection(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (audio_playback == FALSE)) {

	/* Create the selector */
	file_selector =
	    gtk_file_selection_new("Please select a file for editing.");

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(file_selector),
					pathname);

	gtk_signal_connect(GTK_OBJECT
			   (GTK_FILE_SELECTION(file_selector)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(store_filename),
			   NULL);

	/* Ensure that the dialog box is destroyed when the user clicks a button. */
	gtk_signal_connect_object(GTK_OBJECT
				  (GTK_FILE_SELECTION(file_selector)->
				   ok_button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  (gpointer) file_selector);

	gtk_signal_connect_object(GTK_OBJECT
				  (GTK_FILE_SELECTION(file_selector)->
				   cancel_button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  (gpointer) file_selector);


	/* Display the dialog */
	gtk_widget_show(file_selector);
    }
}

void save_as_selection(GtkWidget * widget, gpointer data)
{

    if ((file_processing == FALSE) && (file_is_open == TRUE) && (audio_playback == FALSE)) {

	if (audio_view.selection_region == TRUE) {

	    /* Create the selector */
	    file_selector =
		gtk_file_selection_new("Filename to save selection to:");

	    gtk_file_selection_set_filename(GTK_FILE_SELECTION
					    (file_selector), pathname);

	    gtk_signal_connect(GTK_OBJECT
			       (GTK_FILE_SELECTION(file_selector)->
				ok_button), "clicked",
			       GTK_SIGNAL_FUNC(store_selection_filename),
			       NULL);

	    /* Ensure that the dialog box is destroyed when the user clicks a button. */
	    gtk_signal_connect_object(GTK_OBJECT
				      (GTK_FILE_SELECTION(file_selector)->
				       ok_button), "clicked",
				      GTK_SIGNAL_FUNC(gtk_widget_destroy),
				      (gpointer) file_selector);

	    gtk_signal_connect_object(GTK_OBJECT
				      (GTK_FILE_SELECTION(file_selector)->
				       cancel_button), "clicked",
				      GTK_SIGNAL_FUNC(gtk_widget_destroy),
				      (gpointer) file_selector);

	    /* Display the dialog */
	    gtk_widget_show(file_selector);
	} else {
	    info("Please highlight a region to save first");
	}
    }
}

#define GNOMEUIINFO_ITEM_ACCEL(label, tooltip, callback, xpm_data, accel) \
{ GNOME_APP_UI_ITEM, label, tooltip,\
  (gpointer)callback, NULL, NULL, \
  GNOME_APP_PIXMAP_DATA, xpm_data, accel, (GdkModifierType) 0, NULL}

#define GNOMEUIINFO_ITEM_ACCELMOD(label, tooltip, callback, xpm_data, accel, modifier) \
{ GNOME_APP_UI_ITEM, label, tooltip, (gpointer)callback, NULL, NULL, \
  GNOME_APP_PIXMAP_DATA, xpm_data, accel, (GdkModifierType) modifier, NULL}

GnomeUIInfo file_menu[] = {
    GNOMEUIINFO_MENU_OPEN_ITEM(open_file_selection, NULL),
    GNOMEUIINFO_ITEM_NONE("Save Selection As...",
		      "Saves the current selection to a new wavfile",
		      save_as_selection),
    GNOMEUIINFO_ITEM_NONE("Split audio on song markers",
		      "Create individual track files",
		      split_audio_on_markers),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_EXIT_ITEM(destroy, NULL),
    GNOMEUIINFO_END
};

/* the spaces before the first item's text prevent
* the item text from overwriting the icon
* in the drop-down menus */

GnomeUIInfo edit_menu[] = {
    GNOMEUIINFO_ITEM_ACCEL(" Undo",
		 "Undo last edit action",
		 undo_callback, undo_xpm, GDK_KEY_BackSpace),
    GNOMEUIINFO_SEPARATOR,
    
    GNOMEUIINFO_ITEM(" Apply DSP Frequency Filters", "lowpass,highpass,notch or bandpass biquad filtering",
		 filter_cb, filter_xpm),

    GNOMEUIINFO_ITEM(" Generate Pink Noise", "Replace current view or selection with pink noise",
		 pinknoise_cb, pinknoise_xpm),

    GNOMEUIINFO_ITEM(" Amplify", "Amplify the current view or selection",
		 amplify, amplify_xpm),
    GNOMEUIINFO_SEPARATOR,
    
    GNOMEUIINFO_ITEM_ACCEL(" Declick Strong",
		 "Remove pops/clicks from current view or selection",
		 declick, declick_xpm, GDK_u),

    GNOMEUIINFO_ITEM_ACCEL(" Declick Weak",
		 "Remove weaker pops/clicks from current view or selection",
		 declick_weak, declick_w_xpm, GDK_i),

    GNOMEUIINFO_ITEM_ACCEL(" Declick Manual",
		 "Apply LSAR signal estimation  to current view or selection (< 500 samples)",
		 manual_declick, declick_m_xpm, GDK_o),

    GNOMEUIINFO_ITEM_ACCEL(" Decrackle",
		       "Remove crackle from old, deteriorated vinyl",
		       decrackle, decrackle_xpm, GDK_c),

    GNOMEUIINFO_ITEM_ACCEL(" Estimate",
		 "Estimate signal (> 500 samples) in current view or selection",
		 estimate, estimate_xpm, GDK_k),

    GNOMEUIINFO_ITEM_ACCEL(" Sample",
		 "Use current view or selection as a noise sample",
		 noise_sample, noise_sample_xpm, GDK_h),

    GNOMEUIINFO_ITEM_ACCEL(" Denoise",
		 "Remove noise from  current view or selection",
		 remove_noise, remove_noise_xpm, GDK_j),
    GNOMEUIINFO_ITEM(" Silence", "Insert silence with size of current selection to audio data",
		 silence_callback, silence_xpm),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK(" Cut", "Cut current selection to internal clipboard",
		       cut_callback, GTK_STOCK_CUT),
    GNOMEUIINFO_ITEM_STOCK(" Copy", "Copy current selection to internal clipboard",
		       copy_callback, GTK_STOCK_COPY),
    GNOMEUIINFO_ITEM_STOCK(" Paste", "Insert internal clipboard at begin of current selection",
		       paste_callback, GTK_STOCK_PASTE),
    GNOMEUIINFO_ITEM_STOCK(" Delete", "Delete current selection from audio data",
		       delete_callback, GTK_STOCK_DELETE),

    GNOMEUIINFO_ITEM(" Reverb", "Apply reverberation the current view or selection",
		 reverb, amplify_xpm),
    GNOMEUIINFO_END
};

GnomeUIInfo marker_menu[] = {
    GNOMEUIINFO_ITEM_ACCEL("Toggle Beginning Marker",
		       "Toggle marker at beginning of current selection or view",
		       toggle_start_marker, NULL, GDK_n),

    GNOMEUIINFO_ITEM_ACCEL("Toggle Ending Marker",
		       "Toggle marker at end of current selection or view",
		       toggle_end_marker, NULL, GDK_m),

    GNOMEUIINFO_ITEM_ACCEL("Clear Markers",
		 "Clear all markers in the current selection or view",
		 clear_markers_in_view, NULL, GDK_r),
    GNOMEUIINFO_ITEM_ACCEL("Expand selection to nearest markers",
		 "Select region between two markers",
		 select_markers, NULL, GDK_e),
    GNOMEUIINFO_ITEM("Mark Songs",
		 "Find songs in current selection or view",
		 mark_songs, NULL),
    GNOMEUIINFO_ITEM("Move Song Marker",
		 "Move closest song marker to start of selection",
		 move_song_marker, NULL),
    GNOMEUIINFO_ITEM("Add Song Marker",
		 "Add song marker at start of selection",
		 add_song_marker, NULL),
    GNOMEUIINFO_ITEM("Add Song Marker Pair",
		 "Add song marker at start AND end of selection",
		 add_song_marker_pair, NULL),
    GNOMEUIINFO_ITEM("Delete Song Marker",
		 "Delete closest song marker to start of selection",
		 delete_song_marker, NULL),
    GNOMEUIINFO_ITEM("Next Song Marker",
		       "Select around song marker after start of selection",
		       select_song_marker, NULL),
    GNOMEUIINFO_END
};

GnomeUIInfo view_menu[] = {

    GNOMEUIINFO_ITEM_ACCEL(" ZoomSelect", "Zoom in on selected portion",
		       zoom_select, zoom_sel_xpm, GDK_KEY_slash),

    GNOMEUIINFO_ITEM_ACCEL(" ZoomIn", "Zoom in", zoom_in,
		 zoom_in_xpm, GDK_KEY_KP_Add),

    GNOMEUIINFO_ITEM_ACCEL(" ZoomOut", "Zoom out", zoom_out,
		 zoom_out_xpm, GDK_KEY_KP_Subtract),

    GNOMEUIINFO_ITEM_ACCEL(" ViewAll", "View Entire audio file",
		 view_all, view_all_xpm, GDK_KEY_backslash),

    GNOMEUIINFO_ITEM_ACCEL(" SelectAll", "Select current view",
		 select_all, select_all_xpm, GDK_KEY_KP_Multiply),

    GNOMEUIINFO_ITEM(" SpectralView", "Toggle Sonagram",
		 display_sonogram, spectral_xpm),
    GNOMEUIINFO_END
};

GnomeUIInfo settings_menu[] = {
    GNOMEUIINFO_ITEM_ACCEL("Declick", "Set declick sensitivity, iteration",
		 declick_set_preferences, NULL, GDK_p),
    GNOMEUIINFO_ITEM_ACCEL("Decrackle", "Set decrackle sensitivity",
		 decrackle_set_preferences, NULL, GDK_semicolon),
    GNOMEUIINFO_ITEM_ACCEL("Denoise", "Set denoise parameters",
		 denoise_set_preferences, NULL, GDK_l),
    GNOMEUIINFO_ITEM_ACCELMOD("Preferences", "Program options",
		 set_options, NULL, GDK_p, 4),
    GNOMEUIINFO_END
};

GnomeUIInfo help_menu[] = {
    GNOMEUIINFO_ITEM("How To Use", "Basic instructions for using gwc",
		 help, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM("Quickstart", "For the impatient, how to start using gwc quickly",
		 quickstart_help, NULL),
	   GNOMEUIINFO_SEPARATOR,
	  {GNOME_APP_UI_ITEM, 
	   N_("About"), N_("Info about this program"),
	   about, NULL, NULL, 
	   GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_ABOUT,
	   0, 0, NULL},
    GNOMEUIINFO_END
};

GnomeUIInfo menubar[] = {
    GNOMEUIINFO_MENU_FILE_TREE(file_menu),
    GNOMEUIINFO_MENU_EDIT_TREE(edit_menu),
    GNOMEUIINFO_MENU_VIEW_TREE(view_menu),
    GNOMEUIINFO_SUBTREE("_Markers", marker_menu),
    GNOMEUIINFO_MENU_SETTINGS_TREE(settings_menu),
    GNOMEUIINFO_MENU_HELP_TREE(help_menu),
    GNOMEUIINFO_END
};

GnomeUIInfo transport_toolbar_info[] = {
    GNOMEUIINFO_ITEM("Start",
		 "Playback the selected or current view of audio",
		 start_gwc_playback, start_xpm),
    GNOMEUIINFO_ITEM("Stop", "Stop audio playback",
		 stop_all_playback_functions, stop_xpm),

    GNOMEUIINFO_ITEM("ZoomSelect", "Zoom in on selected portion",
		 zoom_select, zoom_sel_xpm),

    GNOMEUIINFO_ITEM("ZoomIn", "Zoom in", zoom_in,
		 zoom_in_xpm),

    GNOMEUIINFO_ITEM("ZoomOut", "Zoom out", zoom_out,
		 zoom_out_xpm),

    GNOMEUIINFO_ITEM("ViewAll", "View Entire audio file",
		 view_all,
		 view_all_xpm),

    GNOMEUIINFO_ITEM("SelectAll", "Select current view",
		 select_all,
		 select_all_xpm),

    GNOMEUIINFO_ITEM("SpectralView", "Toggle Sonagram",
		 display_sonogram,
		 spectral_xpm),

    GNOMEUIINFO_END
};

GnomeUIInfo edit_toolbar_info[] = {
    GNOMEUIINFO_ITEM("Undo", "Undo the last edit action",
		 undo_callback, undo_xpm),

    GNOMEUIINFO_ITEM("Amplify", "Amplify the current view or selection",
		 amplify, amplify_xpm),

    GNOMEUIINFO_ITEM("Declick Strong",
		 "Remove pops/clicks from current view or selection",
		 declick, declick_xpm),

    GNOMEUIINFO_ITEM("Declick Weak",
		 "Remove weaker pops/clicks from current view or selection",
		 declick_weak, declick_w_xpm),

    GNOMEUIINFO_ITEM("Declick Manual",
		 "Apply LSAR signal estimation to current view or selection",
		 manual_declick, declick_m_xpm),

    GNOMEUIINFO_ITEM("Decrackle",
		 "Remove crackle from old, deteriorated vinyl",
		 decrackle, decrackle_xpm),

    GNOMEUIINFO_ITEM("Estimate",
		 "Estimate signal (> 300 samples) in current view or selection",
		 estimate, estimate_xpm),

    GNOMEUIINFO_ITEM("Sample",
		 "Use current view or selection as a noise sample",
		 noise_sample, noise_sample_xpm),

    GNOMEUIINFO_ITEM("Denoise",
		 "Remove noise from  current view or selection",
		 remove_noise, remove_noise_xpm),

    GNOMEUIINFO_ITEM("Silence", "Insert silence with size of current selection to audio data",
		 silence_callback, silence_xpm),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK("Cut", "Cut current selection to internal clipboard",
		       cut_callback, GTK_STOCK_CUT),
    GNOMEUIINFO_ITEM_STOCK("Copy", "Copy current selection to internal clipboard",
		       copy_callback, GTK_STOCK_COPY),
    GNOMEUIINFO_ITEM_STOCK("Paste", "Insert internal clipboard at begin of current selection",
		       paste_callback, GTK_STOCK_PASTE),
    GNOMEUIINFO_ITEM_STOCK("Delete", "Delete current selection from audio data",
		       delete_callback, GTK_STOCK_DELETE),
    GNOMEUIINFO_END
};

void update_status_bar(gfloat percentage, gfloat min_delta,
		   gboolean init_flag)
{
#ifdef BY_DATA_LENGTH
    static gfloat last_percentage_displayed = -1.0;

    if (percentage - last_percentage_displayed > min_delta
	|| init_flag == TRUE) {
	doing_statusbar_update = TRUE;
	gnome_appbar_set_progress_percentage(GNOME_APPBAR(status_bar), percentage);
	gnome_flush();
	doing_statusbar_update = FALSE;
	last_percentage_displayed = percentage;
    }
#else
    static struct timeval last_displayed = { 0, 0 };
    static struct timezone tz = { 0, 0 };
    static struct timeval this;
    long delta_ms;

    gettimeofday(&this, &tz);

    delta_ms =
	(this.tv_sec - last_displayed.tv_sec) * 1000 + (this.tv_usec -
							last_displayed.
							tv_usec) / 1000;

    if (delta_ms > 1000 * min_delta || init_flag == TRUE) {
	doing_statusbar_update = TRUE;
	gnome_appbar_set_progress_percentage(GNOME_APPBAR(status_bar), percentage);
	gnome_flush();
	doing_statusbar_update = FALSE;
	last_displayed = this;
    }
    #endif
}

void set_status_text(gchar * msg)
{
    gnome_appbar_set_status(GNOME_APPBAR(status_bar), msg);
}

void push_status_text(gchar * msg)
{
    gnome_appbar_push(GNOME_APPBAR(status_bar), msg);
}

void pop_status_text(void)
{
    gnome_appbar_pop(GNOME_APPBAR(status_bar));
}

/* Create a new backing pixmap of the appropriate size */
static gint audio_area_configure_event(GtkWidget * widget, GdkEventConfigure * event)
{
    if (audio_pixmap)
	gdk_pixmap_unref(audio_pixmap);

    audio_pixmap = gdk_pixmap_new(widget->window,
				  widget->allocation.width,
				  widget->allocation.height, -1);
    if (highlight_pixmap)
	gdk_pixmap_unref(highlight_pixmap);

    highlight_pixmap = gdk_pixmap_new(widget->window,
				      widget->allocation.width,
				      widget->allocation.height, -1);
    if (cursor_pixmap)
	gdk_pixmap_unref(cursor_pixmap);

    cursor_pixmap = gdk_pixmap_new(widget->window,
				   1, widget->allocation.height, -1);

    gdk_draw_rectangle(audio_pixmap,
		       widget->style->white_gc,
		       TRUE,
		       0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
    gdk_draw_rectangle(highlight_pixmap,
		       widget->style->white_gc,
		       TRUE,
		       0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
    audio_view.canvas_width = widget->allocation.width;
    audio_view.canvas_height = widget->allocation.height;
    main_redraw(FALSE, TRUE);

    // remember current window size
    gtk_window_get_size (GTK_WINDOW(main_window), &app_prefs.window_width, &app_prefs.window_height);

    return TRUE;
}

/* Redraw the screen from the backing pixmap */
gint audio_expose_event(GtkWidget * widget, GdkEventExpose * event)
{
    gdk_draw_pixmap(widget->window,
		widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
		highlight_pixmap,
		event->area.x, event->area.y,
		event->area.x, event->area.y,
		event->area.width, event->area.height);

/*      g_print("expose x:%d y:%d width:%d height:%d\n",  */
/*  	       event->area.x, event->area.y,  */
/*  	       event->area.width, event->area.height);  */

    return FALSE;
}

void gwc_window_set_title(char *title)
{
    char buf[255];
    snprintf(buf, sizeof(buf), "GWC: %s", title);
    gtk_window_set_title(GTK_WINDOW(main_window), buf);
}

GtkWidget *mk_label_and_pack(GtkBox * box, char *text)
{
    GtkWidget *w = gtk_label_new(text);
    gtk_box_pack_start(box, w, TRUE, TRUE, 0);
    gtk_widget_show(w);
    return w;
}

long time_to_sample(char *time, struct sound_prefs *p)
{
    int nf;
    int h, m;
    double s;
    long position = 0;

    nf = sscanf(time, "%d:%d:%lf", &h, &m, &s);

    if (nf == 3) {
	position = (h * 3600 + m * 60) * p->rate + s * p->rate;
    } else if (nf == 2) {
	nf = sscanf(time, "%d:%lf", &m, &s);
	position = (m * 60) * p->rate + s * p->rate;
    } else if (nf == 1) {
	nf = sscanf(time, "%lf", &s);
	position = s * p->rate;
    }

    return position;
}

int main(int argc, char *argv[])
{
    
    GtkWidget *main_vbox, *led_vbox, *track_times_vbox, *times_vbox, *bottom_hbox;
    GtkWidget *detect_only_box;
    GtkWidget *leave_click_marks_box;

    /* *************************************************************** */
    /* Lindsay Harris addition for SMP operations  */
    /*
     *   On SMP systems,  the stack size does not grow, for reasons beyond
     *  my understanding.   So,  set an increase in size now.  For my
     *  SuSE 8.2 system, the initial size is 2MB,  whereas the "denoise"
     *  operation requires about 3.5MB,  so I picked an arbitrary 4MB
     *  stack size.  This might not be sufficient for some other operations.
     */
    {
	struct  rlimit   rl;

#define  GWC_STACK_LIM (6 * 1024 *1024)

	if( getrlimit( RLIMIT_STACK, &rl ) == -1 )
	{
	    perror( "getrlimit for RLIMIT_STACK:" );
	    exit( 1 );
	}

	printf("Current stack limit: %d bytes\n", (int)rl.rlim_cur);

	/*   Only change the size if it's too small.   */
	if( rl.rlim_cur < GWC_STACK_LIM )
	{

	    rl.rlim_cur = GWC_STACK_LIM ;
	    printf("1 ");
	    if( setrlimit( RLIMIT_STACK, &rl ) == -1 )
	    {
		perror( "setrlimit for RLIMIT_STACK:" );
		exit( 1 );
	    }
	    printf("New stack limit: %d bytes\n", (int)rl.rlim_cur) ;
	}
    }

    /* This is called in all GTK applications. Arguments are parsed
     * from the command line and are returned to the application. */
    //gtk_init(&argc, &argv);

    #define PREFIX "."
    #define SYSCONFDIR "."

    GnomeProgram *gwc_app = gnome_program_init(APPNAME, VERSION, LIBGNOMEUI_MODULE, argc, argv,
		       GNOME_PARAM_POPT_TABLE, NULL,
		       GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);
    if (gwc_app == NULL) {
      printf("ERROR: gnome_program_init failed");
      exit (1);
    }

    main_window = gnome_app_new("gwc", "Gnome Wave Cleaner - Declick & Denoise Audio");
    //gtk_window_set_icon(GTK_WINDOW(main_window), gdk_pixbuf_new_from_file("gwc-logo.png", NULL));
    gtk_window_set_icon_name (GTK_WINDOW(main_window), "media-tape");
    gnome_app_create_menus(GNOME_APP(main_window), menubar);

    load_preferences();
    
    // resize main window
    gtk_window_set_default_size(GTK_WINDOW(main_window), app_prefs.window_width, app_prefs.window_height);
    
    /* When the window is given the "delete_event" signal (this is given
     * by the window manager, usually by the "close" option, or on the
     * titlebar), we ask it to call the delete_event () function
     * as defined above. The data passed to the callback
     * function is NULL and is ignored in the callback function. */
    gtk_signal_connect(GTK_OBJECT(main_window), "delete_event",
		       GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

    /* Here we connect the "destroy" event to a signal handler.  
     * This event occurs when we call gtk_widget_destroy() on the window,
     * or if we return FALSE in the "delete_event" callback. */
    gtk_signal_connect(GTK_OBJECT(main_window), "destroy",
		       GTK_SIGNAL_FUNC(destroy), NULL);

    g_signal_connect(GTK_OBJECT(main_window), "key_press_event",
		       GTK_SIGNAL_FUNC(key_press_cb), NULL);

    /* Sets the border width of the window. */
    gtk_container_set_border_width(GTK_CONTAINER(main_window), 1);

    main_vbox = gtk_vbox_new(FALSE, 1);
    track_times_vbox = gtk_vbox_new(FALSE, 1);
    times_vbox = gtk_vbox_new(FALSE, 1);
    led_vbox = gtk_vbox_new(FALSE, 1);
    bottom_hbox = gtk_hbox_new(FALSE, 1);

    /* This packs the button into the window (a gtk container). */
    gnome_app_set_contents(GNOME_APP(main_window), main_vbox);

    /* setup appbar (bottom of window bar for status, menu hints and
     * progress display) */
    status_bar = gnome_appbar_new(TRUE, TRUE, GNOME_PREFERENCES_USER);
    gnome_app_set_statusbar(GNOME_APP(main_window), status_bar);

    /* make menu hints display on the appbar */
    gnome_app_install_menu_hints(GNOME_APP(main_window), menubar);

    /* create a new canvas */
    audio_drawing_area = gtk_drawing_area_new();

    gtk_signal_connect(GTK_OBJECT(audio_drawing_area), "expose_event",
		       (GtkSignalFunc) audio_expose_event, NULL);
    gtk_signal_connect(GTK_OBJECT(audio_drawing_area),
		       "configure_event",
		       (GtkSignalFunc) audio_area_configure_event,
		       NULL);

    gtk_signal_connect(GTK_OBJECT(audio_drawing_area),
		       "button_press_event",
		       (GtkSignalFunc) audio_area_button_event, NULL);
    gtk_signal_connect(GTK_OBJECT(audio_drawing_area),
		       "motion_notify_event",
		       (GtkSignalFunc) audio_area_motion_event, NULL);
/*          gtk_signal_connect (GTK_OBJECT(audio_drawing_area),"event",  */
/*                             (GtkSignalFunc) audio_area_event, NULL);  */

    gtk_widget_set_events(audio_drawing_area, GDK_EXPOSURE_MASK
			  | GDK_LEAVE_NOTIFY_MASK
			  | GDK_BUTTON_PRESS_MASK
			  | GDK_BUTTON_RELEASE_MASK
			  | GDK_POINTER_MOTION_MASK
			  | GDK_POINTER_MOTION_HINT_MASK);

    gtk_box_pack_start(GTK_BOX(main_vbox), audio_drawing_area, TRUE, TRUE, 0);


    scroll_pos = gtk_adjustment_new(1.0, 0.0, 100.0, 5.0, 5.0, 0.0);
    hscrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(scroll_pos));
    gtk_widget_show(hscrollbar);

    gtk_signal_connect(GTK_OBJECT(GTK_ADJUSTMENT(scroll_pos)), "changed",
		       (GtkSignalFunc) scroll_bar_changed, NULL);

    gtk_signal_connect(GTK_OBJECT(GTK_ADJUSTMENT(scroll_pos)),
		       "value_changed", (GtkSignalFunc) scroll_bar_changed,
		       NULL);

    gtk_box_pack_start(GTK_BOX(main_vbox), hscrollbar, FALSE, TRUE, 0);

    {
	edit_toolbar = gtk_toolbar_new() ;

	gnome_app_fill_toolbar(GTK_TOOLBAR(edit_toolbar),
			       edit_toolbar_info, NULL);

	gnome_app_add_toolbar(GNOME_APP(main_window),
			      GTK_TOOLBAR(edit_toolbar), "Edit tools",
			      BONOBO_DOCK_ITEM_BEH_NORMAL, BONOBO_DOCK_TOP,
			      2, 0, 0);

	transport_toolbar = gtk_toolbar_new() ;

	gnome_app_fill_toolbar(GTK_TOOLBAR(transport_toolbar),
			       transport_toolbar_info, NULL);

	gnome_app_add_toolbar(GNOME_APP(main_window),
			      GTK_TOOLBAR(transport_toolbar),
			      "Playback/selection tools",
			      BONOBO_DOCK_ITEM_BEH_NORMAL, BONOBO_DOCK_TOP,
			      2, 0, 0);

	gtk_toolbar_set_style(GTK_TOOLBAR(transport_toolbar), GTK_TOOLBAR_ICONS) ;
	gtk_toolbar_set_style(GTK_TOOLBAR(edit_toolbar), GTK_TOOLBAR_ICONS) ;
    }

    l_file_time = mk_label_and_pack(GTK_BOX(track_times_vbox), "Track 0:00:000");
    l_samples_perpixel = mk_label_and_pack(GTK_BOX(track_times_vbox), "Samples per pixel: 0");    
    l_file_samples = mk_label_and_pack(GTK_BOX(track_times_vbox), "Track samples: 0");
    l_first_time = mk_label_and_pack(GTK_BOX(times_vbox), "First 0:00:000");
    l_last_time = mk_label_and_pack(GTK_BOX(times_vbox), "Last 0:00:000");
    l_selected_time = mk_label_and_pack(GTK_BOX(times_vbox), "Selected 0:00:000");
    l_samples = mk_label_and_pack(GTK_BOX(times_vbox), "Samples: 0");
    
    gtk_box_pack_start(GTK_BOX(bottom_hbox), led_vbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bottom_hbox), track_times_vbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bottom_hbox), times_vbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), bottom_hbox, FALSE, TRUE, 0);

    detect_only_box = gtk_hbox_new(FALSE, 10);
    detect_only_widget = gtk_check_button_new_with_label("Detect, do not repair clicks");
    GTK_WIDGET_UNSET_FLAGS(detect_only_widget, GTK_CAN_FOCUS);
    gtk_box_pack_start(GTK_BOX(detect_only_box), detect_only_widget, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(led_vbox), detect_only_box, FALSE, FALSE, 0);

    leave_click_marks_box = gtk_hbox_new(FALSE, 11);
    leave_click_marks_widget = gtk_check_button_new_with_label("Leave click marks after repairing");
    GTK_WIDGET_UNSET_FLAGS(leave_click_marks_widget, GTK_CAN_FOCUS);
    gtk_box_pack_start(GTK_BOX(leave_click_marks_box), leave_click_marks_widget, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(led_vbox), leave_click_marks_box, FALSE, FALSE, 0);
    
    /* and the window */
    gtk_widget_show_all(main_window);

    /*   start_monitor("/dev/dsp") ;  */
    /*   config_audio_input(44100, 16, 1) ;  */
    /*   config_audio_input(prefs.rate, prefs.bits, prefs.stereo) ;  */

    push_status_text("Ready");

    {
	char buf[100];
	int v1, v2, v3, i;
	sf_command(NULL, SFC_GET_LIB_VERSION, buf, sizeof(buf));
	for (i = 0; i < strlen(buf); i++) {
	    if (buf[i] == '-') {
		i++;
		break;
	    }
	}

	sscanf(&buf[i], "%d.%d.%d", &v1, &v2, &v3);

	printf("libsndfile Version: %s %d %d %d\n", buf, v1, v2, v3) ;

	if (v1 < 1 || v2 < 0 || v3 < 0) {
	    warning("libsndfile 1.0.0 or greater is required");
	    exit(1);
	}
    }

    if (argc > 1) {
	strcpy(wave_filename, argv[1]);
	open_wave_filename();
    }
    
    /* All GTK applications must have a gtk_main(). Control ends here
     * and waits for an event to occur (like a key press or
     * mouse event). */

    gtk_main();

    return (0);
}
