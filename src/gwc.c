/*****************************************************************************
*   GTK Wave Cleaner Version 0.21
*   Copyright (C) 2001,2002,2003,2004,2005,2006 Jeffrey J. Welty
*
*   Gtk Wave Cleaner Version 0.40, 2017 mod by audioquality.org
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
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include "soundfile.h"
#include "audio_edit.h"
#include <sndfile.h>

#include "icons/amplify.xpm"
#include "icons/pinknoise.xpm"
#include "icons/gtk-wave-cleaner.xpm"
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
GtkStatusbar *status_bar;
GtkWidget *progress_bar;
GtkWidget *edit_toolbar;
GtkWidget *transport_toolbar;
/* The file selection widget */
GtkWidget *file_selector;

struct timespec ts_playback_start;
struct sound_prefs prefs;
struct denoise_prefs denoise_prefs;
struct view audio_view;
struct click_data click_data;
int audio_playback = FALSE;
int audio_is_looping = FALSE;
gint playback_timer = 0;
gint cursor_timer = 0;
gint spectral_view_flag = FALSE;
gint repair_clicks = 1;
double view_scale = 1.0;
double declick_sensitivity = 0.75;
double weak_declick_sensitivity = 0.60;
double strong_declick_sensitivity = 0.35;
double weak_fft_declick_sensitivity = 3.0 ;
double strong_fft_declick_sensitivity = 5.0 ;
int declick_detector_type = HPF_DETECT ;
double stop_key_highlight_interval = 1;
double song_key_highlight_interval = 1;
double song_mark_silence = 1;
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

gint window_x;
gint window_y;
gint window_width = 1200;
gint window_height = 800;
gboolean window_maximised;
gint doing_progressbar_update = FALSE;
guint status_message, push_message;

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
        fprintf(stderr, "Usage:\n\%s\n\%s [file]\n",prog, prog);
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

GKeyFile* read_config(void)
{
    gchar     *config_file;
    GKeyFile  *key_file = NULL;
    GError    *error = NULL;

    config_file = g_build_filename (g_get_user_config_dir (), APPNAME, SETTINGS_FILE, NULL);
    //fprintf(stderr, "%s \n", config_file);
    key_file = g_key_file_new ();
    if (! g_key_file_load_from_file (key_file, config_file, G_KEY_FILE_KEEP_COMMENTS, &error)) {
        if (error->code != G_IO_ERROR_NOT_FOUND) {
            // this is expected when running for the first time
            // therefore don't use g_warning, which will error if G_DEBUG environment variable is set to "fatal-warnings"
            g_message ("Could not load options file: %s\n", error->message);
        }
        g_clear_error (&error);
    }
    g_free (config_file);
    return(key_file);
}

void write_config(GKeyFile *key_file)
{
    gchar     *file_data;
    gchar     *config_file;

    file_data = g_key_file_to_data (key_file, NULL, NULL);
    if (g_mkdir_with_parents (g_build_filename (g_get_user_config_dir (), APPNAME, NULL), 0755) != -1) {
        config_file = g_build_filename (g_get_user_config_dir (), APPNAME, SETTINGS_FILE, NULL);
        g_file_set_contents (config_file, file_data, -1, NULL);
        g_free (config_file);
    }
    else
        g_printerr ("Could not write settings file: %s\n", strerror (errno)); 
    g_free (file_data);
    g_key_file_free (key_file);
}

void load_preferences(void)
{
    GKeyFile  *key_file = read_config();
    // We should probably have a separate test for each preference...
    if (g_key_file_has_group(key_file, "config") == TRUE) {
        strcpy(pathname, g_key_file_get_string(key_file, "config", "pathname", NULL));
        strcpy(last_filename, g_key_file_get_string(key_file, "config", "last_filename", NULL));
        prefs.rate = g_key_file_get_integer(key_file, "config", "rate", NULL);
        prefs.bits = g_key_file_get_integer(key_file, "config", "bits", NULL);
        prefs.stereo = g_key_file_get_integer(key_file, "config", "stereo", NULL);
        audio_view.first_sample = g_key_file_get_integer(key_file, "config", "first_sample_viewed", NULL);
        audio_view.last_sample = g_key_file_get_integer(key_file, "config", "last_sample_viewed", NULL);
        audio_view.channel_selection_mask = g_key_file_get_integer(key_file, "config", "channel_selection_mask", NULL);
        num_song_markers = 0;
        weak_declick_sensitivity = g_key_file_get_double(key_file, "config", "weak_declick_sensitivity", NULL);
        strong_declick_sensitivity = g_key_file_get_double(key_file, "config", "strong_declick_sensitivity", NULL);
        declick_iterate_flag = g_key_file_get_integer(key_file, "config", "declick_iterate", NULL);
        weak_fft_declick_sensitivity = g_key_file_get_double(key_file, "config", "weak_fft_declick_sensitivity", NULL);
        strong_fft_declick_sensitivity = g_key_file_get_double(key_file, "config", "strong_fft_declick_sensitivity", NULL);
        declick_detector_type = g_key_file_get_integer(key_file, "config", "declick_detector_type", NULL);
        decrackle_level = g_key_file_get_double(key_file, "config", "decrackle_level", NULL);
        decrackle_window = g_key_file_get_integer(key_file, "config", "decrackle_window", NULL);
        decrackle_average = g_key_file_get_integer(key_file, "config", "decrackle_average", NULL);
        stop_key_highlight_interval = g_key_file_get_double(key_file, "config", "stop_key_highlight_interval", NULL);
        song_key_highlight_interval = g_key_file_get_double(key_file, "config", "song_key_highlight_interval", NULL);
        song_mark_silence = g_key_file_get_double(key_file, "config", "song_mark_silence", NULL);
        sonogram_log = g_key_file_get_double(key_file, "config", "sonogram_log", NULL);
        strcpy(audio_device, g_key_file_get_string(key_file, "config", "audio_device", NULL));
    }
    if (g_key_file_has_group(key_file, "window") == TRUE) {
    window_width = g_key_file_get_integer(key_file, "window", "width", NULL);
    window_height = g_key_file_get_integer(key_file, "window", "height", NULL);
    window_x = g_key_file_get_integer(key_file, "window", "x", NULL);
    window_y = g_key_file_get_integer(key_file, "window", "y", NULL);
    window_maximised = g_key_file_get_boolean(key_file, "window", "maximised", NULL);
    }
    g_key_file_free (key_file);
}

