/*******************************************************************************
*                                                                              *
*                           INTERNET ACCESS LIBRARY                            *
*                                                                              *
*                       Copyright (C) 2006 Scott A. Franco                     *
*                                                                              *
*                              5/06 S. A. Franco                               *
*                                                                              *
* Implements access to Internet functions, via tcp/ip. tcp/ip is implemented   *
* via the "file" paradigm. An address and port is used to create a file, then  *
* normal C read and write statements are used to access it.                    *
*                                                                              *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
*******************************************************************************/

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00

/* Windows/mingw definitions */
#include <stdio.h>
#include <windows.h>

/* Petit-Ami definitions */
#include <localdefs.h>
#include <network.h>

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); } while (0)

#define MAXFIL 100 /* maximum number of open files */

/* File tracking.
   Files can be passthrough to syslib, or can be associated with a window. If
   on a window, they can be output, or they can be input. In the case of
   input, the file has its own input queue, and will receive input from all
   windows that are attached to it. */
typedef struct {

    int      net;   /* it's a network file */
    int      inp;   /* it's the input side of a network file */
    filhdl   han;   /* handle to underlying I/O file */
    int      sock;  /* handle to network socket */
    sockaddr socka; /* socket address structure */
    filhdl   lnk;   /* link to other side of network pair */
    int      autoc; /* entry was automatically closed as pair */

} filrec, *filptr;

/* error codes */
typedef enum {

    ewskini, /* cannot initalize winsock */
    einvhan, /* invalid file handle */
    enetopn, /* cannot reset || rewrite network file */
    enetpos, /* cannot position network file */
    enetloc, /* cannot find location network file */
    enetlen, /* cannot find length network file */
    esckeof, /* End encountered on socket */
    efinuse, /* file already in use */
    enetwrt, /* Attempt to write to input side of network pair */
    enomem,  /* Out of memory */
    esystem  /* System consistency check */

} errcod;

filptr  opnfil[MAXFIL]; /* open files table */
filhdl  xltfil[MAXFIL]; /* window equivalence table */
int     fi;             /* index for files table */
int     r;              /* result */
wsadata wsd: wsadata;   /* Winsock data structure */

/* The double fault flag is set when exiting, so if we exit again, it
  is checked, then forces an immediate exit. This keeps faults from
  looping. */
int     dblflt; /* double fault flag */

/*******************************************************************************

Process network library error

Outputs an error message using the special syslib function, then halts.

*******************************************************************************/

static void netwrterr(const char* s)

{

    fprintf(stderr, "\nError: Network: %s\n", s);

    exit(1);

}

/*******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.
This needs to go to a dialog instead of the system error trap.

*******************************************************************************/

static void error(errcod e)

{

    switch (e) { /* error */

        case ewskini:  netwrterr('Cannot initialize winsock'); break;
        case einvhan:  netwrterr('Invalid file number'); break;
        case enetopn:  netwrterr('Cannot reset or rewrite network file');
                       break;
        case enetpos:  netwrterr('Cannot position network file'); break;
        case enetloc:  netwrterr('Cannot find location network file'); break;
        case enetlen:  netwrterr('Cannot find length network file'); break;
        case esckeof:  netwrterr('End encountered on socket'); break;
        case efinuse:  netwrterr('File already in use'); break;
        case enetwrt:
            netwrterr('Attempt to write to input side of network pair'); break;
        case enomem:   netwrterr('Out of memory'); break;
        case esystem:
            netwrterr('System consistency check, please contact vendor'); break;

    }

}

/*******************************************************************************

Handle Winsock error

Only called if the last error variable is set. The text string for the error
is output, and then the program halted.

*******************************************************************************/

static void wskerr(void)
{

    LPVOID lpMsgBuf;

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf,
                  0, NULL);
    fprintf(stderr, "\n*** Windows error: %s\n", lpMsgBuf);

    exit(1);

}

/*******************************************************************************

Get file entry

Allocates and initializes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

void getfet(filptr* fp)

{

    *fp = malloc(sizeof(filrec)); /* get file entry */
    if (!*fp) error(enomem); /* no memory error */
    (*fp)->net   = FALSE; /* set ! a network file */
    (*fp)->inp   = FALSE; /* set ! input side */
    (*fp)->han   = 0;     /* set handle clear */
    (*fp)->sock  = 0;     /* clear out socket entry */
    (*fp)->lnk   = 0;     /* set no link */
    (*fp)->autoc = FALSE; /* set no automatic close */

}

