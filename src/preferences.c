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


/* preferences.c */
/* preference settings for GWC */

#include <glib.h>
#include <string.h>
#include "gwc.h"

int gwc_dialog_run(GtkDialog *dlg)
{
    int dres ;

    dres = gtk_dialog_run(GTK_DIALOG(dlg));

    if (dres == GTK_RESPONSE_CANCEL)
	return 1 ;

    return 0 ;
}

/*
 * Settings -> Options menu window
 */
void set_options(GtkWidget * widget, gpointer data)
{
    extern double stop_key_highlight_interval;
    extern double song_key_highlight_interval;
    extern double song_mark_silence;
    extern int sonogram_log;
    GtkWidget *dlg;
    GtkWidget *stop_interval_entry;
    GtkWidget *song_interval_entry;
    GtkWidget *dialog_table;
    GtkWidget *normalize_entry;
    GtkWidget *silence_entry;
    GtkWidget *sonogram_log_entry;
    GtkWidget *audio_device_entry;
    extern char audio_device[];
    extern int denoise_normalize;
    int dres;
    int row = 0;

    dlg =
	gtk_dialog_new_with_buttons("Preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,

			 GTK_STOCK_OK, GTK_RESPONSE_OK, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 NULL, NULL);

    dialog_table = gtk_table_new(6, 2, 0);


    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 5);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);
    gtk_widget_show(dialog_table);

    stop_interval_entry =
	add_number_entry_with_label_double(stop_key_highlight_interval,
					   "Seconds of audio pre-selected when \"S\" key is struck",
					   dialog_table, row++);
    song_interval_entry =
	add_number_entry_with_label_double(song_key_highlight_interval,
					   "Seconds of audio highlighted around song marker when markers are \"shown\"",
					   dialog_table, row++);
    normalize_entry =
	add_number_entry_with_label_int(denoise_normalize,
					"Normalize values for declick, denoise?",
					dialog_table, row++);
    silence_entry =
	add_number_entry_with_label_double(song_mark_silence,
					   "Silence estimate in seconds for marking songs",
					   dialog_table, row++);

    sonogram_log_entry =
	gtk_check_button_new_with_label("Log frequency in sonogram");
    if (sonogram_log)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sonogram_log_entry),
				     TRUE);
    gtk_widget_show(sonogram_log_entry);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), sonogram_log_entry,
			      0, 1, row, row + 1);
    row++;

    audio_device_entry =
	add_number_entry_with_label(audio_device,
			   "Audio device try (/dev/dsp for OSS) (default, hw:0,0 or hw:1,0 ... for ALSA)", dialog_table, row++);


    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	stop_key_highlight_interval =
	    atof(gtk_entry_get_text((GtkEntry *) stop_interval_entry));
	song_key_highlight_interval =
	    atof(gtk_entry_get_text((GtkEntry *) song_interval_entry));
	song_mark_silence =
	    atof(gtk_entry_get_text((GtkEntry *) silence_entry));
	denoise_normalize =
	    atoi(gtk_entry_get_text((GtkEntry *) normalize_entry));
	sonogram_log =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (sonogram_log_entry));
	strcpy(audio_device, 
	    gtk_entry_get_text(((GtkEntry *) audio_device_entry)));

	save_preferences();

	main_redraw(FALSE, TRUE);
    }

    gtk_widget_destroy(dlg) ;
}