void save_preferences(void)
{
    GKeyFile  *key_file = read_config();

    g_key_file_set_string(key_file, "config", "pathname", pathname);
    g_key_file_set_string(key_file, "config", "last_filename", last_filename);
    g_key_file_set_integer(key_file, "config", "rate", prefs.rate);
    g_key_file_set_integer(key_file, "config", "bits", prefs.bits);
    g_key_file_set_integer(key_file, "config", "stereo", prefs.stereo);
    g_key_file_set_integer(key_file, "config", "first_sample_viewed", audio_view.first_sample);
    g_key_file_set_integer(key_file, "config", "last_sample_viewed", audio_view.last_sample);
    g_key_file_set_integer(key_file, "config", "channel_selection_mask", audio_view.channel_selection_mask);
    g_key_file_set_double(key_file, "config", "weak_declick_sensitivity", weak_declick_sensitivity);
    g_key_file_set_double(key_file, "config", "strong_declick_sensitivity", strong_declick_sensitivity);
    g_key_file_set_integer(key_file, "config", "declick_iterate", declick_iterate_flag);
    g_key_file_set_double(key_file, "config", "weak_fft_declick_sensitivity", weak_fft_declick_sensitivity);
    g_key_file_set_double(key_file, "config", "strong_fft_declick_sensitivity", strong_fft_declick_sensitivity);
    g_key_file_set_integer(key_file, "config", "declick_detector_type", declick_detector_type);
    g_key_file_set_double(key_file, "config", "decrackle_level", decrackle_level);
    g_key_file_set_integer(key_file, "config", "decrackle_window", decrackle_window);
    g_key_file_set_integer(key_file, "config", "decrackle_average", decrackle_average);
    g_key_file_set_double(key_file, "config", "stop_key_highlight_interval", stop_key_highlight_interval);
    g_key_file_set_double(key_file, "config", "song_key_highlight_interval", song_key_highlight_interval);
    g_key_file_set_double(key_file, "config", "song_mark_silence", song_mark_silence);
    g_key_file_set_integer(key_file, "config", "sonogram_log", sonogram_log);
    g_key_file_set_string(key_file, "config", "audio_device", audio_device);
    g_key_file_set_integer(key_file, "window", "width", window_width);
    g_key_file_set_integer(key_file, "window", "height", window_height);
    g_key_file_set_integer(key_file, "window", "x", window_x);
    g_key_file_set_integer(key_file, "window", "y", window_y);
    g_key_file_set_boolean(key_file, "window", "maximised", window_maximised);
    write_config(key_file);
}

void display_message(char *msg, char *title) 
{
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, msg);

    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy( GTK_WIDGET(dialog) );
    
    main_redraw(FALSE, TRUE);
}

void warning(char *msg)
{
    display_message(msg, "Warning");
}

void info(char *msg)
{
    display_message(msg, "Info");
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
        dres = 2 ;              /* return we clicked cancel */
    } else if (dres == GTK_RESPONSE_YES) {
        dres = 0 ;              /* return we clicked yes */
    } else {
        dres = 1 ;
    }

    main_redraw(FALSE, TRUE);

    return dres;
}

int yesno(char *msg)
{
    int ret;
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, msg);

    gtk_window_set_title(GTK_WINDOW(dialog), "GWC");
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy( GTK_WIDGET(dialog) );
    
    if (result == GTK_RESPONSE_YES) {
        ret = 0;
    } else {
        ret = 1;
    }
    
    main_redraw(FALSE, TRUE);
    
    return ret;
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
        dres = 1 ;              /* return we clicked cancel */
    } else {
        dres = 0 ;              /* return we clicked yes */
        strcpy(s, gtk_entry_get_text(GTK_ENTRY(entry)));
    }

    gtk_widget_destroy(dlg) ;

    main_redraw(FALSE, TRUE);

    return dres;
}

void show_help(const char *uri)
{
    char buf[256];
    sprintf(buf, "xdg-open %s", uri);
    system(buf);
    return;
}

void help(GtkWidget * widget, gpointer data)
{
    show_help("https://github.com/audioquality/gwc") ;
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
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
        return;

    if (audio_view.selection_region == FALSE) {
        set_status_text("Please select audio to declick first");
        return;
    }

    file_processing = TRUE;

    if(declick_detector_type == HPF_DETECT)
        declick_with_sensitivity(strong_declick_sensitivity);
    else
        declick_with_sensitivity(strong_fft_declick_sensitivity);

    file_processing = FALSE;
}

void declick_weak(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
        return;

    if (audio_view.selection_region == FALSE) {
        set_status_text("Please select audio to declick first");
        return;
    }
    
    file_processing = TRUE;

    if(declick_detector_type == HPF_DETECT)
        declick_with_sensitivity(weak_declick_sensitivity);
    else
        declick_with_sensitivity(weak_fft_declick_sensitivity);

    file_processing = FALSE;

}

