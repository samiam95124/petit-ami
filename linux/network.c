/*******************************************************************************
*                                                                              *
*                           INTERNET ACCESS LIBRARY                            *
*                                                                              *
*                       Copyright (C) 2006 Scott A. Moore                      *
*                                                                              *
*                              5/06 S. A. Moore                                *
*                                                                              *
* Implements access to internet functions, via tcp/ip. tcp/ip is implemented   *
* via the "file" paradygm. An address and port is used to create a file, then  *
* normal stdio read and write statements are used to access it.                *
*                                                                              *
* Linux (as well as all socket based Unix clones), already treats connections  *
* as files. What the network library does for you is make it portable to       *
* systems that cannot do that (Windows), and also give a standard method to    *
* open such files.                                                             *
*                                                                              *
*******************************************************************************/

#include <network.h>

#define MAXFIL 100 /* maximum number of open files */

typedef char* string;  /* general string type */
typedef enum { false, true } boolean; /* boolean */

/* File tracking.
   Files can be passthrough to the OS, or can be associated with a window. If
   on a window, they can be output, or they can be input. In the case of
   input, the file has its own input queue, and will receive input from all
   windows that are attached to it. */
typedef struct filrec {

   boolean     net;   /* it's a network file */
   boolean     inp;   /* it's the input side of a network file */
   FILE*       han;   /* handle to underlying I/O file */
   int         sock;  /* handle to network socket */
   sc_sockaddr socka; /* socket address structure */
   int         lnk;   /* link to other side of network pair */
   boolean     autoc; /* entry was automatically closed as pair */

} filrec;
typedef filrec* filptr; /* pointer to file record */

/* error codes */
typedef enum {
    ewskini,  /* cannot initalize winsock */
    einvhan,  /* invalid file handle */
    enetopn,  /* cannot reset or rewrite network file */
    enetpos,  /* cannot position network file */
    enetloc,  /* cannot find location network file */
    enetlen,  /* cannot find length network file */
    esckeof,  /* } encountered on socket */
    efinuse,  /* file already in use */
    enetwrt,  /* Attempt to write to input side of network pair */
    esystem   /* System consistency check */
} errcod;