void declick_set_preferences(GtkWidget * widget, gpointer data)
{
    extern double weak_declick_sensitivity;
    extern double strong_declick_sensitivity;
    extern double weak_fft_declick_sensitivity;
    extern double strong_fft_declick_sensitivity;
    extern int declick_iterate_flag ;
    extern int declick_detector_type ;
    GtkWidget *dlg;
    GtkWidget *dc_weak_entry;
    GtkWidget *dc_strong_entry;
    GtkWidget *iterate_widget;
    GtkWidget *dc_fft_weak_entry;
    GtkWidget *dc_fft_strong_entry;
    GtkWidget *method_widget;
    GtkWidget *dialog_table;
    int dres;
    int row = 1 ;

    dlg =
	gtk_dialog_new_with_buttons("Declicking preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);

    dialog_table = gtk_table_new(5, 2, 0);

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);

    gtk_widget_show(dialog_table);

    dc_weak_entry =
	add_number_entry_with_label_double(weak_declick_sensitivity,
					   "Weak Declick Sensitivity (default = 0.45) ",
					   dialog_table, row++);
    dc_strong_entry =
	add_number_entry_with_label_double(strong_declick_sensitivity,
					   "Strong Declick Sensitivity (default = 0.35) ",
					   dialog_table, row++);

    dc_fft_weak_entry =
	add_number_entry_with_label_double(weak_fft_declick_sensitivity,
					   "FFT Weak Declick Sensitivity (default = 3.0) ",
					   dialog_table, row++);
    dc_fft_strong_entry =
	add_number_entry_with_label_double(strong_fft_declick_sensitivity,
					   "FFT Strong Declick Sensitivity (default = 5.0) ",
					   dialog_table, row++);

    method_widget = gtk_check_button_new_with_label ("Use FFT click detector");
    gtk_toggle_button_set_active((GtkToggleButton *) method_widget, declick_detector_type == FFT_DETECT ? TRUE : FALSE);
    gtk_widget_show(method_widget);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), method_widget, 0, 2, row, row+1);
    row += 2 ;

    iterate_widget = gtk_check_button_new_with_label ("Iterate in repair clicks until all repaired");
    gtk_toggle_button_set_active((GtkToggleButton *) iterate_widget, declick_iterate_flag == 1 ? TRUE : FALSE);
    gtk_widget_show(iterate_widget);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), iterate_widget, 0, 2, row, row+1);
    row += 2 ;


    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	weak_declick_sensitivity = atof(gtk_entry_get_text((GtkEntry *) dc_weak_entry));
	strong_declick_sensitivity = atof(gtk_entry_get_text((GtkEntry *) dc_strong_entry));
	weak_fft_declick_sensitivity = atof(gtk_entry_get_text((GtkEntry *) dc_fft_weak_entry));
	strong_fft_declick_sensitivity = atof(gtk_entry_get_text((GtkEntry *) dc_fft_strong_entry));
	declick_iterate_flag = gtk_toggle_button_get_active((GtkToggleButton *) iterate_widget) ==
	    TRUE ? 1 : 0;
	declick_detector_type = gtk_toggle_button_get_active((GtkToggleButton *) method_widget) ==
	    TRUE ? FFT_DETECT : HPF_DETECT ;
    }

    gtk_widget_destroy(dlg) ;
}

void decrackle_set_preferences(GtkWidget * widget, gpointer data)
{
    extern double decrackle_level;
    extern gint decrackle_window, decrackle_average;
    GtkWidget *dlg;
    GtkWidget *dcr_entry, *dcw_entry, *dca_entry;
    GtkWidget *dialog_table;

    int dres;

    dlg =
	gtk_dialog_new_with_buttons("Decrackling preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);

    dialog_table = gtk_table_new(3, 2, 0);

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);

    gtk_widget_show(dialog_table);

    dcr_entry =
	add_number_entry_with_label_double(decrackle_level,
					   "Decrackle level (default = 0.2) ",
					   dialog_table, 1);
    dcw_entry =
	add_number_entry_with_label_int(decrackle_window,
					"Decrackling window (default = 2000)",
					dialog_table, 2);
    dca_entry =
	add_number_entry_with_label_int(decrackle_average,
					"Decrackling average window (default = 3 [7])",
					dialog_table, 3);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	decrackle_level =
	    atof(gtk_entry_get_text((GtkEntry *) dcr_entry));
	decrackle_window =
	    atoi(gtk_entry_get_text((GtkEntry *) dcw_entry));
	decrackle_average =
	    atoi(gtk_entry_get_text((GtkEntry *) dca_entry));
    }

    gtk_widget_destroy(dlg);
}


extern struct denoise_prefs denoise_prefs;
static int noise_suppression_method, window_type;