void estimate(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
        return;

    if (audio_view.selection_region == FALSE) {
        set_status_text("Please select region to estimate first");
        return;
    }

    long first, last;
    file_processing = TRUE;
    get_region_of_interest(&first, &last, &audio_view);
    dethunk(&prefs, first, last, audio_view.channel_selection_mask);
    main_redraw(FALSE, TRUE);
    set_status_text("Estimate done");
    file_processing = FALSE;
}

void manual_declick(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
        return;

    if (audio_view.selection_region == FALSE) {
        set_status_text("Please select audio to declick first");
        return;
    }

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
        set_status_text("Manual declick done");

        close_undo();

        main_redraw(FALSE, TRUE);
    }

    file_processing = FALSE;

}

void decrackle(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
        return;

    if (audio_view.selection_region == FALSE) {
        set_status_text("Please select audio to decrackle first");
        return;
    }

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
        set_status_text("Decrackle done");
    }

    main_redraw(FALSE, TRUE);
    file_processing = FALSE;
}

void noise_sample(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
        return;

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

void remove_noise(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
        return;
    
    if (denoise_data.ready == FALSE) {
        set_status_text("Please select the noise sample first");
        return;
    }

    file_processing = TRUE;

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
            push_status_text("Denoising selection...");
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
            set_status_text("Denoise done");
        }
        

        main_redraw(FALSE, TRUE);
    }

    file_processing = FALSE;
}

void undo_callback(GtkWidget * widget, gpointer data)
{
    if (file_is_open == FALSE) {
        set_status_text("Nothing to Undo");
        return;
    }
    if (audio_playback == TRUE) {
        set_status_text("Please try Undo when Audio Playback has stopped");
        return;
    }
    if (file_processing == TRUE)
        return;
  
    file_processing = TRUE;
    undo(&audio_view, &prefs);
    main_redraw(FALSE, TRUE);
    file_processing = FALSE;
    
}


void scale_up_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
        return;

    file_processing = TRUE;
    view_scale *= 1.25;
    main_redraw(FALSE, TRUE);
    file_processing = FALSE;
}

void cut_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
      return;

    if (!is_region_selected())
      return;
      
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

void copy_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
      return;
      
    if (!is_region_selected())
      return;

    file_processing = TRUE;
    audioedit_copy_selection(&audio_view);
    file_processing = FALSE;
}

void paste_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
      return;
      
    if (!is_region_selected())
      return;

    if (audioedit_has_clipdata()) {
        file_processing = TRUE;
        audioedit_paste_selection(&audio_view);
        main_redraw(FALSE, TRUE);
        file_processing = FALSE;
    } else {
        info("No audio data in internal clipboard.");
    }
}

void delete_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
      return;
      
    if (!is_region_selected())
      return;
  
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

void silence_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
      return;
      
    if (!is_region_selected())
      return;

    if(!yesno("Insert silence can take a long time, continue?")) {
        file_processing = TRUE;
        audioedit_insert_silence(&audio_view);
        main_redraw(FALSE, TRUE);
        file_processing = FALSE;
    }
}

void scale_reset_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
      return;

    file_processing = TRUE;
    view_scale = 1.00;
    spectral_amp = 1.00;
    main_redraw(FALSE, TRUE);
    file_processing = FALSE;
}

void scale_down_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
      return;
  
    file_processing = TRUE;
    view_scale /= 1.25;
    main_redraw(FALSE, TRUE);
    file_processing = FALSE;
}

