//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Sat May  9 18:57:48 PDT 2009
// Last Modified: Sat May  9 19:48:31 PDT 2009
// Filename:      alsarawmidiout.c
// Syntax:        C; ALSA 1.0
// $Smake:        gcc -o %b %f -lasound
//
// Description:   Send a MIDI note to a synthesizer using the ALSA rawmidi
//                interface.  Reverse engineered from amidi.c (found in
//                ALSA utils 1.0.19 program set).
//
// First double-check that you have the ALSA system installed on your computer
// by running this command-line command:
//    cat /proc/asound/version
// Which should return something like:
//   Advanced Linux Sound Architecture Driver Version 1.0.17.
// This example program should work if the version number (1.0.17 in this
// case) is "1".
//
// Online documentation notes:
//
// http://www.alsa-project.org/alsa-doc/alsa-lib/rawmidi.html
//
// Using SND_RAWMIDI_NONBLOCK flag for snd_rawmidi_open() or
// snd_rawmidi_open_lconf() instruct device driver to return the -EBUSY
// error when device is already occupied with another application. This
// flag also changes behaviour of snd_rawmidi_write() and snd_rawmidi_read()
// returning -EAGAIN when no more bytes can be processed.
//
// Using SND_RAWMIDI_APPEND flag (output only) instruct device driver to
// append contents of written buffer - passed by snd_rawmidi_write() -
// atomically to output ring buffer in the kernel space. This flag also
// means that the device is not opened exclusively, so more applications can
// share given rawmidi device. Note that applications must send the whole
// MIDI message including the running status, because another writting
// application might break the MIDI message in the output buffer.
//
// Using SND_RAWMIDI_SYNC flag (output only) assures that the contents of
// output buffer specified using snd_rawmidi_write() is always drained before
// the function exits. This behaviour is the same as snd_rawmidi_write()
// followed immediately by snd_rawmidi_drain().
//
// http://www.alsa-project.org/alsa-doc/alsa-lib/group___raw_midi.html
//
// int snd_rawmidi_open(snd_rawmidi_t** input, snd_rawmidi_t output,
//                                             const char* name, int mode)
//    intput   == returned input handle (NULL if not wanted)
//    output   == returned output handle (NULL if not wanted)
//    name     == ASCII identifier of the rawmidi handle, such as "hw:1,0,0"
//    mode     == open mode (see mode descriptions above):
//                SND_RAWMIDI_NONBLOCK, SND_RAWMIDI_APPEND, SND_RAWMIDI_SYNC
//
// int snd_rawmidi_close(snd_rawmidi_t* rawmidi)
//    Close a deviced opended by snd_rawmidi_open().  Returns an negative
//    error code if there was an error closing the device.
//
// int snd_rawmidi_write(snd_rawmidi_t* output, char* data, int datasize)
//    output   == midi output pointer setup with snd_rawmidi_open().
//    data     == array of bytes to send.
//    datasize == number of bytes in the data array to write to MIDI output.
//
// const char* snd_strerror(int errornum)
//    errornum == error number returned by an ALSA snd__* function.
//    Returns a string explaining the error number.
//

#include <alsa/asoundlib.h>     /* Interface to the ALSA system */
#include <unistd.h>             /* for sleep() function */

// function declarations:
void errormessage(const char *format, ...);

///////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
   int status;
   int mode = SND_RAWMIDI_SYNC;
   snd_rawmidi_t* midiout = NULL;
   const char* portname = "hw:1,0,0";  // see alsarawportlist.c example program
   if ((argc > 1) && (strncmp("hw:", argv[1], 3) == 0)) {
      portname = argv[1];
   }
   if ((status = snd_rawmidi_open(NULL, &midiout, portname, mode)) < 0) {
      errormessage("Problem opening MIDI output: %s", snd_strerror(status));
      exit(1);
   }

   char noteon[3]  = {0x90, 60, 100};
   char noteoff[3] = {0x90, 60, 0};

   if ((status = snd_rawmidi_write(midiout, noteon, 3)) < 0) {
      errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
      exit(1);
   }

   sleep(1);  // pause the program for one second to allow note to sound.

   if ((status = snd_rawmidi_write(midiout, noteoff, 3)) < 0) {
      errormessage("Problem writing to MIDI output: %s", snd_strerror(status));
      exit(1);
   }

   snd_rawmidi_close(midiout);
   midiout = NULL;    // snd_rawmidi_close() does not clear invalid pointer,
   return 0;          // so might be a good idea to erase it after closing.
}

///////////////////////////////////////////////////////////////////////////

//////////////////////////////
//
// error -- Print an error message.
//

void errormessage(const char *format, ...) {
   va_list ap;
   va_start(ap, format);
   vfprintf(stderr, format, ap);
   va_end(ap);
   putc('\n', stderr);
}