var

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef int (*punlink_t)(const char*);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_unlink(punlink_t nfp, punlink_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static punlink_t ofpunlink;
static plseek_t  ofplseek;

/*
 * Variables
 */
filptr opnfil[MAXFIL]; /* open files table */
int fi; /* index for files table */
int r; /* result */

/*******************************************************************************

Process network library error

Outputs an error message using the special syslib function, then halts.

*******************************************************************************/

static void netwrterr(string s)

{

    fprintf(stderr, "\nError: Sound: %s\n", s);

    exit(1);

}

/*******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.
This needs to go to a dialog instead of the system error trap.

*******************************************************************************/
 
void error(errcod e)

{

    switch (e) { /* error */

        case ewskini:  netwrterr('Cannot initalize winsock'); break;
        case einvhan:  netwrterr('Invalid file number'); break;
        case enetopn:  netwrterr('Cannot reset or rewrite network file'); break;
        case enetpos:  netwrterr('Cannot position network file'); break;
        case enetloc:  netwrterr('Cannot find location network file'); break;
        case enetlen:  netwrterr('Cannot find length network file'); break;
        case esckeof:  netwrterr('} encountered on socket'); break;
        case efinuse:  netwrterr('File already in use'); break;
        case enetwrt:  netwrterr('Attempt to write to input side of network pair'); break;
        case esystem:  netwrterr('System consistency check, please contact v}or'); break;

    }

    exit(1);

}

/*******************************************************************************

Handle Linux error

Handles error in errno. Prints the string assocated with the error.

*******************************************************************************/

void linuxerror(void)

{

    fprintf(stderr, "\nLinux Error: %s\n", strerrpr(errno));

}

/*******************************************************************************

Find length of padded string

Finds the length of a right space padded string.

*******************************************************************************/

function len(view s: string): integer;

var i: integer; /* index for string */

{

   i := max(s); /* index last of string */
   if i <> 0 then { /* string has length */

      while (i > 1) and (s[i] = ' ') do i := i-1; /* find last character */
      if s[i] = ' ' then i := 0 /* handle single blank case */

   };

   len := i /* return length of string */

};

/*******************************************************************************

Find lower case

Finds the lower case of a character.

*******************************************************************************/

function lcase(c: char): char;

{

   if c in ['A'..'Z'] then c := chr(ord(c)-ord('A')+ord('a'));

   lcase := c

};

/*******************************************************************************

Find string match

Finds if the padded strings match, without regard to case.

*******************************************************************************/

function compp(view d, s: string): boolean;

var i:      integer; /* index for string */
    ls, ld: integer; /* length saves */
    r:      boolean; /* result save */

{

   ls := len(s); /* calculate this only once */
   ld := len(d);
   if ls <> ld then r := false /* strings don't match in length */
   else { /* check each character */

      r := true; /* set strings match */
      for i := 1 to ls do if lcase(d[i]) <> lcase(s[i]) then
         r := false /* mismatch */

   };

   compp := r /* return match status */

};

/*******************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

*******************************************************************************/

void getfet(var fp: filptr);

{

   new(fp); /* get a new file entry */
   with fp^ do { /* set up file entry */

      net   := false; /* set not a network file */
      inp   := false; /* set not input side */
      han   := 0;     /* set handle clear */
      sock  := 0;     /* clear out socket entry */
      lnk   := 0;     /* set no link */
      autoc := false; /* set no automatic close */

   }
   
};

/*******************************************************************************

Make file entry

Indexes a present file entry or creates a new one. Looks for a free entry
in the files table, indicated by 0. If found, that is returned, otherwise
the file table is full.

Note that the "predefined" file slots are never allocated.

*******************************************************************************/

void makfil(var fn: ss_filhdl); /* file handle */

var fi: 1..ss_maxhdl; /* index for files table */
    ff: 0..ss_maxhdl; /* found file entry */
    
{

   /* find idle file slot (file with closed file entry) */
   ff := 0; /* clear found file */
   for fi := 1 to ss_maxhdl do { /* search all file entries */

      if opnfil[fi] = nil then /* found an unallocated slot */
         ff := fi /* set found */
      else 
         /* check if slot is allocated, but unoccupied */
         if (opnfil[fi]^.han = 0) and not opnfil[fi]^.net then
            ff := fi /* set found */

   };
   if ff = 0 then error(einvhan); /* no empty file positions found */
   if opnfil[ff] = nil then 
      getfet(opnfil[ff]); /* get and initalize the file entry */
   fn := ff /* set file id number */

};

/*******************************************************************************

Get logical file number from file

Gets the logical translated file number from a text file, and verifies it
is valid.

*******************************************************************************/

function txt2lfn(var f: text): ss_filhdl;

var fn: ss_filhdl;

{

   fn := xltfil[getlfn(f)]; /* get and translate lfn */
   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */

   txt2lfn := fn /* return result */

};

/*******************************************************************************

Check file entry open

Checks if the given file handle corresponds to a file entry that is allocated,
and open.

*******************************************************************************/

void chkopn(fn: ss_filhdl);

{

   if opnfil[fn] = nil then error(einvhan); /* invalid handle */
   if (opnfil[fn]^.han = 0) and not opnfil[fn]^.net then error(einvhan)

};

/*******************************************************************************

Alias file number

Aliases a top level (application program) file number to its syslib equivalent
number. Paslib passes this information down the stack when it opens a file.
Having both the top level number and the syslib equivalent allows us to be
called by the program, as well as interdicting the syslib calls.

*******************************************************************************/

void filealias(fn, fa: ss_filhdl);

{

   /* check file has entry */
   if opnfil[fn] = nil then error(einvhan); /* invalid handle */
   /* throw consistentcy check if alias is bad */
   if (fa < 1) or (fa > ss_maxhdl) then error(esystem);
   xltfil[fa] := fn /* place translation entry */

};

/*******************************************************************************

Resolve filename

Resolves header file parameters. We pass this through.

*******************************************************************************/

void fileresolve(view nm: string; var fs: pstring);

{

   /* check its our special network identifier */
   if not compp(fs^, '_input_network') and 
      not compp(fs^, '_output_network') then {

      /* its one of our specials, just transfer it */
      new(fs, max(nm)); /* get space for string */
      fs^ := nm /* copy */

   } else ss_old_resolve(nm, fs, sav_resolve) /* pass it on */

};

/*******************************************************************************

Open file for read

Opens the file by name, and returns the file handle. Opens for read and write
don't have any purpose for network files, because they are handled by opennet.
The other files are passed through. If an openread is attempted on a net file,
it is an error.

*******************************************************************************/

void fileopenread(var fn: ss_filhdl; view nm: string);

{

   makfil(fn); /* find file slot */
   if not compp(nm, '_input_network') then /* open regular */
      ss_old_openread(opnfil[fn]^.han, nm, sav_openread)

};

/*******************************************************************************

Open file for write

Opens the file by name, and returns the file handle. Opens for read and write
don't have any purpose for network files, because they are handled by opennet.
The other files are passed through. If an openread is attempted on a net file,
it is an error.

*******************************************************************************/

void fileopenwrite(var fn: ss_filhdl; view nm: string);

{

   makfil(fn); /* find file slot */
   if not compp(nm, '_output_network') then /* open regular */
      ss_old_openwrite(opnfil[fn]^.han, nm, sav_openwrite)

};

/*******************************************************************************

Close file

Closes the file. The file is closed at the system level, then we remove the
table entry for the file.

A file can be a normal file, the input side of a network connection, or the 
output side of a network connection. If its normal, it is passed on to the 
operating system. If it's the input or output side of a network connection, 
then the network connection is broken, and both the input and output sides
are closed.

We institute "automatic close forgiveness". Because network files are allocated
in pairs, Paslib will attempt to close out its entire file entry table, and
thus redundantly close these pairs. We flag the automatically closed side
of a pair, which could be either side, and then forgive a call to close it.
Being flagged automatically closed will not prevent the entry from being
reallocated on the next open, so this technique will only work for Paslib.
It could create problems for multitask programs.

*******************************************************************************/

void fileclose(fn: ss_filhdl);

var r:        integer;   /* result */
    /* b:        boolean; */
    ifn, ofn: ss_filhdl; /* input and output side ids */
    acfn:     ss_filhdl; /* automatic close side */

/* close and clear file entry */

void clsfil(var fr: filrec);

{

   with fr do {

      net   := false; /* set not network file */
      inp   := false; /* set not input side */
      han   := 0;     /* clear out file table entry */
      sock  := 0;     /* clear out socket entry */
      lnk   := 0;     /* set no link */
      autoc := false; /* set no automatic close */

   }

};

{

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   if not opnfil[fn]^.autoc then chkopn(fn); /* check its open */
   if opnfil[fn]^.net then { /* it's a network file */

      acfn := opnfil[fn]^.lnk; /* set which side will be automatically closed */
      ifn := fn; /* set input side */
      /* if we have the output side, link back to the input side */
      if not opnfil[fn]^.inp then ifn := opnfil[fn]^.lnk;
      ofn := opnfil[ifn]^.lnk; /* get output side id */
      with opnfil[ifn]^ do { /* close the connection on socket */

         /* b := sc_disconnectex(sock, 0); */ /* disconnect socket */
         /* if not b then wskerr; */ /* cannot disconnnect */
         r := sc_closesocket(sock); /* close socket */
         if r <> 0 then wskerr /* cannot close socket */

      };
      clsfil(opnfil[ifn]^); /* close input side entry */
      clsfil(opnfil[ofn]^); /* close output side entry */
      opnfil[acfn]^.autoc := true /* flag automatically closed side */

   } else if not opnfil[fn]^.autoc then { /* standard file */

      chkopn(fn); /* check valid handle */
      ss_old_close(opnfil[fn]^.han, sav_close); /* close at lower level */
      clsfil(opnfil[fn]^) /* close entry */

   };
   opnfil[fn]^.autoc := false; /* remove automatic close status */

};

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

*******************************************************************************/

void fileread(fn: ss_filhdl; var ba: bytarr);

var r: integer; /* function result */

{

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   if opnfil[fn]^.net then { /* process network file */

      r := sc_recv(opnfil[fn]^.sock, ba, 0); /* receive network data */
      if r = 0 then error(esckeof); /* flag eof error */
      if r <> max(ba) then wskerr /* flag read error */
      
   } else {

      chkopn(fn); /* check valid handle */
      ss_old_read(opnfil[fn]^.han, ba, sav_read) /* pass to lower level */

   }

};

/*******************************************************************************

Write file

Writes a byte buffer to the output file. If the file is normal, we pass it on.
If the file is a network file, we process a write on the associated socket.

*******************************************************************************/

void filewrite(fn: ss_filhdl; view ba: bytarr);

var r: integer; /* function result */

{

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   if opnfil[fn]^.net then { /* process network file */

      if opnfil[fn]^.inp then error(enetwrt); /* write to input side */
      /* transmit network data */
      r := sc_s}(opnfil[opnfil[fn]^.lnk]^.sock, ba, 0);
      if r <> max(ba) then wskerr /* flag write error */

   } else { /* standard file */

      chkopn(fn); /* check valid handle */
      ss_old_write(opnfil[fn]^.han, ba, sav_write) /* pass to lower level */

   }

};