void stop_all_playback_functions(GtkWidget * widget, gpointer data)
{
    if (cursor_timer != 0) {
        g_source_remove(cursor_timer);    
        cursor_timer = 0 ;
    }

    if (playback_timer != 0) {
        g_source_remove(playback_timer);
        playback_timer = 0 ;
    }
    
    stop_playback(0);
    
    audio_playback = FALSE;
    audio_is_looping = FALSE;

    audio_view.cursor_position = playback_startplay_position;
    
    // show current playback position, if outside of the view
    if ((audio_view.cursor_position > audio_view.last_sample) ||
        (audio_view.cursor_position < audio_view.first_sample)) {
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

void gtk_flush(void)
{
    while (gtk_events_pending())
        gtk_main_iteration();
}

/*
 * timer callback
 * 
 * run from start_gwc_playback()
 * 
 * repaint the cursor in the waveform display by main_redraw()
 */
gboolean update_cursor(gpointer data)
{
    if (!audio_playback) {
      audio_debug_print("update_cursor stopped due to audio_playback=false\n");
      return FALSE;
    }
    long first, last;
    get_region_of_interest(&first, &last, &audio_view);
    

    struct timespec cur_time;
    clock_gettime(CLOCK_REALTIME, &cur_time);
    // how long is audio playing
    long playtime_ms = (cur_time.tv_sec*1000 + cur_time.tv_nsec/1000000) - (ts_playback_start.tv_sec*1000 + ts_playback_start.tv_nsec/1000000);
    // how many samples are we from the playback start position
    long samples_from_start = playtime_ms * prefs.rate / 1000;
    //audio_debug_print("update_cursor: playtime=%u, end=%u, sp=%u\n", playtime_ms, last+1, playback_startplay_position);
    samples_from_start = samples_from_start + playback_startplay_position - first;
    if (audio_is_looping) {
      samples_from_start %= last - first;
    }
    // update audio_view.cursor_position
    audio_view.cursor_position = first + samples_from_start ;
    
    if (!audio_is_looping) {
      // stop playback at the end
      if ((audio_view.cursor_position == 0) || (audio_view.cursor_position >= last)) {
        audio_view.cursor_position = first;
          stop_all_playback_functions(NULL, NULL);
          // repaint cursor
          main_redraw(TRUE, TRUE);
          return FALSE;
      }
    }

    // autoscroll on playback
    //
    //  NOTE:  autoscrolling is omitted if cursor_samples_per_pixel is too low (zoomed in too much),
    //         this would cause buffering failure (audio skipping)
    //
    long cursor_samples_per_pixel = (audio_view.last_sample - audio_view.first_sample) / audio_view.canvas_width;
    if ((cursor_samples_per_pixel > 100) &&
      (audio_view.cursor_position > (audio_view.last_sample - 
      ((audio_view.last_sample - audio_view.first_sample) / 30)))) {
      
      long sample_shift = (audio_view.last_sample - audio_view.first_sample) * 0.9;
      audio_debug_print("autoscrolling %d samples\n", sample_shift) ;
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
    
    return (TRUE);
}

/*
 * timer callback
 * 
 * init from start_gwc_playback()
 * 
 * call audio_util.c: process_audio() to load audio data from file into buffer
 */
gboolean play_a_block(gpointer data)
{
  if (audio_playback) {
      process_audio();
  }
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

    if (file_is_open == FALSE || file_processing == TRUE || audio_playback == TRUE)
      return;
    
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
    // limits on screen redraws
    // too high value would cause cursor "jump" at the end of the playback
    if (cursor_millisec < 25)
        cursor_millisec = 25;
    else if (cursor_millisec > 200)
        cursor_millisec = 200;

    audio_debug_print("play_a_block timer: %ld ms\n", playback_millisec);
    audio_debug_print("update_cursor timer: %ld ms\n", cursor_millisec);
    audio_debug_print("cursor_samples_per_pixel: %ld\n", cursor_samples_per_pixel);
    
    // count iterations of update_cursor() during playback
    clock_gettime(CLOCK_REALTIME, &ts_playback_start);
    
    audio_playback = TRUE;
    play_a_block(NULL);
    //playback_timer = g_timeout_add(playback_millisec, play_a_block, NULL);
    //cursor_timer = g_timeout_add_full(G_PRIORITY_HIGH_IDLE + 20, cursor_millisec, update_cursor, NULL, NULL);
    playback_timer = gdk_threads_add_timeout(playback_millisec, play_a_block, NULL);
    cursor_timer = gdk_threads_add_timeout_full(G_PRIORITY_HIGH_IDLE + 20, cursor_millisec, update_cursor, NULL, NULL);

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
                set_status_text("Amplify done");
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

        if(audio_view.selected_first_sample < 0) 
          audio_view.selected_first_sample = 0;
        if(audio_view.selected_last_sample > prefs.n_samples-1)
          audio_view.selected_last_sample = prefs.n_samples-1;

        audio_view.selection_region = FALSE;
        set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample, audio_view.last_sample);
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
        audio_view.channel_selection_mask = 0x03;
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

/*      for(i = 0 ; i < num_song_markers ; i++) {  */
/*          printf("marker %2d at sample %d\n", i, song_markers[i]) ;  */
/*      }  */
/*    */
/*      i=0 ;  */
        

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
                d_print("Save as wavfile %s %ld->%ld\n", filename, first_sample, last_sample) ;
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
        audio_view.channel_selection_mask = 0x03;
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
                i--;            /* current marker deleted, next marker dropped into position i */
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
                // NOTE: audio_is_looping has to be set before start_gwc_playback() call!
                if ((event->state & GDK_CONTROL_MASK) || (event->state & GDK_SHIFT_MASK))
                  audio_is_looping = TRUE;
                start_gwc_playback(widget, data);
            } else {
              stop_all_playback_functions(widget, data);
              // repaint cursor
              main_redraw(TRUE, TRUE);
            }
            break;
        case GDK_s:
        // select last S seconds of audio
            if (audio_playback == TRUE) {
                stop_all_playback_functions(widget, data);
                audio_view.selected_last_sample = audio_view.cursor_position;
                audio_view.selected_first_sample = MAX(0,
                    (audio_view.selected_last_sample - prefs.rate * stop_key_highlight_interval));
                audio_view.selection_region = TRUE;
                playback_startplay_position = audio_view.selected_first_sample;
                audio_view.cursor_position = playback_startplay_position;
                main_redraw(FALSE, TRUE);
            }
            break;
        case GDK_Escape:
        // deselect audio
            if (audio_playback == FALSE) {
                audio_view.selected_first_sample = 0;
                audio_view.selected_last_sample = prefs.n_samples - 1;
                audio_view.selection_region = FALSE;
                audio_view.channel_selection_mask = 0x03;
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
        case GDK_KEY_KP_Add:
        // zoom in time scale
            zoom_in(widget, data);
            break;
        case GDK_KEY_Down:
        case GDK_KEY_KP_Subtract:
        // zoom out time scale
            zoom_out(widget, data);
            break;
        case GDK_KEY_KP_Multiply:
        // select view
            select_all(widget, data);
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

    if(file_is_open && get_undo_levels() > 0) {
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
    if ( file_processing == TRUE ) {
        warning("Can't quit while file is being processed.");
        return TRUE;
    }
    if (cleanup_and_close(&audio_view, &prefs)) {
        gtk_main_quit();
    }
    return FALSE;
}

void about(GtkWidget * widget, gpointer data)
{
    const gchar *authors[] = { "Jeffrey J. Welty", "James Tappin", "Ian Leonard", "Bill Jetzer", "Charles Morgon", "Frank Freudenberg", "Thiemo Gehrke", "Rob Frohne", "Alister Hood", NULL };
    const gchar* LICENSE="This program is free software; you can redistribute it and/or "
        "modify it under the terms of the GNU General Public License "
        "as published by the Free Software Foundation; either version 2 "
        "of the License, or (at your option) any later version.\n\n"
        "This program is distributed in the hope that it will be useful, "
        "but WITHOUT ANY WARRANTY; without even the implied warranty of "
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
        "GNU General Public License for more details.\n"
        "You should have received a copy of the GNU General Public License "
        "along with this program; if not, write to the Free Software "
        "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.";

    gtk_show_about_dialog(GTK_WINDOW( NULL ),
        "version", VERSION,
        "copyright", "Copyright 2001, 2002, 2003, 2004, 2005 Redhawk.org",
        "license", LICENSE,
        "wrap-license", TRUE,
        "website", "http://gwc.sourceforge.net/",
        "comments", "Removes noise (hiss, pops and clicks) from audio files in WAV and similar formats.\ne.g recordings of scratchy vinyl records.",
        "authors", authors,
        "logo-icon-name", APPNAME,
        NULL);
}

void main_redraw(int cursor_flag, int redraw_data)
{
    if (doing_progressbar_update == TRUE)
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

    gtk_flush();

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
                audio_view.channel_selection_mask = 0x03;
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
            }
        } else {
            strcpy(wave_filename, last_filename);
        }
    } else {
        file_is_open = FALSE;
    }
}