/*******************************************************************************

Make file entry

Indexes a present file entry or creates a new one. Looks for a free entry
in the files table, indicated by 0. If found, that is returned, otherwise
the file table is full.

Note that the "predefined" file slots are never allocated.

*******************************************************************************/

void makfil(filhdl* fn) /* file handle */

{

    int fi; /* index for files table */
    int ff; /* found file entry */

    /* find idle file slot (file with closed file entry) */
    ff = -1; /* clear found file */
    for (fi = 0; fi < MAXFIL; fi++) { /* search all file entries */

        if (opnfil[fi] < 0) /* found an unallocated slot */
            ff = fi; /* set found */
        else
            /* check if slot is allocated, but unoccupied */
            if (opnfil[fi]->han == 0 && !opnfil[fi]->net)
                ff = fi; /* set found */

    }
    if (ff < 0) error(einvhan); /* no empty file positions found */
    if (!opnfil[ff]) getfet(opnfil[ff]); /* get and initialize the file entry */
    fn = ff; /* set file id number */

}

/*******************************************************************************

Get logical file number from file

Gets the logical translated file number from a text file, and verifies it
is valid.

*******************************************************************************/

int txt2lfn(FILE* f)

{

    int fn;

    fn = xltfil[getlfn(f)]; /* get and translate lfn */
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    return (fn); /* return result */

}

/*******************************************************************************

Check file entry open

Checks if the given file handle corresponds to a file entry that is allocated,
and open.

*******************************************************************************/

void chkopn(int fn)

{

    if (!opnfil[fn]) error(einvhan); /* invalid handle */
    if (!opnfil[fn]->han && !opnfil[fn]->net) error(einvhan);

}

/*******************************************************************************

Open file for read

Opens the file by name, and returns the file handle. Opens for read and write
don't have any purpose for network files, because they are handled by opennet.
The other files are passed through. If an openread is attempted on a net file,
it is an error.

*******************************************************************************/

void fileopenread(int* fn, const string nm)