void load_denoise_preferences(void)
{
    denoise_prefs.noise_suppression_method = 3 ;
    denoise_prefs.window_type = 2 ;
    denoise_prefs.smoothness = 11;
    denoise_prefs.FFT_SIZE = 4096 ;
    denoise_prefs.n_noise_samples = 16 ;
    denoise_prefs.amount = 0.22 ;
    denoise_prefs.dn_gamma = 0.8 ;
    denoise_prefs.randomness = 0.0 ;
    denoise_prefs.min_sample_freq = 0.0 ;
    denoise_prefs.max_sample_freq = 44100.0 ;
    denoise_prefs.freq_filter = 0 ;
    denoise_prefs.estimate_power_floor = 0 ;

    GKeyFile  *key_file = read_config();

    if (g_key_file_has_group(key_file, "denoise_params") == TRUE) {
        denoise_prefs.n_noise_samples = g_key_file_get_integer(key_file, "denoise_params", "n_noise_samples", NULL);
        denoise_prefs.smoothness = g_key_file_get_integer(key_file, "denoise_params", "smoothness", NULL);
        denoise_prefs.FFT_SIZE = g_key_file_get_integer(key_file, "denoise_params", "FFT_SIZE", NULL);
        denoise_prefs.amount = g_key_file_get_double(key_file, "denoise_params", "amount", NULL);
        denoise_prefs.dn_gamma = g_key_file_get_double(key_file, "denoise_params", "dn_gamma", NULL);
        denoise_prefs.randomness = g_key_file_get_double(key_file, "denoise_params", "randomness", NULL);
        denoise_prefs.window_type = g_key_file_get_integer(key_file, "denoise_params", "window_type", NULL);
        denoise_prefs.freq_filter = g_key_file_get_integer(key_file, "denoise_params", "freq_filter", NULL);
        denoise_prefs.estimate_power_floor = g_key_file_get_integer(key_file, "denoise_params", "estimate_power_floor", NULL);
        denoise_prefs.min_sample_freq = g_key_file_get_double(key_file, "denoise_params", "min_sample_freq", NULL);
        denoise_prefs.max_sample_freq = g_key_file_get_double(key_file, "denoise_params", "max_sample_freq", NULL);
        denoise_prefs.noise_suppression_method = g_key_file_get_integer(key_file, "denoise_params", "noise_suppression_method", NULL);
    }

    g_key_file_free (key_file);
}

void save_denoise_preferences(void)
{
    GKeyFile  *key_file = read_config();

    g_key_file_set_integer(key_file, "denoise_params", "n_noise_samples", denoise_prefs.n_noise_samples);
    g_key_file_set_integer(key_file, "denoise_params", "smoothness", denoise_prefs.smoothness);
    g_key_file_set_integer(key_file, "denoise_params", "FFT_SIZE", denoise_prefs.FFT_SIZE);
    g_key_file_set_double(key_file, "denoise_params", "amount", denoise_prefs.amount);
    g_key_file_set_double(key_file, "denoise_params", "dn_gamma", denoise_prefs.dn_gamma);
    g_key_file_set_double(key_file, "denoise_params", "randomness", denoise_prefs.randomness);
    g_key_file_set_integer(key_file, "denoise_params", "window_type", denoise_prefs.window_type);
    g_key_file_set_integer(key_file, "denoise_params", "freq_filter", denoise_prefs.freq_filter);
    g_key_file_set_integer(key_file, "denoise_params", "estimate_power_floor", denoise_prefs.estimate_power_floor);
    g_key_file_set_double(key_file, "denoise_params", "min_sample_freq", denoise_prefs.min_sample_freq);
    g_key_file_set_double(key_file, "denoise_params", "max_sample_freq", denoise_prefs.max_sample_freq);
    g_key_file_set_integer(key_file, "denoise_params", "noise_suppression_method", denoise_prefs.noise_suppression_method);

    write_config(key_file);
}