void open_file_selection(GtkWidget * widget, gpointer data)
{
    if ((file_processing == TRUE) || (audio_playback == TRUE))
      return;

    GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open WAV File:",
                                          GTK_WINDOW(main_window),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                          NULL);
                                          
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name (filter, "WAV Files");
    gtk_file_filter_add_pattern (filter, "*.wav");    
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), pathname);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        char strbuf[PATH_MAX+100];
        
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        d_print("Selected file: %s\n", filename);

        char* ext = strrchr(filename, '.');
        if (strcasecmp(ext, ".wav") == 0) {
          strcpy(wave_filename, filename);
          open_wave_filename();
          if (file_is_open) {
              sprintf(strbuf, "File: %s", wave_filename);
          } else {
              sprintf(strbuf, "Error opening file: %s", wave_filename);
          }
          set_status_text(strbuf);
        } else {
          set_status_text("File type not supported. Please open a 16bit 44.1kHz WAV file.");
        }
        g_free (filename);
    }
    gtk_widget_destroy (dialog);
}

void save_as_selection(GtkWidget * widget, gpointer data)
{

    if ((file_processing == TRUE) || (file_is_open == FALSE) || (audio_playback == TRUE))
      return;

    if (audio_view.selection_region == FALSE) {
      info("Please highlight a region to save first");
      return;
    }

    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new ("Save selection to file:",
                                          GTK_WINDOW(main_window),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
    
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern (filter, "*.wav");    
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);    
    
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), pathname);
      
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        
        if (strcmp(filename, wave_filename)) {
            save_selection_as_wavfile(filename, &audio_view);
        } else {
            warning("Cannot save selection over the currently open file!");
        }       
        
        g_free (filename);
    }
    gtk_widget_destroy (dialog);
}


static struct {
	gchar *stockid;
	const char **icon_xpm;
} stock_icons[] = {
	{"filter_icon", filter_xpm },
	{"pinknoise_icon", pinknoise_xpm },
	{"amplify_icon", amplify_xpm },
	{"declick_icon", declick_xpm },
	{"gwc_icon", gtk_wave_cleaner_xpm },
	{"declick_w_icon", declick_w_xpm },
	{"declick_m_icon", declick_m_xpm },
	{"decrackle_icon", decrackle_xpm },
	{"estimate_icon", estimate_xpm },
	{"noise_sample_icon", noise_sample_xpm },
	{"remove_noise_icon", remove_noise_xpm },
	{"silence_icon", silence_xpm },
	{"zoom_sel_icon", zoom_sel_xpm },
	{"zoom_in_icon", zoom_in_xpm },
	{"zoom_out_icon", zoom_out_xpm },
	{"view_all_icon", view_all_xpm },
	{"select_all_icon", select_all_xpm },
	{"spectral_icon", spectral_xpm },
	{"start_icon", start_xpm },
	{"stop_icon", stop_xpm }
};

static gint n_stock_icons = G_N_ELEMENTS (stock_icons);

static void
register_stock_icons (void)
{
	GtkIconFactory *icon_factory;
	GtkIconSet *icon_set;
	GdkPixbuf *pixbuf;
	gint i;

	icon_factory = gtk_icon_factory_new ();

	for (i = 0; i < n_stock_icons; i++)
	{
		pixbuf = gdk_pixbuf_new_from_xpm_data(stock_icons[i].icon_xpm);
		icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
		g_object_unref(pixbuf);
		gtk_icon_factory_add (icon_factory, stock_icons[i].stockid, icon_set);
		gtk_icon_set_unref (icon_set);
	}

	gtk_icon_factory_add_default(icon_factory);

	g_object_unref(icon_factory);
}