/*******************************************************************************

Position file

Positions the given file. Normal files are passed through. Network files give
an error, since they have no specified location. 

*******************************************************************************/

void fileposition(fn: ss_filhdl; p: integer);

{

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   chkopn(fn); /* check valid handle */
   if opnfil[fn]^.net then error(enetpos) /* cannot position this file */
   else /* standard file */
      ss_old_position(opnfil[fn]^.han, p, sav_position) /* pass to lower level */

};

/*******************************************************************************

Find location of file

Find the location of the given file. Normal files are passed through. Network
files give an error, since they have no specified location.

*******************************************************************************/

function filelocation(fn: ss_filhdl): integer;

{

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   chkopn(fn); /* check valid handle */
   if opnfil[fn]^.net then error(enetpos) /* cannot position this file */
   else /* standard file */
      filelocation := 
         ss_old_location(opnfil[fn]^.han, sav_location) /* pass to lower level */

};

/*******************************************************************************

Find length of file

Find the length of the given file. Normal files are passed through. Network
files give an error, since they have no specified length.

*******************************************************************************/

function filelength(fn: ss_filhdl): integer;

{

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   chkopn(fn); /* check valid handle */
   if opnfil[fn]^.net then error(enetpos) /* cannot position this file */
   else /* standard file */
      /* pass to lower level */
      filelength := ss_old_length(opnfil[fn]^.han, sav_length)

};