void fft_window_select(GtkWidget * clist, gint row, gint column,
		       GdkEventButton * event, gpointer data)
{
    if (row == 0)
	window_type = DENOISE_WINDOW_BLACKMAN;
    if (row == 1)
	window_type = DENOISE_WINDOW_BLACKMAN_HYBRID;
    if (row == 2)
	window_type = DENOISE_WINDOW_HANNING_OVERLAP_ADD;
#ifdef DENOISE_TRY_ONE_SAMPLE
    if (row == 3)
	window_type = DENOISE_WINDOW_ONE_SAMPLE;
    if (row == 4)
	window_type = DENOISE_WINDOW_WELTY;
#else
    if (row == 3)
	window_type = DENOISE_WINDOW_WELTY;
#endif
}

void noise_method_window_select(GtkWidget * clist, gint row, gint column,
				GdkEventButton * event, gpointer data)
{
    if (row == 0)
	noise_suppression_method = DENOISE_WEINER;
    if (row == 1)
	noise_suppression_method = DENOISE_POWER_SPECTRAL_SUBTRACT;
    if (row == 2)
	noise_suppression_method = DENOISE_EM;
    if (row == 3)
	noise_suppression_method = DENOISE_LORBER;
    if (row == 4)
	noise_suppression_method = DENOISE_WOLFE_GODSILL;
    if (row == 5)
	noise_suppression_method = DENOISE_EXPERIMENTAL ;
}