/* Normal items */
static const GtkActionEntry entries[] = {
  { "FileMenu", NULL, "File" },
  { "Open", GTK_STOCK_OPEN, "Open...", "<control>O", "Open a file", G_CALLBACK(open_file_selection) },
  { "SaveSelection", NULL, "Save selection as...", "<control>S", "Save the current selection to a new wavfile", G_CALLBACK(save_as_selection) },
  //{ "SaveSplit", NULL, "Split audio on song markers", NULL, "Create individual track files", G_CALLBACK(split_audio_on_markers) },
  { "Quit", GTK_STOCK_QUIT, "Quit", "<control>Q", "Close GWC", G_CALLBACK(delete_event) },
  { "EditMenu", NULL, "Edit" },
  { "Undo", GTK_STOCK_UNDO, "Undo", "BackSpace", "Undo the last edit", G_CALLBACK(undo_callback) },
  { "Filter", "filter_icon", "Apply DSP Frequency Filters", NULL, "lowpass, highpass, notch or bandpass biquad filtering", G_CALLBACK(filter_cb) },
  { "PinkNoise", "pinknoise_icon", "Generate Pink Noise", NULL, "Replace current view or selection with pink noise", G_CALLBACK(pinknoise_cb) },
  { "Amplify", "amplify_icon", "Amplify", NULL, "Amplify or attenuate the current view or selection", G_CALLBACK(amplify) },
  { "DeclickStrong", "declick_icon", "Declick Strong", "U", "Automatically find and repair pops/clicks in current view or selection", G_CALLBACK(declick) },
  { "DeclickWeak", "declick_w_icon", "Declick Weak", "I", "Automatically find and repair weaker pops/clicks in current view or selection", G_CALLBACK(declick_weak) },
  { "DeclickManual", "declick_m_icon", "Declick Manual", "O", "Apply LSAR signal estimation to repair a single selected/viewed click", G_CALLBACK(manual_declick) },
  { "Decrackle", "decrackle_icon", "Decrackle", "C", "Remove crackle from old, deteriorated vinyl", G_CALLBACK(decrackle) },
  { "Estimate", "estimate_icon", "Estimate", "K", "Estimate signal (> 300 samples) in current view or selection", G_CALLBACK(estimate) },
  { "Sample", "noise_sample_icon", "Sample", "H", "Use current view or selection as a noise sample", G_CALLBACK(noise_sample) },
  { "Denoise", "remove_noise_icon", "Denoise", "J", "Remove noise from current view or selection", G_CALLBACK(remove_noise) },
  { "Silence", "silence_icon", "Silence", NULL, "Insert silence with size of current selection", G_CALLBACK(silence_callback) },
  { "Reverb", NULL, "Reverb", NULL, "Apply reverberation to the current view or selection", G_CALLBACK(reverb) },
  { "Cut", GTK_STOCK_CUT, "Cut", NULL, "Cut current selection to internal clipboard", G_CALLBACK(cut_callback) },
  { "Copy", GTK_STOCK_COPY, "Copy", NULL, "Copy current selection to internal clipboard", G_CALLBACK(copy_callback) },
  { "Paste", GTK_STOCK_PASTE, "Paste", NULL, "Insert internal clipboard before current selection", G_CALLBACK(paste_callback) },
  { "Delete", GTK_STOCK_DELETE, "Delete", NULL, "Delete current selection from audio data", G_CALLBACK(delete_callback) },
  { "ViewMenu", NULL, "_View" },
  { "ZoomSelect", "zoom_sel_icon", "Zoom to selection", "slash", "Zoom in on selected portion", G_CALLBACK(zoom_select) },
  { "ZoomIn", "zoom_in_icon", "Zoom in", "plus", "Zoom in", G_CALLBACK(zoom_in) },
  { "ZoomOut", "zoom_out_icon", "Zoom out", "minus", "Zoom out", G_CALLBACK(zoom_out) },
  { "ViewAll", "view_all_icon", "View all", "backslash", "View entire audio file", G_CALLBACK(view_all) },
  { "SelectAll", "select_all_icon", "Select current view", "asterisk", "Select everything visible in the window", G_CALLBACK(select_all) },
  { "Spectral", "spectral_icon", "Spectral view", NULL, "Toggle between waveform view and spectrogram", G_CALLBACK(display_sonogram) },
  { "MarkersMenu", NULL, "Markers" },
  { "ToggleBegin", NULL, "Toggle beginning marker", "N", "Add or remove an edit marker at the beginning of the highlighted selection (or current view)", G_CALLBACK(toggle_start_marker) },
  { "ToggleEnd", NULL, "Toggle ending marker", "M", "Add or remove an edit marker at the end of the highlighted selection (or current view)", G_CALLBACK(toggle_end_marker) },
  { "ClearMarkers", NULL, "Clear markers", "R", "Remove all edit markers in the highlighted selection (or current view)", G_CALLBACK(clear_markers_in_view) },
  { "ExpandSelection", NULL, "Expand selection to nearest markers", "E", "Select the region between two edit markers", G_CALLBACK(select_markers) },
  { "MarkSongs", NULL, "Detect songs", NULL, "Mark all songs in the file using silence detection", G_CALLBACK(mark_songs) },
  { "MoveMarker", NULL, "Move song marker", NULL, "Move the closest song marker to the beginning of the current (or most recent) selection", G_CALLBACK(move_song_marker) },
  { "AddMarker", NULL, "Add song marker", NULL, "Add a song marker at the beginning of the current (or most recent) selection", G_CALLBACK(add_song_marker) },
  { "AddMarkerPair", NULL, "Add song marker pair", NULL, "Add a song marker at the beginning AND end of the current (or most recent) selection", G_CALLBACK(add_song_marker_pair) },
  { "DeleteMarker", NULL, "Clear song markers", NULL, "Remove all song markers in the current (or most recent) selection", G_CALLBACK(delete_song_marker) },
  { "NextMarker", NULL, "Next song marker", "N", "Select 15 seconds of audio around the next song marker", G_CALLBACK(select_song_marker) },
  { "SettingsMenu", NULL, "Settings" },
  { "DeclickPrefs", NULL, "Declick", "P", "Set declick sensitivity, iteration", G_CALLBACK(declick_set_preferences) },
  { "DecracklePrefs", NULL, "Decrackle", "semicolon", "Set decrackle sensitivity", G_CALLBACK(decrackle_set_preferences) },
  { "DenoisePrefs", NULL, "Denoise", "L", "Set denoise parameters", G_CALLBACK(denoise_set_preferences) },
  { "MiscPrefs", NULL, "Miscellaneous", "<control>P", "Miscellaneous parameters", G_CALLBACK(set_options) },
  { "HelpMenu", NULL, "Help" },
  { "Help", GTK_STOCK_HELP, "User Guide", NULL, "Instructions for using GWC", G_CALLBACK(help) },
  { "About", GTK_STOCK_ABOUT, "About", NULL, "About GWC", G_CALLBACK(about) },
  { "PlayAudio", "start_icon", "Start audio playback", NULL, "Playback the selected or current view of audio", G_CALLBACK(start_gwc_playback) },
  { "StopAudio", "stop_icon", "Stop", NULL, "Stop audio playback", G_CALLBACK(stop_all_playback_functions) }
};

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Open'/>"
"      <menuitem action='SaveSelection'/>"
//"      <menuitem action='SaveSplit'/>"
"      <separator/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='EditMenu'>"
"      <menuitem action='Undo'/>"
"      <menuitem action='Filter'/>"
"      <menuitem action='PinkNoise'/>"
"      <menuitem action='Amplify'/>"
"      <menuitem action='DeclickStrong'/>"
"      <menuitem action='DeclickWeak'/>"
"      <menuitem action='DeclickManual'/>"
"      <menuitem action='Decrackle'/>"
"      <menuitem action='Estimate'/>"
"      <menuitem action='Sample'/>"
"      <menuitem action='Denoise'/>"
"      <menuitem action='Silence'/>"
"      <menuitem action='Reverb'/>"
"      <separator/>"
"      <menuitem action='Cut'/>"
"      <menuitem action='Copy'/>"
"      <menuitem action='Paste'/>"
"      <menuitem action='Delete'/>"
"    </menu>"
"    <menu action='ViewMenu'>"
"      <menuitem action='ZoomSelect'/>"
"      <menuitem action='ZoomIn'/>"
"      <menuitem action='ZoomOut'/>"
"      <menuitem action='ViewAll'/>"
"      <menuitem action='SelectAll'/>"
"      <menuitem action='Spectral'/>"
"    </menu>"
"    <menu action='MarkersMenu'>"
"      <menuitem action='ToggleBegin'/>"
"      <menuitem action='ToggleEnd'/>"
"      <menuitem action='ClearMarkers'/>"
"      <menuitem action='ExpandSelection'/>"
"      <separator/>"
"      <menuitem action='MarkSongs'/>"
"      <menuitem action='MoveMarker'/>"
"      <menuitem action='AddMarker'/>"
"      <menuitem action='AddMarkerPair'/>"
"      <menuitem action='DeleteMarker'/>"
"      <menuitem action='NextMarker'/>"
"    </menu>"
"    <menu action='SettingsMenu'>"
"      <menuitem action='DeclickPrefs'/>"
"      <menuitem action='DecracklePrefs'/>"
"      <menuitem action='DenoisePrefs'/>"
"      <menuitem action='MiscPrefs'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='Help'/>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"  <toolbar name='MainToolbar' action='MainToolbarAction'>"
"    <placeholder name='ToolItems'>"
"      <separator/>"
"      <toolitem action='PlayAudio'/>"
"      <toolitem action='StopAudio'/>"
"      <toolitem action='ZoomSelect'/>"
"      <toolitem action='ZoomIn'/>"
"      <toolitem action='ZoomOut'/>"
"      <toolitem action='ViewAll'/>"
"      <toolitem action='SelectAll'/>"
"      <toolitem action='Spectral'/>"
"      <separator/>"
"      <separator/>"
"      <toolitem action='Undo'/>"
"      <separator/>"
"      <toolitem action='Amplify'/>"
"      <toolitem action='DeclickStrong'/>"
"      <toolitem action='DeclickWeak'/>"
"      <toolitem action='DeclickManual'/>"
"      <toolitem action='Decrackle'/>"
"      <toolitem action='Estimate'/>"
"      <toolitem action='Sample'/>"
"      <toolitem action='Denoise'/>"
"      <toolitem action='Silence'/>"
"      <separator/>"
"      <toolitem action='Cut'/>"
"      <toolitem action='Copy'/>"
"      <toolitem action='Paste'/>"
"      <toolitem action='Delete'/>"
"      <separator/>"
"    </placeholder>"
"  </toolbar>"
"</ui>";