{

    makfil(*fn); /* find file slot */
    if (strcmp(nm, '_input_network') /* open regular */
      ss_old_openread(opnfil[fn]->han, nm, sav_openread)

}

/*******************************************************************************

Open file for write

Opens the file by name, && returns the file handle. Opens for read && write
don't have any purpose for network files, because they are handled by opennet.
The other files are passed through. If an openread is attempted on a net file,
it is an error.

*******************************************************************************/

void fileopenwrite(var fn: ss_filhdl; view nm: char*);

{

   makfil(fn); /* find file slot */
   if ! compp(nm, '_output_network') then /* open regular */
      ss_old_openwrite(opnfil[fn]->han, nm, sav_openwrite)

};

/*******************************************************************************

Close file

Closes the file. The file is closed at the system level, then we remove the
table entry for the file.

A file can be a normal file, the input side of a network connection, || the
output side of a network connection. If its normal, it is passed on to the
operating system. If it's the input || output side of a network connection,
then the network connection is broken, && both the input && output sides
are closed.

We institute "automatic close forgiveness". Because network files are allocated
in pairs, Paslib will attempt to close out its entire file entry table, and
thus redundantly close these pairs. We flag the automatically closed side
of a pair, which could be either side, && then forgive a call to close it.
Being flagged automatically closed will ! prevent the entry from being
reallocated on the next open, so this technique will only work for Paslib.
It could create problems for multitask programs.

*******************************************************************************/

void fileclose(fn: ss_filhdl);

var r:        int;   /* result */
    /* b:        boolean; */
    ifn, ofn: ss_filhdl; /* input && output side ids */
    acfn:     ss_filhdl; /* automatic close side */

/* close && clear file entry */

void clsfil(var fr: filrec);

{

   with fr do {

      net   = false; /* set ! network file */
      inp   = false; /* set ! input side */
      han   = 0;     /* clear out file table entry */
      sock  = 0;     /* clear out socket entry */
      lnk   = 0;     /* set no link */
      autoc = false; /* set no automatic close */

   }

};

{

   if (fn < 1) || (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   if ! opnfil[fn]->autoc then chkopn(fn); /* check its open */
   if opnfil[fn]->net then { /* it's a network file */

      acfn = opnfil[fn]->lnk; /* set which side will be automatically closed */
      ifn = fn; /* set input side */
      /* if we have the output side, link back to the input side */
      if ! opnfil[fn]->inp then ifn = opnfil[fn]->lnk;
      ofn = opnfil[ifn]->lnk; /* get output side id */
      with opnfil[ifn]^ do { /* close the connection on socket */

         /* b = disconnectex(sock, 0); */ /* disconnect socket */
         /* if ! b then wskerr; */ /* cannot disconnnect */
         r = closesocket(sock); /* close socket */
         if r <> 0 then wskerr /* cannot close socket */

      };
      clsfil(opnfil[ifn]^); /* close input side entry */
      clsfil(opnfil[ofn]^); /* close output side entry */
      opnfil[acfn]->autoc = true /* flag automatically closed side */

   } else if ! opnfil[fn]->autoc then { /* standard file */

      chkopn(fn); /* check valid handle */
      ss_old_close(opnfil[fn]->han, sav_close); /* close at lower level */
      clsfil(opnfil[fn]^) /* close entry */

   };
   opnfil[fn]->autoc = false; /* remove automatic close status */

};

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

*******************************************************************************/

void fileread(fn: ss_filhdl; var ba: bytarr);

var r: int; /* int result */

{

   if (fn < 1) || (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   if opnfil[fn]->net then { /* process network file */

      r = recv(opnfil[fn]->sock, ba, 0); /* receive network data */
      if r == 0 then error(esckeof); /* flag eof error */
      if r <> max(ba) then wskerr /* flag read error */

   } else {

      chkopn(fn); /* check valid handle */
      ss_old_read(opnfil[fn]->han, ba, sav_read) /* pass to lower level */

   }

};

/*******************************************************************************

Write file

Writes a byte buffer to the output file. If the file is normal, we pass it on.
If the file is a network file, we process a write on the associated socket.

*******************************************************************************/

void filewrite(fn: ss_filhdl; view ba: bytarr);

var r: int; /* int result */

{

   if (fn < 1) || (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   if opnfil[fn]->net then { /* process network file */

      if opnfil[fn]->inp then error(enetwrt); /* write to input side */
      /* transmit network data */
      r = s}(opnfil[opnfil[fn]->lnk]->sock, ba, 0);
      if r <> max(ba) then wskerr /* flag write error */

   } else { /* standard file */

      chkopn(fn); /* check valid handle */
      ss_old_write(opnfil[fn]->han, ba, sav_write) /* pass to lower level */

   }

};

/*******************************************************************************

Position file

Positions the given file. Normal files are passed through. Network files give
an error, since they have no specified location.

*******************************************************************************/

void fileposition(fn: ss_filhdl; p: int);

{

   if (fn < 1) || (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   chkopn(fn); /* check valid handle */
   if opnfil[fn]->net then error(enetpos) /* cannot position this file */
   else /* standard file */
      ss_old_position(opnfil[fn]->han, p, sav_position) /* pass to lower level */

};

/*******************************************************************************

Find location of file

Find the location of the given file. Normal files are passed through. Network
files give an error, since they have no specified location.

*******************************************************************************/

int filelocation(fn: ss_filhdl): int;

{

   if (fn < 1) || (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   chkopn(fn); /* check valid handle */
   if opnfil[fn]->net then error(enetpos) /* cannot position this file */
   else /* standard file */
      filelocation =
         ss_old_location(opnfil[fn]->han, sav_location) /* pass to lower level */

};

/*******************************************************************************

Find length of file

Find the length of the given file. Normal files are passed through. Network
files give an error, since they have no specified length.

*******************************************************************************/

int filelength(fn: ss_filhdl): int;

{

   if (fn < 1) || (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   chkopn(fn); /* check valid handle */
   if opnfil[fn]->net then error(enetpos) /* cannot position this file */
   else /* standard file */
      /* pass to lower level */
      filelength = ss_old_length(opnfil[fn]->han, sav_length)

};

/*******************************************************************************

Check eof of file

Returns the eof status on the file. Normal files are passed through. Network
files allways return false, because there is no specified eof signaling
method.

*******************************************************************************/

int fileeof(fn: ss_filhdl): boolean;

{

   if (fn < 1) || (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   chkopn(fn); /* check valid handle */
   if opnfil[fn]->net then fileeof = false /* network files never } */
   else /* standard file */
      fileeof = ss_old_eof(opnfil[fn]->han, sav_eof) /* pass to lower level */

};

/*******************************************************************************

Open network file

Opens a network file with the given address && port. The file access is
broken up into a read && write side, that work indep}ently, can can be
spread among different tasks. This is done because the Pascal file paradygm
does ! handle read/write on the same file well.

*******************************************************************************/

void opennet(var infile, outfile: text; addr: int; port: int);

var ifn, ofn: ss_filhdl; /* input && output file numbers */
    r:        int;   /* return value */
    i2b: record /* int to bytes converter */

       case boolean of

          false: (i: int);
          true:  (b: array 4 of byte);

       /* } */

    };

{

   /* open input side */
   if getlfn(infile) <> 0 then error(efinuse); /* file in use */
   assign(infile, '_input_network'); /* assign special filename */
   reset(infile); /* open file for reading */
   ifn = txt2lfn(infile); /* index that entry */
   opnfil[ifn]->net = true; /* set open network file */

   /* open output side */
   if getlfn(outfile) <> 0 then error(efinuse); /* file in use */
   assign(outfile, '_output_network'); /* assign special filename */
   rewrite(outfile); /* open file for writing */
   ofn = txt2lfn(outfile); /* index that entry */
   opnfil[ofn]->net = true; /* set open network file */

   /* cross link the entries */
   opnfil[ofn]->lnk = ifn; /* link output side to input side */
   opnfil[ifn]->lnk = ofn; /* link input side to output side */

   /* set up output side */
   opnfil[ofn]->inp = false; /* set is output side */

   /* Open socket && connect. We keep the parameters on the input side */
   with opnfil[ifn]^ do { /* in file context */

      inp = true; /* set its the input side */
      /* open socket as internet, stream */
      sock = socket(af_inet, sock_stream, 0);
      if sock < 0 then wskerr; /* process Winsock error */
      socka.sin_family = pf_inet;
      /* note, parameters specified in big }ian */
      socka.sin_port = port*0x100;
      i2b.i = addr; /* convert address to bytes */
      socka.sin_addr[0] = i2b.b[4];
      socka.sin_addr[1] = i2b.b[3];
      socka.sin_addr[2] = i2b.b[2];
      socka.sin_addr[3] = i2b.b[1];
      r = connect(sock, socka, sockaddr_len);
      if r < 0 then wskerr /* process Winsock error */

   }

};

/*******************************************************************************

Get server address

Retrives a server address by name. The name is given as a char*. The address
is returned as an int.

*******************************************************************************/

void addrnet(view name: char*; var addr: int);

var sp: hostent_ptr; /* host entry pointer */
    i2b: record /* int to bytes converter */

       case boolean of

          false: (i: int);
          true:  (b: array 4 of char);

       /* } */

    };

{

   sp = gethostbyname(name); /* get data structure for host address */
   if sp == nil then wskerr; /* process Winsock error */
   /* check at least one address present */
   if sp->h_addr_list^[0] == nil then error(esystem);
   i2b.b[4] = sp->h_addr_list^[0]^[0]; /* get 1st address in list */
   i2b.b[3] = sp->h_addr_list^[0]^[1];
   i2b.b[2] = sp->h_addr_list^[0]^[2];
   i2b.b[1] = sp->h_addr_list^[0]^[3];

   addr = i2b.i /* place result */

};

/*******************************************************************************

Network startup

*******************************************************************************/

static void pa_init_network (void) __attribute__((constructor (103)));
static void pa_init_network()

{

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);

   dblflt = false; /* set no double fault */

   /* clear open files table */
   for fi = 1 to ss_maxhdl do opnfil[fi] = nil; /* set unoccupied */
   /* clear file logical number translator table */
   for fi = 1 to ss_maxhdl do xltfil[fi] = 0; /* set unoccupied */

   /* perform winsock startup */
   r = wsastartup(0x0002, wsd);
   if r <> 0 then error(ewskini); /* can't initalize Winsock */

};

/*******************************************************************************

Network shutdown

*******************************************************************************/

{

   if ! dblflt then { /* we haven't already exited */

      dblflt = true; /* set we already exited */
      /* close all open files && windows */
      for fi = 1 to ss_maxhdl do
         if opnfil[fi] <> nil then
            if (opnfil[fi]->han <> 0) || (opnfil[fi]->net) then
               with opnfil[fi]^ do fileclose(fi)

   }

}.