void denoise_set_preferences(GtkWidget * widget, gpointer data)
{
    GtkWidget *dlg;
    GtkWidget *fft_size_entry;
    GtkWidget *amount_entry;
    GtkWidget *gamma_entry;
    GtkWidget *smoothness_entry;
    GtkWidget *n_noise_entry;
    GtkWidget *dialog_table;
    GtkWidget *freq_filter_entry;
    GtkWidget *estimate_power_floor_entry;
    GtkWidget *min_sample_freq_entry;
    GtkWidget *max_sample_freq_entry;

    int dres;

    GtkWidget *fft_window_list;

    gchar *fft_window_titles[] = { "Windowing Function" };
#ifdef DENOISE_TRY_ONE_SAMPLE
    gchar *fft_window_parms[4][1] = { {"Blackman"},
    {"Hybrid Blackman-Full Pass"},
    {"Hanning-overlap-add (Best)"},
    {"Hanning-one-sample-shift (Experimental)"}
#else
    gchar *fft_window_parms[3][1] = { {"Blackman"},
    {"Hybrid Blackman-Full Pass"},
    {"Hanning-overlap-add (Best)"}
#endif
    };

    GtkWidget *noise_method_window_list;

    gchar *noise_method_window_titles[] = { "Noise Suppresion Method" };
    gchar *noise_method_window_parms[6][1] = {
						{"Weiner"},
						{"Power Spectral Subtraction"},
						{"Ephraim-Malah 1984"},
						{"Lorber & Hoeldrich (Best)"},
						{"Wolfe & Godsill (Experimental)"},
						{"Extremely Experimental"}
					    };

    load_denoise_preferences();


    fft_window_list = gtk_clist_new_with_titles(1, fft_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(fft_window_list),
				 GTK_SELECTION_SINGLE);
    gtk_clist_append(GTK_CLIST(fft_window_list), fft_window_parms[0]);
    gtk_clist_append(GTK_CLIST(fft_window_list), fft_window_parms[1]);
    gtk_clist_append(GTK_CLIST(fft_window_list), fft_window_parms[2]);
#ifdef DENOISE_TRY_ONE_SAMPLE
    gtk_clist_append(GTK_CLIST(fft_window_list), fft_window_parms[3]);
#endif
    gtk_clist_select_row(GTK_CLIST(fft_window_list),
			 denoise_prefs.window_type, 0);
    gtk_signal_connect(GTK_OBJECT(fft_window_list), "select_row",
		       GTK_SIGNAL_FUNC(fft_window_select), NULL);
    window_type = denoise_prefs.window_type;
    gtk_widget_show(fft_window_list);

    noise_method_window_list =
	gtk_clist_new_with_titles(1, noise_method_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(noise_method_window_list),
				 GTK_SELECTION_SINGLE);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[0]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[1]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[2]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[3]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[4]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[5]);
    gtk_clist_select_row(GTK_CLIST(noise_method_window_list),
			 denoise_prefs.noise_suppression_method, 0);
    gtk_signal_connect(GTK_OBJECT(noise_method_window_list), "select_row",
		       GTK_SIGNAL_FUNC(noise_method_window_select), NULL);
    noise_suppression_method = denoise_prefs.noise_suppression_method;
    gtk_widget_show(noise_method_window_list);

    dlg =
	gtk_dialog_new_with_buttons("Denoise",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);

    dialog_table = gtk_table_new(9, 2, 0);

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);

    gtk_widget_show(dialog_table);

    fft_size_entry =
	add_number_entry_with_label_int(denoise_prefs.FFT_SIZE,
					"FFT_SIZE (4096 for 44.1khz sample rate)", dialog_table, 0);
    amount_entry =
	add_number_entry_with_label_double(denoise_prefs.amount,
					   "Reduction (0.0-1.0)",
					   dialog_table, 1);
    smoothness_entry =
	add_number_entry_with_label_int(denoise_prefs.smoothness,
					"(Smoothness for Blackman window (2-11)",
					dialog_table, 2);
    n_noise_entry =
	add_number_entry_with_label_int(denoise_prefs.n_noise_samples,
					"# noise samples (2-16)",
					dialog_table, 3);

    gamma_entry =
	add_number_entry_with_label_double(denoise_prefs.dn_gamma,
					   "gamma -- for Lorber & Hoelrich or Ephraim-Malah , (0.7-1, try 0.8)",
					   dialog_table, 4);

    freq_filter_entry =
	add_number_entry_with_label_int(denoise_prefs.freq_filter,
					"Apply freq filter (0,1)", dialog_table, 5);

    estimate_power_floor_entry =
	add_number_entry_with_label_int(denoise_prefs.estimate_power_floor,
					"Estimate power floor (0,1)", dialog_table, 6);

    min_sample_freq_entry =
	add_number_entry_with_label_int(denoise_prefs.min_sample_freq,
					"Minimum frequency to use in noise sample (Hz)", dialog_table, 7);

    max_sample_freq_entry =
	add_number_entry_with_label_int(denoise_prefs.max_sample_freq,
					"Maximum frequency to use in noise sample (Hz)", dialog_table, 8);


    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), fft_window_list,
		       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),
		       noise_method_window_list, TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	int i;
	i = atoi(gtk_entry_get_text((GtkEntry *) fft_size_entry));
	for (denoise_prefs.FFT_SIZE = 8;
	     denoise_prefs.FFT_SIZE < i
	     && denoise_prefs.FFT_SIZE < DENOISE_MAX_FFT;
	     denoise_prefs.FFT_SIZE *= 2);
	denoise_prefs.amount =
	    atof(gtk_entry_get_text((GtkEntry *) amount_entry));
	denoise_prefs.smoothness =
	    atoi(gtk_entry_get_text((GtkEntry *) smoothness_entry));
	denoise_prefs.n_noise_samples =
	    atoi(gtk_entry_get_text((GtkEntry *) n_noise_entry));
	denoise_prefs.dn_gamma =
	    atof(gtk_entry_get_text((GtkEntry *) gamma_entry));
	denoise_prefs.noise_suppression_method =
	    noise_suppression_method;
	denoise_prefs.window_type = window_type;
	denoise_prefs.freq_filter = atoi(gtk_entry_get_text((GtkEntry *) freq_filter_entry));
	denoise_prefs.estimate_power_floor = atoi(gtk_entry_get_text((GtkEntry *) estimate_power_floor_entry));
	denoise_prefs.min_sample_freq = atof(gtk_entry_get_text((GtkEntry *) min_sample_freq_entry));
	denoise_prefs.max_sample_freq = atof(gtk_entry_get_text((GtkEntry *) max_sample_freq_entry));
	save_denoise_preferences();
    }

    gtk_widget_destroy(dlg) ;
}