void update_progress_bar(gfloat percentage, gfloat min_delta,
       gboolean init_flag)
{
#ifdef BY_DATA_LENGTH
    static gfloat last_percentage_displayed = -1.0;

    if (percentage - last_percentage_displayed > min_delta || init_flag == TRUE) {
        doing_progressbar_update = TRUE;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR(progress_bar), percentage);
        gtk_flush();
        doing_progressbar_update = FALSE;
        last_percentage_displayed = percentage;
    }
#else
    static struct timeval this, last_displayed = { 0, 0 };
    static struct timezone tz = { 0, 0 };
    long delta_ms;

    gettimeofday(&this, &tz);

    delta_ms = (this.tv_sec*1000 + this.tv_usec/1000) -
               (last_displayed.tv_sec*1000 + last_displayed.tv_usec/1000);

    if (delta_ms > 1000 * min_delta || init_flag == TRUE) {
        doing_progressbar_update = TRUE;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR(progress_bar), percentage);
        gtk_flush();
        doing_progressbar_update = FALSE;
        last_displayed = this;
    }
#endif
}

void set_status_text(gchar * msg)
{
    gtk_statusbar_pop (status_bar, status_message);
    gtk_statusbar_push (status_bar, status_message, msg);
}

void push_status_text(gchar * msg)
{
    gtk_statusbar_push(status_bar, push_message, msg);
}

