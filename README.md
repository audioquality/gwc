# Gnome Wave Cleaner

![](doc/gwc.png)

Denoise & Declick audio files. Clean up digitalized vinyl and tape recordings, remove common distortion while preserving the musical information. The input is expected to be 16bit 44.1kHz stereo WAV file format.

## About

Original GWC Homepage: [http://gwc.sourceforge.net]()

The GWC was started in 2002 by Jeff Welty. The latest original version is [0.21.19 released in 2013-04-05](https://sourceforge.net/projects/gwc). The author responds to open tickets on sourceforge, but there was no significant development activity for a decade. Most of the used GTK2 API is marked as deprecated. This fork is based on the GWC 0.21.19 code with included Debian repository fixes.

The original GWC attempts to do everything: audio recording, tag editing, format conversion, CD-R exports... so a lot of development time was wasted on non-essential features. Today, there are many new programs doing it all better, but the GWC still has the best code for cleaning vinyl audio.

This fork throws the non-essential features overboard and improves the usability instead. I changed what I was missing while cleaning more than 70 albums:

## Changes


**The old version 0.21.19** had some interface issues that have been addressed:

  * playback position behavior - this simplistic impementation made it very hard to identify/clean clicks by ear:
    - if audio was selected, playback always started at the beginning of a selection
    - if no audio was selected, playback started at the beginning of the file (!?)
    - no playback autoscroll
    - scrolling possible only by dragging the scrollbar
  * stereo playback, even if single channel was selected - this made it hard to identify clicks present only in a single channel
  * very few keyboard shortcuts
  * forgets the window size between restarts

**New in this (master branch) version**:

  * new playback position behavior: 
    * continue playback from where it stopped
    * if no audio is selected and the playback cursor is outside of the view, playback starts at the beginning of current view
    * autoscroll the wave during playback (if not zoomed in too much)
    * start playback at any position with mouse-rightclick
  * if only one channel is selected, it is played as mono to both L and R outputs to help identify single channel distortions
  * many new keyboard shortcuts, including wave scrolling using arrows
  * remember the main window size between restarts
  * new open/save GTK dialogs
  * cleaned code, no compilation warnings
  * Removed cluttered, non-essential features:
    * ledbar - the way it was implemented it was more confusing than useful
    * batch mode - this was a bad idea, always creating inferior output
    * cdrdao export - if you really need to burn an audio CD, see [Brasero](https://wiki.gnome.org/Apps/Brasero)
    * lossy compression support - if you really need to export or edit different formats, use dedicated tools:
      - Audio Editor: [Audacity](http://www.audacityteam.org)
      - Metadata Editor: [EasyTAG](https://wiki.gnome.org/Apps/EasyTAG) or [Kid3](https://kid3.sourceforge.io)
      - Format Converter: [Sound Converter](http://soundconverter.org)

**v0.30 branch**

This branch contains a few fixes and improvements from the master branch, but also the old non-essential code, like the confusing ledbar, inferior batch functionality, and lossy compression support. Check out this branch only if you know what you are doing.

## Installation

This is a Linux application, compiled and tested on Debian Linux 9 stretch. It should compile on any other distribution, the dependencies have to be preinstalled accordingly.

Install dependencies (Debian):

    apt-get install libc6-dev libfftw3-dev libsndfile1-dev libpulse-dev
    
Download, Compile & Install:

    git clone https://github.com/audioquality/gwc.git
    cd gwc/src
    ./configure
    make
    make install

The program shortcut is in _Menu->Multimedia->Gnome Wave Cleaner_ , or run in terminal:

    gwc

If the menu icons are missing (disabled by default for GTK2 apps), they can be enabled by:

    gconftool-2 --type bool --set /desktop/gnome/interface/menus_have_icons true


## Usage

**WARNING: GWC commits all changes to the original file instantly!** Undo saves the deltas needed to get back to the original, so on exit all your changes are saved. GWC will notify you that changes have been made, and give you the chance to exit (or open a new file) without making the changes permanent.

### Keyboard functions

| _Function_                 | _Keyboard Shortcut_ |
|----------------------------|:--------------------|
|Play/Stop playback          |**Space**|
|Play selection in a loop    |**Shift + Space**, **Ctrl + Space**|
|Go foward / backward by half revolution of 33 rpm record|**Right / Left**|
|Go foward / backward by 2 revolutions of 33 rpm record|**Ctrl + Right / Left**|
|Go foward / backward by 4 revolutions of 33 rpm record|**Shift + Right / Left**|
|Select last S seconds of audio|**S**|
|Edit -> Undo                |**Bksp**|
|Edit -> Declick Strong      |**U**|
|Edit -> Declick Weak        |**I**|
|Edit -> Declick Manual      |**O**|
|Edit -> Decrackle           |**C**|
|Edit -> Sample              |**H**|
|Edit -> Denoise             |**J**|
|Edit -> Estimate Signal     |**K**|
|Deselect All                |**Esc**|
|View -> Select View         |__*__|
|View -> Zoom In             |**Up** , **+**|
|View -> Zoom Out            |**Down** , **-**|
|View -> Zoom to All         |**\\**|
|View -> Zoom to Selected    |**/**|
|Markers -> Toggle beginning marker|**N**|
|Markers -> Toggle end marker|**M**|
|Markers -> Clear markers    |**R**|
|Markers -> Expand selection to nearest marker|**E**|
|Select Markers              |**W**|
|Settings -> Declick         |**P**|
|Settings -> Denoise         |**L**|
|Settings -> Decrackle       |**;**|
|Settings -> Preferences     |**Ctrl + P**|
|Zoom in Y scale             |**F**|
|Zoom out Y scale            |**D**|
|Reset Y scale               |**G**|
|Amplify sonogram            |**B**|
|Attenuate sonogram          |**V**|

### Mouse functions

  * **Left Click & Drag**: Select audio section
  * **Right Click**: Start playback at that position
  * scrolling is currently not implemented, use keyboard arrows for navigation

### Menu

  - **File**
    - **Open** - Opens an audio file for editing (16bit 44.1 kHz PCM WAV)
    - **Save Selection As** - Saves the highlighted selection (or current view) as a new file
    - **Quit** - guess what!
  - **Edit**
    - **Undo** - Undo the previous edit
    - **Apply DSP Frequency Filters** - Applies High Pass, Low Pass, Notch or Band Pass DSP iir filters (These are not well documented)
    - **Generate Pink Noise** - If you do not have a good noise sample, this will overwrite the selected region	with pink and or white noise, which is a good approximation for hiss. You can	then use this for your noise sample.
    - **Amplify** - Amplify (or attenuate) the highlighted selection (or current view)
    - **Declick Strong** - Remove strong clicks (pops, scratches) the highlighted selection (or current view), note this automatically attempts to detect individual clicks
    - **Declick Weak** - Remove weak clicks the highlighted selection (or current view), note this automatically attempts to detect individual clicks
    - **Declick Manual** - Apply declick algoritm to the highlighted selection (or current view). This assumes there is only _one_ click, and it is defined by the	highlighted selection or current view.  Use this for cases where there are very	few clicks in the audio file, or a click escapes the automatic detection but you can still hear it.
    - **Decrackle** - Decrackle the highlighted selection (or current view) -- use this for recordings from	very dirty or damaged vinyl which may have thousands of small "clicks" per second. This will also attenuate the high frequencies somewhat.
    - **Estimate** - When you have a large section of audio (like a dropout, or needle bounce on a scratchy or	uneven vinyl), usually greater than about 500 samples, then try this. It estimates the audio that might have been in that section.  This is _not_ a very sophisticated algorithm, and often does not improve the audio. But sometimes it does. Try it and audition the result, and **undo** if you don't like the result. You may try adjusting the start or end of the selection	to see if that can improve the result.
    - **Sample** - Identifies the "noise only" part of the audio file. The denoise algorithm will take noise sub-samples	from this highlighted selection (or current view).
    - **Denoise** - Applies the denoising algorithm to the highlighted selection (or current view), see _Settings->Denoise_ for options.
    - **Cut** - Identifies either the start or end of the audio file to be truncated. The truncation will occur when you exit	GWC or open another audio file.
    - **Reverb** - Uses the [TAP Reverb](http://tap-plugins.sourceforge.net/reverbed.html) software to apply reverberation to the highlighted selection or current view. It can add a little "presence" back into the audio if the denoising seemed to take it out. For this application, I recommend your wet levels be very small indeed, try -30 Db for wet, -1 Db for Dry, and 1500 ms with the "Ambience (Thick) - HD" Reverb setting. You can of course apply more reverb if you desire it.
  - **View**
    - **Zoom Select** - Expand the highlighted selection so it fills the entire window
    - **Zoom In** - Zoom in by a factor of 1.5. The audio stays centered in the window
    - **Zoom Out** - Zoom out by a factor of 1.5. The audio stays centered in the window
    - **View All** - Display the entire audio file in the window
    - **Select View** - Select current view. To select the whole audio file, do _View All_ & _Select View_.
    - **Spectral View** -	Displays a sonogram of the current window. This can be especially useful for locating individual clicks	or pops, which show themselves as bright, vertical lines. 
  - **Markers** - There are 2 kinds of markers in GWC. Editing markers, which precisely identify a location in the audio, and the highlighting function will "snap" to a marker which is less than 10 pixels from the cursor. The second type of marker is the song marker.
    - **Toggle Beginning Marker** - sets or unsets an edit marker at the beginning of the highlighted selection (or current view)
    - **Toggle Ending Marker** - Sets or unsets an edit marker at the ending of the highlighted selection (or current view)
    - **Clear Markers** - Clears all edit markers
    - **Mark Songs** - Automatically place markers between songs. This uses the quiet sections between songs to place the markers
    - **Move Song Marker** - The song marker closest to the start of the highlighted selection is moved to the start of the highlighted selection
    - **Add Song Marker** - A song marker is added at the start of the highlighted selection
    - **Delete Song Marker** - Song markers within the highlighted selection are deleted
    - **Next Song Marker** - Highlights a small region around the next song marker, wraps from last to first at end of audio file.
  - **Settings**
    - **Declick**
      + **Weak Declick Sensitivity** - Set this to something greater than 1 (1.5 is the largest you should try) to detect even weaker clicks (but you will get more "false positives"), set it lower to detect a little bit stronger clicks.
      + **Strong Declick Sensitivity** - This takes the value 0.75 by default.  Lower values will only detect stronger clicks, higher values will detect weaker clicks.
      + **FFT Weak Declick Sensitivity** - Set this to something greater than 2 to detect even weaker clicks (but you will get more "false positives"), set it higher to detect a little bit stronger clicks.
      + **FFT Strong Declick Sensitivity** - This takes the value 5 by default. Lower values will only detect stronger clicks, higher values will detect weaker clicks.
      + **Use FFT click detector** - Check this box to use FFT click detection. It appears to generate fewer false positives and fewer false negatives, based on limited testing.
      + **Iterate in repair clicks until all repaired** - The click detection algorithm compares each candidate click against the overall sound profile in the general region of the candidate click. If the click "looks" a lot like the other sound energy (for example a cymbal crash contains a lot of the same frequencies as a click), then the candidate is ignored. But, sometimes a stronger click may mask out a weaker click because of this comparison process. So, this option causes the click detector to run on a region of the audio after any click is repaired, to re-check for more possible clicks in the absence of those clicks that were repaired. <em>The iterate function does not yet work for FFT click detection/removal</em>.
    - **Decrackle** - not documented
    - **Denoise**
      + **FFT SIZE** - You have to think about this standing on your head.  (Took me a while at least). Since the samples are say, 1/44100 of a second apart, the highest sample time detectable by the FFT will _always_ be 1/22050 of a second, regardless of the fft window size. A larger and larger fft window size allows you to detect lower and lower frequencies, and to get more of an "averaging" effect of the higher frequencies which are present in the fft window.
      + **Reduction** - The amount of noise you want removed from the audio, as a proportion. If you use 1.0, then all noise is removed, but unfortunately you get nasty artifacts in the resulting audio, it may get burbly or metallic sounding. In general, you should set this value as low as possible (0.2 - 0.3), and test a few spots in the audio file (remembering to "undo" each test!!!).
      + **Smoothness** - For the Blackman window, it determines how the windowing function "steps along" the audio file. In general, this should be set between 5 and 11, usually just leave this at 11
      + **# noise samples** - The denoise algorithm sub-samples the audio data you marked for "sampling".  Because noise varies from one millisecond to the next, gwc will take many sub-samples and take the "average" noise signature from those sub-samples. You should usually just leave this at 16. Don't worry if the (number of noise samples)*FFT_SIZE is greater than the sample area, gwc will just overlap the sub-samples.
      + **gamma** - The Ephraim-Malah and Lorber-Hoeldrich noise reduction algorithms attempt to reduce the metallic sounding artifact in denoising by holding the noise removal process very constant from one window frame to the next. Gamma is the parameter that controls this, if you set it to 0, you essentially get Weiner noise reduction on every frame, set it close to 1, and you get some weird sounding stuff. Setting it in the range of 0.5 to 0.95 usually is pretty good. Play around with this and the reduction amount if you have that metallic sound after denoising.
      + **Windowing Function**
	      - _Blackman_: Try this second
	      - _Hybrid Blackman-Full Pass_: Fastest, may work well for relatively "clean" audio files with only a little hiss
	      - _Hanning-overlap-add_: Usually this is the one you want (Default)
      + **Noise Suppression Method**
        - _Weiner_: A little better than Power Spectral Subtraction
        - _Power Spectral Subtraction_: Crudest
        - _Ephram-Malah 1984_: Pretty darn good
        - _Lorber & Hoeldrich_: Large improvement over Ephram-Malah (Default)
    - **Preferences**
      + **Seconds of audio preselected when "S" key is struck** - Stop playback and select audio from "current_position - S_seconds"  to "current_position"
      +	**Normalize values for declick, denoise?** - The normalize option asks libsndfile to normalize the audio data on the interval -1.0 to 1.0, it appears to make no difference, and it really only there for testing. Set to 1 for normalization, 0 for non-normalized data. 
      +	**Silence estimate in seconds for marking songs** - Used for _Markers->Mark Songs_
      +	**Log frequency in sonogram** - Logarithmic frequency scale
      +	**Audio Device** - Output Device. ALSA example: _hw:1,0_
            NOTE: I only tested the ALSA output mode. The OSS/Pulseaudio compatibility might be affected, use at your own risk.


## ToDo

This codebase seems a little clunky from the UI perspective. Considering that it is more than a decade old, it makes no sense to build more advanced features on top of this code. A good idea might be a complete code rewrite, while keeping the original audio restoration algorithms. The GWC is still missing:

 * multithreaded audio processing
 * lossless audio format support (FLAC)
 * better playback waveform visualisation
 * 24 bit audio support
 * clipping restoration function (not vinyl, but a common digital audio problem)

Currently the playback buffering can fail, if another CPU intensive task is running. Even during GWC waveform scrolling and main window resizing! This is very buggy behavior and cannot be changed within the current implementation :(


## License

This program is released under the GNU GPL, see the COPYING file.