/*******************************************************************************

Check eof of file

Returns the eof status on the file. Normal files are passed through. Network
files allways return false, because there is no specified eof signaling
method.

*******************************************************************************/

function fileeof(fn: ss_filhdl): boolean;

{

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); /* invalid file handle */
   chkopn(fn); /* check valid handle */
   if opnfil[fn]^.net then fileeof := false /* network files never } */
   else /* standard file */
      fileeof := ss_old_eof(opnfil[fn]^.han, sav_eof) /* pass to lower level */

};

/*******************************************************************************

Open network file

Opens a network file with the given address and port. The file access is
broken up into a read and write side, that work indep}ently, can can be
spread among different tasks. This is done because the Pascal file paradygm
does not handle read/write on the same file well.

*******************************************************************************/

void opennet(var infile, outfile: text; addr: integer; port: integer);

var ifn, ofn: ss_filhdl; /* input and output file numbers */
    r:        integer;   /* return value */
    i2b: record /* integer to bytes converter */

       case boolean of

          false: (i: integer);
          true:  (b: array 4 of byte);

       /* } */

    };

{

   /* open input side */
   if getlfn(infile) <> 0 then error(efinuse); /* file in use */
   assign(infile, '_input_network'); /* assign special filename */
   reset(infile); /* open file for reading */
   ifn := txt2lfn(infile); /* index that entry */
   opnfil[ifn]^.net := true; /* set open network file */

   /* open output side */
   if getlfn(outfile) <> 0 then error(efinuse); /* file in use */
   assign(outfile, '_output_network'); /* assign special filename */
   rewrite(outfile); /* open file for writing */
   ofn := txt2lfn(outfile); /* index that entry */
   opnfil[ofn]^.net := true; /* set open network file */

   /* cross link the entries */
   opnfil[ofn]^.lnk := ifn; /* link output side to input side */
   opnfil[ifn]^.lnk := ofn; /* link input side to output side */

   /* set up output side */
   opnfil[ofn]^.inp := false; /* set is output side */

   /* Open socket and connect. We keep the parameters on the input side */
   with opnfil[ifn]^ do { /* in file context */

      inp := true; /* set its the input side */
      /* open socket as internet, stream */
      sock := sc_socket(sc_af_inet, sc_sock_stream, 0);
      if sock < 0 then wskerr; /* process Winsock error */
      socka.sin_family := sc_pf_inet;
      /* note, parameters specified in big }ian */
      socka.sin_port := port*$100;
      i2b.i := addr; /* convert address to bytes */
      socka.sin_addr[0] := i2b.b[4];
      socka.sin_addr[1] := i2b.b[3];
      socka.sin_addr[2] := i2b.b[2];
      socka.sin_addr[3] := i2b.b[1];
      r := sc_connect(sock, socka, sc_sockaddr_len);
      if r < 0 then wskerr /* process Winsock error */

   }

};