void pop_status_text(void)
{
    gtk_statusbar_pop(status_bar, push_message);
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
    gtk_window_get_size (GTK_WINDOW(main_window), &window_width, &window_height);

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
/*             event->area.x, event->area.y,  */
/*             event->area.width, event->area.height);  */

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

void gwc_signal_handler(int sig)
{
    if (close_wavefile(&audio_view)) {
        save_preferences();
        undo_purge();
    }

    switch (sig) {
        case SIGSEGV:
            display_message("Segmentation Fault", "Fatal");
            break;
        case SIGBUS:
            display_message("Bus Error", "Fatal");
            break;
    }
    exit(1);
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
    
    GtkWidget *main_vbox, *led_vbox, *track_times_vbox, *times_vbox, *bottom_hbox, *menubar, *toolbar;
    GtkWidget *detect_only_box;
    GtkWidget *leave_click_marks_box;
    GtkActionGroup *action_group;
    GtkUIManager *ui_manager;
    GError *error;

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

    load_preferences();

    /* load ~/.config/gtk-wave-cleaner/gtkrc if it exists */
    gtk_rc_add_default_file ( g_build_filename (g_get_user_config_dir (), APPNAME, "gtkrc", NULL) );

    /* This is called in all GTK applications. Arguments are parsed
     * from the command line and are returned to the application. */
    gtk_init(&argc, &argv);
    g_set_application_name("Gtk Wave Cleaner");
  
    register_stock_icons ();

    main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), APPNAME) == FALSE) {
      GdkPixbuf *icon =  gtk_widget_render_icon(main_window, "gwc_icon", 6, NULL);
      gtk_icon_theme_add_builtin_icon(APPNAME, 48, icon);
    }
    gtk_window_set_default_icon_name(APPNAME);
    // Remember the window geometry from the last time GWC was used, but check for sanity
    // Looks like we don't actually need to get this information - see comments below.
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gint screen_width = gdk_screen_get_width(screen);
    gint screen_height = gdk_screen_get_height(screen);
    gtk_window_set_default_size(GTK_WINDOW(main_window), 
      MIN( window_width, screen_width), MIN(window_height, screen_height) );
    // Looks like we could just set it to window_x and window_y - perhaps because I have a helpful window manager?
    gtk_window_move(GTK_WINDOW(main_window), 
        MIN( window_x, screen_width - 20), MIN( window_y, screen_height - 20 ) );
    if (window_maximised == TRUE) {
        gtk_window_maximize(GTK_WINDOW(main_window));
    }
    
    /* When the window is given the "delete_event" signal (this is given
     * by the window manager, usually by the "close" option, or on the
     * titlebar), we ask it to call the delete_event () function
     * as defined above. The data passed to the callback
     * function is NULL and is ignored in the callback function. */
    gtk_signal_connect(GTK_OBJECT(main_window), "delete_event",
                       GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

    g_signal_connect(GTK_OBJECT(main_window), "key_press_event",
                       GTK_SIGNAL_FUNC(key_press_cb), NULL);

    /* Sets the border width of the window. */
    gtk_container_set_border_width(GTK_CONTAINER(main_window), 1);

    main_vbox = gtk_vbox_new(FALSE, 1);
    track_times_vbox = gtk_vbox_new(FALSE, 1);
    times_vbox = gtk_vbox_new(FALSE, 1);
    led_vbox = gtk_vbox_new(FALSE, 1);
    bottom_hbox = gtk_hbox_new(FALSE, 1);
    gtk_container_add (GTK_CONTAINER (main_window), main_vbox);

    ui_manager = gtk_ui_manager_new();
    action_group = gtk_action_group_new("MenuActions");
    gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS(entries), main_window);
    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    gtk_accel_map_load( g_build_filename (g_get_user_config_dir(), APPNAME, ACCELERATORS_FILE, NULL) );
    gtk_window_add_accel_group (GTK_WINDOW(main_window), gtk_ui_manager_get_accel_group(ui_manager));
    error = NULL;
    if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &error)) {
        g_message ("building menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
    }

    menubar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
    gtk_box_pack_start (GTK_BOX(main_vbox), menubar, FALSE, FALSE, 0);
    toolbar = gtk_ui_manager_get_widget (ui_manager, "/MainToolbar");
    gtk_toolbar_set_style (GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_box_pack_start (GTK_BOX(main_vbox), toolbar, FALSE, FALSE, 0);
    progress_bar = gtk_progress_bar_new();
    status_bar = GTK_STATUSBAR(gtk_statusbar_new());
    status_message = gtk_statusbar_get_context_id (status_bar, "status-message");
    push_message = gtk_statusbar_get_context_id (status_bar, "push-message");


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
                       "button_release_event",
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
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(progress_bar), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), GTK_WIDGET(status_bar), FALSE, TRUE, 0);

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

    signal(SIGSEGV, gwc_signal_handler);
    signal(SIGBUS, gwc_signal_handler);

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