/*******************************************************************************

Get server address

Retrives a server address by name. The name is given as a string. The address
is returned as an integer.

*******************************************************************************/

void addrnet(view name: string; var addr: integer);

var sp: sc_hostent_ptr; /* host entry pointer */
    i2b: record /* integer to bytes converter */

       case boolean of

          false: (i: integer);
          true:  (b: array 4 of char);

       /* } */

    };

{

   sp := sc_gethostbyname(name); /* get data structure for host address */
   if sp = nil then wskerr; /* process Winsock error */
   /* check at least one address present */
   if sp^.h_addr_list^[0] = nil then error(esystem);
   i2b.b[4] := sp^.h_addr_list^[0]^[0]; /* get 1st address in list */
   i2b.b[3] := sp^.h_addr_list^[0]^[1]; 
   i2b.b[2] := sp^.h_addr_list^[0]^[2]; 
   i2b.b[1] := sp^.h_addr_list^[0]^[3];
   
   addr := i2b.i /* place result */

};

/*******************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
unlink
lseek

We use interdiction to filter standard I/O calls towards the terminal. The
0 (input) and 1 (output) files are interdicted. In ANSI terminal, we act as a
filter, so this does not change the user ability to redirect the file handles
elsewhere.

*******************************************************************************/

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)

{

    ssize_t r;

    if (fd < 1 || fd >= MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd]->net) /* process network file */
        r = read(opnfil[fn]^.sock, buff, count); /* receive network data */
    else {

        chkopn(fn); /* check valid handle */
        r = (*ofpread)(fd, buff, count);

    }

    return (r);

}

/*******************************************************************************

Write

*******************************************************************************/

static ssize_t iwrite(int fn, const void* buff, size_t count)

{

    ssize_t r;

    if (fn < 1 || fn > MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fn]->net) { /* process network file */

        if (opnfil[fn]->inp) error(enetwrt); /* write to input side */
        r = write(openfil[fn]->sock, buff, count); /* send network data */

    } else { /* standard file */

        chkopn(fn); /* check valid handle */
        r = (*ofpwrite)(opnfil[fn]->han, buff, count);

    }

    return (r);

}

/*******************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

*******************************************************************************/

static int iopen(const char* pathname, int flags, int perm)

{

    return (*ofpopen)(pathname, flags, perm);

}

/*******************************************************************************

Close

Does nothing but pass on.

*******************************************************************************/

static int iclose(int fd)

{

    return (*ofpclose)(fd);

}

/*******************************************************************************

Unlink

Unlink has nothing to do with us, so we just pass it on.

*******************************************************************************/

static int iunlink(const char* pathname)

{

    return (*ofpunlink)(pathname);

}

/*******************************************************************************

Lseek

Lseek is never possible on a terminal, so this is always an error on the stdin
or stdout handle.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    /* check seeking on terminal attached file (input or output) and error
       if so */
    if (fd == INPFIL || fd == OUTFIL) error(efilopr);

    return (*ofplseek)(fd, offset, whence);

}
/*******************************************************************************

Netlib startup

*******************************************************************************/

static void pa_init_network (void) __attribute__((constructor (102)));
static void pa_init_network()

{

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
//    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

    /* clear open files table */
    for (fi = 0; fi < MAXFIL; fi++) opnfil[fi] = NULL; /* set unoccupied */

};

/*******************************************************************************

Netlib shutdown

*******************************************************************************/

static void pa_deinit_network (void) __attribute__((destructor (102)));
static void pa_deinit_network()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
//    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);

}
