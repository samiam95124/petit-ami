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
* normal Pascal read and write statements are used to access it.               *
*                                                                              *
*******************************************************************************/

#include "network.h"

type

   { File tracking.
     Files can be passthrough to syslib, or can be associated with a window. If
     on a window, they can be output, or they can be input. In the case of
     input, the file has its own input queue, and will receive input from all
     windows that are attached to it. }
   filptr = ^filrec;
   filrec = record

      net:   boolean;     { it's a network file }
      inp:   boolean;     { it's the input side of a network file }
      han:   ss_filhdl;   { handle to underlying I/O file }
      sock:  integer;     { handle to network socket }
      socka: sc_sockaddr; { socket address structure }
      lnk:   ss_filhdl;   { link to other side of network pair }
      autoc: boolean;     { entry was automatically closed as pair }

   end;
   { error codes }
   errcod = (ewskini,  { cannot initalize winsock }
             einvhan,  { invalid file handle }
             enetopn,  { cannot reset or rewrite network file }
             enetpos,  { cannot position network file }
             enetloc,  { cannot find location network file }
             enetlen,  { cannot find length network file }
             esckeof,  { End encountered on socket }
             efinuse,  { file already in use }
             enetwrt,  { Attempt to write to input side of network pair }
             esystem); { System consistency check }

var

    { saves for hooked routines }
    sav_alias:     ss_pp;
    sav_resolve:   ss_pp;
    sav_openread:  ss_pp;
    sav_openwrite: ss_pp;
    sav_close:     ss_pp;
    sav_read:      ss_pp;
    sav_write:     ss_pp;
    sav_position:  ss_pp;
    sav_location:  ss_pp;
    sav_length:    ss_pp;
    sav_eof:       ss_pp;

    opnfil: array [1..ss_maxhdl] of filptr; { open files table }
    xltfil: array [1..ss_maxhdl] of ss_filhdl; { window equivalence table }
    fi:     1..ss_maxhdl; { index for files table }
    r:      integer; { result }
    wsd: sc_wsadata; { Winsock data structure }

    { The double fault flag is set when exiting, so if we exit again, it
      is checked, then forces an immediate exit. This keeps faults from
      looping. }

    dblflt:     boolean; { double fault flag }

{******************************************************************************

Process network library error

Outputs an error message using the special syslib function, then halts.

******************************************************************************}

procedure netwrterr(view s: string);

var i:     integer; { index for string }
    pream: packed array [1..8] of char; { preamble string }
    p:     pstring; { pointer to string }

begin

   pream := 'Netlib: '; { set preamble }
   new(p, max(s)+8); { get string to hold }
   for i := 1 to 8 do p^[i] := pream[i]; { copy preamble }
   for i := 1 to max(s) do p^[i+8] := s[i]; { copy string }
   ss_wrterr(p^); { output string }
   dispose(p); { release storage }

end;

{******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.
This needs to go to a dialog instead of the system error trap.

******************************************************************************}
 
procedure error(e: errcod);

begin

   case e of { error }

      ewskini:  netwrterr('Cannot initalize winsock');
      einvhan:  netwrterr('Invalid file number');
      enetopn:  netwrterr('Cannot reset or rewrite network file');
      enetpos:  netwrterr('Cannot position network file');
      enetloc:  netwrterr('Cannot find location network file');
      enetlen:  netwrterr('Cannot find length network file');
      esckeof:  netwrterr('End encountered on socket');
      efinuse:  netwrterr('File already in use');
      enetwrt:  netwrterr('Attempt to write to input side of network pair');
      esystem:  netwrterr('System consistency check, please contact vendor');

   end;
   goto 88 { abort module }

end;

{******************************************************************************

Handle Winsock error

Only called if the last error variable is set. The text string for the error
is output, and then the program halted.

******************************************************************************}

procedure wskerr;

var e:      integer;
    es, ts: pstring;

function cat(view sa, sb: string): pstring;

var i: integer; { index for string }
    d: pstring; { temp string }

begin

   new(d, max(sa)+max(sb)); { create destination }
   for i := 1 to max(sa) do d^[i] := sa[i]; { copy left string }
   for i := 1 to max(sb) do d^[max(sa)+i] := sb[i]; { copy right string }

   cat := d { return result }

end;

function copy(view s: string): pstring;

var t: pstring;

begin

   new(t, max(s)); { create destination }
   t^ := s; { copy string }

   copy := t { return result }

end;

begin

   e := sc_wsagetlasterror;
   case e of { winsock error }

      sc_WSAEINTR:                   
         ts := copy('WSAEINTR: Interrupted function call');                  
      sc_WSAEBADF:                   
         ts := copy('WSAEBADF: (error message unspecified)');      
      sc_WSAEACCES:                  
         ts := copy('WSAEACCES: Permission denied');      
      sc_WSAEFAULT:                  
         ts := copy('WSAEFAULT: Bad address');      
      sc_WSAEINVAL:                  
         ts := copy('WSAEINVAL: Invalid argument');      
      sc_WSAEMFILE:                  
         ts := copy('WSAEMFILE: Too many open files');      
      sc_WSAEWOULDBLOCK:             
         ts := copy('WSAEWOULDBLOCK: Resource temporarily unavailable');      
      sc_WSAEINPROGRESS:             
         ts := copy('WSAEINPROGRESS: Operation now in progress');      
      sc_WSAEALREADY:                
         ts := copy('WSAEALREADY: Operation already in progress');      
      sc_WSAENOTSOCK:                
         ts := copy('WSAENOTSOCK: Socket operation on nonsocket');      
      sc_WSAEDESTADDRREQ:            
         ts := copy('WSAEDESTADDRREQ: Destination address required');      
      sc_WSAEMSGSIZE:                
         ts := copy('WSAEMSGSIZE: Message too long');      
      sc_WSAEPROTOTYPE:              
         ts := copy('WSAEPROTOTYPE: Protocol wrong type for socket');      
      sc_WSAENOPROTOOPT:             
         ts := copy('WSAENOPROTOOPT: Bad protocol option');      
      sc_WSAEPROTONOSUPPORT:         
         ts := copy('WSAEPROTONOSUPPORT: Protocol not supported');      
      sc_WSAESOCKTNOSUPPORT:         
         ts := copy('WSAESOCKTNOSUPPORT: Socket type not supported');      
      sc_WSAEOPNOTSUPP:              
         ts := copy('WSAEOPNOTSUPP: Operation not supported');      
      sc_WSAEPFNOSUPPORT:            
         ts := copy('WSAEPFNOSUPPORT: Protocol family not supported');      
      sc_WSAEAFNOSUPPORT: 
ts := copy('WSAEAFNOSUPPORT: Address family not supported by protocol family');
      sc_WSAEADDRINUSE:              
         ts := copy('WSAEADDRINUSE: Address already in use');      
      sc_WSAEADDRNOTAVAIL:           
         ts := copy('WSAEADDRNOTAVAIL: Cannot assign requested address');      
      sc_WSAENETDOWN:                
         ts := copy('WSAENETDOWN: Network is down');      
      sc_WSAENETUNREACH:             
         ts := copy('WSAENETUNREACH: Network is unreachable');      
      sc_WSAENETRESET:               
         ts := copy('WSAENETRESET: Network dropped connection on reset');      
      sc_WSAECONNABORTED:            
         ts := copy('WSAECONNABORTED: Software caused connection abort');      
      sc_WSAECONNRESET:              
         ts := copy('WSAECONNRESET: Connection reset by peer');      
      sc_WSAENOBUFS:                 
         ts := copy('WSAENOBUFS: No buffer space available');      
      sc_WSAEISCONN:                 
         ts := copy('WSAEISCONN: Socket is already connected');      
      sc_WSAENOTCONN:                
         ts := copy('WSAENOTCONN: Socket is not connected');      
      sc_WSAESHUTDOWN:               
         ts := copy('WSAESHUTDOWN: Cannot send after socket shutdown');      
      sc_WSAETOOMANYREFS:            
         ts := copy('WSAETOOMANYREFS: (error message unspecified)');      
      sc_WSAETIMEDOUT:               
         ts := copy('WSAETIMEDOUT: Connection timed out');      
      sc_WSAECONNREFUSED:            
         ts := copy('WSAECONNREFUSED: Connection refused');      
      sc_WSAELOOP:                   
         ts := copy('WSAELOOP: (error message unspecified)');      
      sc_WSAENAMETOOLONG:            
         ts := copy('WSAENAMETOOLONG: (error message unspecified)');      
      sc_WSAEHOSTDOWN:               
         ts := copy('WSAEHOSTDOWN: Host is down');      
      sc_WSAEHOSTUNREACH:            
         ts := copy('WSAEHOSTUNREACH: No route to host');      
      sc_WSAENOTEMPTY:               
         ts := copy('WSAENOTEMPTY: (error message unspecified)');      
      sc_WSAEPROCLIM:                
         ts := copy('WSAEPROCLIM: Too many processes');      
      sc_WSAEUSERS:                  
         ts := copy('WSAEUSERS: (error message unspecified)');      
      sc_WSAEDQUOT:                  
         ts := copy('WSAEDQUOT: (error message unspecified)');      
      sc_WSAESTALE:                  
         ts := copy('WSAESTALE: (error message unspecified)');      
      sc_WSAEREMOTE:                 
         ts := copy('WSAEREMOTE: (error message unspecified)');      
      sc_WSASYSNOTREADY:             
         ts := copy('WSASYSNOTREADY: Network subsystem is unavailable');      
      sc_WSAVERNOTSUPPORTED:         
         ts := copy('WSAVERNOTSUPPORTED: Winsock.dll version out of range');      
      sc_WSANOTINITIALISED:          
ts := copy('WSANOTINITIALISED: Successful WSAStartup not yet performed');
      sc_WSAEDISCON:                 
         ts := copy('WSAEDISCON: Graceful shutdown in progress');      
      sc_WSAENOMORE:                 
         ts := copy('WSAENOMORE: (error message unspecified)');      
      sc_WSAECANCELLED:              
         ts := copy('WSAECANCELLED: (error message unspecified)');      
      sc_WSAEINVALIDPROCTABLE:       
         ts := copy('WSAEINVALIDPROCTABLE: (error message unspecified)');   
      sc_WSAEINVALIDPROVIDER:        
         ts := copy('WSAEINVALIDPROVIDER: (error message unspecified)');   
      sc_WSAEPROVIDERFAILEDINIT:     
         ts := copy('WSAEPROVIDERFAILEDINIT: (error message unspecified)');   
      sc_WSASYSCALLFAILURE:          
         ts := copy('WSASYSCALLFAILURE: (error message unspecified)');   
      sc_WSASERVICE_NOT_FOUND:       
         ts := copy('WSASERVICE_NOT_FOUND: (error message unspecified)');   
      sc_WSATYPE_NOT_FOUND:          
         ts := copy('WSATYPE_NOT_FOUND: Class type not found');   
      sc_WSA_E_NO_MORE:              
         ts := copy('WSA_E_NO_MORE: (error message unspecified)');   
      sc_WSA_E_CANCELLED:            
         ts := copy('WSA_E_CANCELLED: (error message unspecified)');   
      sc_WSAEREFUSED:                
         ts := copy('WSAEREFUSED: (error message unspecified)');   
      sc_WSAHOST_NOT_FOUND:          
         ts := copy('WSAHOST_NOT_FOUND: Host not found');   
      sc_WSATRY_AGAIN:               
         ts := copy('WSATRY_AGAIN: Nonauthoritative host not found');   
      sc_WSANO_RECOVERY:             
         ts := copy('WSANO_RECOVERY: This is a nonrecoverable error');   
      sc_WSANO_DATA:                 
         ts := copy('WSANO_DATA: Valid name, no data record of requested type');
      sc_WSA_QOS_RECEIVERS:          
         ts := copy('WSA_QOS_RECEIVERS: at least one Reserve has arrived');   
      sc_WSA_QOS_SENDERS:            
         ts := copy('WSA_QOS_SENDERS: at least one Path has arrived');   
      sc_WSA_QOS_NO_SENDERS:         
         ts := copy('WSA_QOS_NO_SENDERS: there are no senders');   
      sc_WSA_QOS_NO_RECEIVERS:       
         ts := copy('WSA_QOS_NO_RECEIVERS: there are no receivers');   
      sc_WSA_QOS_REQUEST_CONFIRMED:  
         ts := copy('WSA_QOS_REQUEST_CONFIRMED: Reserve has been confirmed');
      sc_WSA_QOS_ADMISSION_FAILURE:  
ts := copy('WSA_QOS_ADMISSION_FAILURE: error due to lack of resources');
      sc_WSA_QOS_POLICY_FAILURE:     
ts := copy('WSA_QOS_POLICY_FAILURE: rejected for administrative reasons - bad credentials');
      sc_WSA_QOS_BAD_STYLE:          
         ts := copy('WSA_QOS_BAD_STYLE: unknown or conflicting style');
      sc_WSA_QOS_BAD_OBJECT:         
ts := copy('WSA_QOS_BAD_OBJECT: problem with some part of the filterspec or provider specific buffer in general');
      sc_WSA_QOS_TRAFFIC_CTRL_ERROR: 
ts := copy('WSA_QOS_TRAFFIC_CTRL_ERROR: problem with some part of the flowspec');
      sc_WSA_QOS_GENERIC_ERROR:      
         ts := copy('WSA_QOS_GENERIC_ERROR: general error');
      sc_WSA_QOS_ESERVICETYPE:       
         ts := copy('WSA_QOS_ESERVICETYPE: invalid service type in flowspec');
      sc_WSA_QOS_EFLOWSPEC:          
         ts := copy('WSA_QOS_EFLOWSPEC: invalid flowspec');
      sc_WSA_QOS_EPROVSPECBUF:       
         ts := copy('WSA_QOS_EPROVSPECBUF: invalid provider specific buffer');
      sc_WSA_QOS_EFILTERSTYLE:       
         ts := copy('WSA_QOS_EFILTERSTYLE: invalid filter style');
      sc_WSA_QOS_EFILTERTYPE:        
         ts := copy('WSA_QOS_EFILTERTYPE: invalid filter type');
      sc_WSA_QOS_EFILTERCOUNT:       
         ts := copy('WSA_QOS_EFILTERCOUNT: incorrect number of filters');
      sc_WSA_QOS_EOBJLENGTH:         
         ts := copy('WSA_QOS_EOBJLENGTH: invalid object length');
      sc_WSA_QOS_EFLOWCOUNT:         
         ts := copy('WSA_QOS_EFLOWCOUNT: incorrect number of flows');
      sc_WSA_QOS_EUNKOWNPSOBJ:       
ts := copy('WSA_QOS_EUNKOWNPSOBJ: unknown object in provider specific buffer');
      sc_WSA_QOS_EPOLICYOBJ:         
ts := copy('WSA_QOS_EPOLICYOBJ: invalid policy object in provider specific buffer');
      sc_WSA_QOS_EFLOWDESC:          
         ts := copy('WSA_QOS_EFLOWDESC: invalid flow descriptor in the list');
      sc_WSA_QOS_EPSFLOWSPEC:        
ts := copy('WSA_QOS_EPSFLOWSPEC: inconsistent flow spec in provider specific buffer');
      sc_WSA_QOS_EPSFILTERSPEC:      
ts := copy('WSA_QOS_EPSFILTERSPEC: invalid filter spec in provider specific buffer');
      sc_WSA_QOS_ESDMODEOBJ:         
ts := copy('WSA_QOS_ESDMODEOBJ: invalid shape discard mode object in provider specific buffer');
      sc_WSA_QOS_ESHAPERATEOBJ:      
ts := copy('WSA_QOS_ESHAPERATEOBJ: invalid shaping rate object in provider specific buffer');
      sc_WSA_QOS_RESERVED_PETYPE:    
ts := copy('WSA_QOS_RESERVED_PETYPE: reserved policy element in provider specific buffer');
      else ts := copy('???')

   end;
   es := cat('Winsock error: ', ts^); { form message }
   dispose(ts); { release temp }
   netwrterr(es^); { process error }
   dispose(es); { release message }
   goto 88 { abort module }

end;

{******************************************************************************

Find length of padded string

Finds the length of a right space padded string.

******************************************************************************}

function len(view s: string): integer;

var i: integer; { index for string }

begin

   i := max(s); { index last of string }
   if i <> 0 then begin { string has length }

      while (i > 1) and (s[i] = ' ') do i := i-1; { find last character }
      if s[i] = ' ' then i := 0 { handle single blank case }

   end;

   len := i { return length of string }

end;

{******************************************************************************

Find lower case

Finds the lower case of a character.

******************************************************************************}

function lcase(c: char): char;

begin

   if c in ['A'..'Z'] then c := chr(ord(c)-ord('A')+ord('a'));

   lcase := c

end;

{******************************************************************************

Find string match

Finds if the padded strings match, without regard to case.

******************************************************************************}

function compp(view d, s: string): boolean;

var i:      integer; { index for string }
    ls, ld: integer; { length saves }
    r:      boolean; { result save }

begin

   ls := len(s); { calculate this only once }
   ld := len(d);
   if ls <> ld then r := false { strings don't match in length }
   else begin { check each character }

      r := true; { set strings match }
      for i := 1 to ls do if lcase(d[i]) <> lcase(s[i]) then
         r := false { mismatch }

   end;

   compp := r { return match status }

end;

{******************************************************************************

Get file entry

Allocates and initalizes a new file entry. File entries are left in the opnfil
array, so are recycled in place.

******************************************************************************}

procedure getfet(var fp: filptr);

begin

   new(fp); { get a new file entry }
   with fp^ do begin { set up file entry }

      net   := false; { set not a network file }
      inp   := false; { set not input side }
      han   := 0;     { set handle clear }
      sock  := 0;     { clear out socket entry }
      lnk   := 0;     { set no link }
      autoc := false; { set no automatic close }

   end
   
end;

{******************************************************************************

Make file entry

Indexes a present file entry or creates a new one. Looks for a free entry
in the files table, indicated by 0. If found, that is returned, otherwise
the file table is full.

Note that the "predefined" file slots are never allocated.

******************************************************************************}

procedure makfil(var fn: ss_filhdl); { file handle }

var fi: 1..ss_maxhdl; { index for files table }
    ff: 0..ss_maxhdl; { found file entry }
    
begin

   { find idle file slot (file with closed file entry) }
   ff := 0; { clear found file }
   for fi := 1 to ss_maxhdl do begin { search all file entries }

      if opnfil[fi] = nil then { found an unallocated slot }
         ff := fi { set found }
      else 
         { check if slot is allocated, but unoccupied }
         if (opnfil[fi]^.han = 0) and not opnfil[fi]^.net then
            ff := fi { set found }

   end;
   if ff = 0 then error(einvhan); { no empty file positions found }
   if opnfil[ff] = nil then 
      getfet(opnfil[ff]); { get and initalize the file entry }
   fn := ff { set file id number }

end;

{******************************************************************************

Get logical file number from file

Gets the logical translated file number from a text file, and verifies it
is valid.

******************************************************************************}

function txt2lfn(var f: text): ss_filhdl;

var fn: ss_filhdl;

begin

   fn := xltfil[getlfn(f)]; { get and translate lfn }
   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }

   txt2lfn := fn { return result }

end;

{******************************************************************************

Check file entry open

Checks if the given file handle corresponds to a file entry that is allocated,
and open.

******************************************************************************}

procedure chkopn(fn: ss_filhdl);

begin

   if opnfil[fn] = nil then error(einvhan); { invalid handle }
   if (opnfil[fn]^.han = 0) and not opnfil[fn]^.net then error(einvhan)

end;

{******************************************************************************

Alias file number

Aliases a top level (application program) file number to its syslib equivalent
number. Paslib passes this information down the stack when it opens a file.
Having both the top level number and the syslib equivalent allows us to be
called by the program, as well as interdicting the syslib calls.

******************************************************************************}

procedure filealias(fn, fa: ss_filhdl);

begin

   { check file has entry }
   if opnfil[fn] = nil then error(einvhan); { invalid handle }
   { throw consistentcy check if alias is bad }
   if (fa < 1) or (fa > ss_maxhdl) then error(esystem);
   xltfil[fa] := fn { place translation entry }

end;

{******************************************************************************

Resolve filename

Resolves header file parameters. We pass this through.

******************************************************************************}

procedure fileresolve(view nm: string; var fs: pstring);

begin

   { check its our special network identifier }
   if not compp(fs^, '_input_network') and 
      not compp(fs^, '_output_network') then begin

      { its one of our specials, just transfer it }
      new(fs, max(nm)); { get space for string }
      fs^ := nm { copy }

   end else ss_old_resolve(nm, fs, sav_resolve) { pass it on }

end;

{******************************************************************************

Open file for read

Opens the file by name, and returns the file handle. Opens for read and write
don't have any purpose for network files, because they are handled by opennet.
The other files are passed through. If an openread is attempted on a net file,
it is an error.

******************************************************************************}

procedure fileopenread(var fn: ss_filhdl; view nm: string);

begin

   makfil(fn); { find file slot }
   if not compp(nm, '_input_network') then { open regular }
      ss_old_openread(opnfil[fn]^.han, nm, sav_openread)

end;

{******************************************************************************

Open file for write

Opens the file by name, and returns the file handle. Opens for read and write
don't have any purpose for network files, because they are handled by opennet.
The other files are passed through. If an openread is attempted on a net file,
it is an error.

******************************************************************************}

procedure fileopenwrite(var fn: ss_filhdl; view nm: string);

begin

   makfil(fn); { find file slot }
   if not compp(nm, '_output_network') then { open regular }
      ss_old_openwrite(opnfil[fn]^.han, nm, sav_openwrite)

end;

{******************************************************************************

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

******************************************************************************}

procedure fileclose(fn: ss_filhdl);

var r:        integer;   { result }
    { b:        boolean; }
    ifn, ofn: ss_filhdl; { input and output side ids }
    acfn:     ss_filhdl; { automatic close side }

{ close and clear file entry }

procedure clsfil(var fr: filrec);

begin

   with fr do begin

      net   := false; { set not network file }
      inp   := false; { set not input side }
      han   := 0;     { clear out file table entry }
      sock  := 0;     { clear out socket entry }
      lnk   := 0;     { set no link }
      autoc := false; { set no automatic close }

   end

end;

begin

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   if not opnfil[fn]^.autoc then chkopn(fn); { check its open }
   if opnfil[fn]^.net then begin { it's a network file }

      acfn := opnfil[fn]^.lnk; { set which side will be automatically closed }
      ifn := fn; { set input side }
      { if we have the output side, link back to the input side }
      if not opnfil[fn]^.inp then ifn := opnfil[fn]^.lnk;
      ofn := opnfil[ifn]^.lnk; { get output side id }
      with opnfil[ifn]^ do begin { close the connection on socket }

         { b := sc_disconnectex(sock, 0); } { disconnect socket }
         { if not b then wskerr; } { cannot disconnnect }
         r := sc_closesocket(sock); { close socket }
         if r <> 0 then wskerr { cannot close socket }

      end;
      clsfil(opnfil[ifn]^); { close input side entry }
      clsfil(opnfil[ofn]^); { close output side entry }
      opnfil[acfn]^.autoc := true { flag automatically closed side }

   end else if not opnfil[fn]^.autoc then begin { standard file }

      chkopn(fn); { check valid handle }
      ss_old_close(opnfil[fn]^.han, sav_close); { close at lower level }
      clsfil(opnfil[fn]^) { close entry }

   end;
   opnfil[fn]^.autoc := false; { remove automatic close status }

end;

{******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

******************************************************************************}

procedure fileread(fn: ss_filhdl; var ba: bytarr);

var r: integer; { function result }

begin

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   if opnfil[fn]^.net then begin { process network file }

      r := sc_recv(opnfil[fn]^.sock, ba, 0); { receive network data }
      if r = 0 then error(esckeof); { flag eof error }
      if r <> max(ba) then wskerr { flag read error }
      
   end else begin

      chkopn(fn); { check valid handle }
      ss_old_read(opnfil[fn]^.han, ba, sav_read) { pass to lower level }

   end

end;

{******************************************************************************

Write file

Writes a byte buffer to the output file. If the file is normal, we pass it on.
If the file is a network file, we process a write on the associated socket.

******************************************************************************}

procedure filewrite(fn: ss_filhdl; view ba: bytarr);

var r: integer; { function result }

begin

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   if opnfil[fn]^.net then begin { process network file }

      if opnfil[fn]^.inp then error(enetwrt); { write to input side }
      { transmit network data }
      r := sc_send(opnfil[opnfil[fn]^.lnk]^.sock, ba, 0);
      if r <> max(ba) then wskerr { flag write error }

   end else begin { standard file }

      chkopn(fn); { check valid handle }
      ss_old_write(opnfil[fn]^.han, ba, sav_write) { pass to lower level }

   end

end;

{******************************************************************************

Position file

Positions the given file. Normal files are passed through. Network files give
an error, since they have no specified location. 

******************************************************************************}

procedure fileposition(fn: ss_filhdl; p: integer);

begin

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   chkopn(fn); { check valid handle }
   if opnfil[fn]^.net then error(enetpos) { cannot position this file }
   else { standard file }
      ss_old_position(opnfil[fn]^.han, p, sav_position) { pass to lower level }

end;

{******************************************************************************

Find location of file

Find the location of the given file. Normal files are passed through. Network
files give an error, since they have no specified location.

******************************************************************************}

function filelocation(fn: ss_filhdl): integer;

begin

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   chkopn(fn); { check valid handle }
   if opnfil[fn]^.net then error(enetpos) { cannot position this file }
   else { standard file }
      filelocation := 
         ss_old_location(opnfil[fn]^.han, sav_location) { pass to lower level }

end;

{******************************************************************************

Find length of file

Find the length of the given file. Normal files are passed through. Network
files give an error, since they have no specified length.

******************************************************************************}

function filelength(fn: ss_filhdl): integer;

begin

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   chkopn(fn); { check valid handle }
   if opnfil[fn]^.net then error(enetpos) { cannot position this file }
   else { standard file }
      { pass to lower level }
      filelength := ss_old_length(opnfil[fn]^.han, sav_length)

end;

{******************************************************************************

Check eof of file

Returns the eof status on the file. Normal files are passed through. Network
files allways return false, because there is no specified eof signaling
method.

******************************************************************************}

function fileeof(fn: ss_filhdl): boolean;

begin

   if (fn < 1) or (fn > ss_maxhdl) then error(einvhan); { invalid file handle }
   chkopn(fn); { check valid handle }
   if opnfil[fn]^.net then fileeof := false { network files never end }
   else { standard file }
      fileeof := ss_old_eof(opnfil[fn]^.han, sav_eof) { pass to lower level }

end;

{******************************************************************************

Open network file

Opens a network file with the given address and port. The file access is
broken up into a read and write side, that work independently, can can be
spread among different tasks. This is done because the Pascal file paradygm
does not handle read/write on the same file well.

******************************************************************************}

procedure opennet(var infile, outfile: text; addr: integer; port: integer);

var ifn, ofn: ss_filhdl; { input and output file numbers }
    r:        integer;   { return value }
    i2b: record { integer to bytes converter }

       case boolean of

          false: (i: integer);
          true:  (b: array 4 of byte);

       { end }

    end;

begin

   { open input side }
   if getlfn(infile) <> 0 then error(efinuse); { file in use }
   assign(infile, '_input_network'); { assign special filename }
   reset(infile); { open file for reading }
   ifn := txt2lfn(infile); { index that entry }
   opnfil[ifn]^.net := true; { set open network file }

   { open output side }
   if getlfn(outfile) <> 0 then error(efinuse); { file in use }
   assign(outfile, '_output_network'); { assign special filename }
   rewrite(outfile); { open file for writing }
   ofn := txt2lfn(outfile); { index that entry }
   opnfil[ofn]^.net := true; { set open network file }

   { cross link the entries }
   opnfil[ofn]^.lnk := ifn; { link output side to input side }
   opnfil[ifn]^.lnk := ofn; { link input side to output side }

   { set up output side }
   opnfil[ofn]^.inp := false; { set is output side }

   { Open socket and connect. We keep the parameters on the input side }
   with opnfil[ifn]^ do begin { in file context }

      inp := true; { set its the input side }
      { open socket as internet, stream }
      sock := sc_socket(sc_af_inet, sc_sock_stream, 0);
      if sock < 0 then wskerr; { process Winsock error }
      socka.sin_family := sc_pf_inet;
      { note, parameters specified in big endian }
      socka.sin_port := port*$100;
      i2b.i := addr; { convert address to bytes }
      socka.sin_addr[0] := i2b.b[4];
      socka.sin_addr[1] := i2b.b[3];
      socka.sin_addr[2] := i2b.b[2];
      socka.sin_addr[3] := i2b.b[1];
      r := sc_connect(sock, socka, sc_sockaddr_len);
      if r < 0 then wskerr { process Winsock error }

   end

end;

{******************************************************************************

Get server address

Retrives a server address by name. The name is given as a string. The address
is returned as an integer.

******************************************************************************}

procedure addrnet(view name: string; var addr: integer);

var sp: sc_hostent_ptr; { host entry pointer }
    i2b: record { integer to bytes converter }

       case boolean of

          false: (i: integer);
          true:  (b: array 4 of char);

       { end }

    end;

begin

   sp := sc_gethostbyname(name); { get data structure for host address }
   if sp = nil then wskerr; { process Winsock error }
   { check at least one address present }
   if sp^.h_addr_list^[0] = nil then error(esystem);
   i2b.b[4] := sp^.h_addr_list^[0]^[0]; { get 1st address in list }
   i2b.b[3] := sp^.h_addr_list^[0]^[1]; 
   i2b.b[2] := sp^.h_addr_list^[0]^[2]; 
   i2b.b[1] := sp^.h_addr_list^[0]^[3];
   
   addr := i2b.i { place result }

end;

{******************************************************************************

Netlib startup

******************************************************************************}

begin

   { override our interdicted calls }
   ss_ovr_alias(filealias, sav_alias);
   ss_ovr_resolve(fileresolve, sav_resolve);
   ss_ovr_openread(fileopenread, sav_openread);
   ss_ovr_openwrite(fileopenwrite, sav_openwrite);
   ss_ovr_close(fileclose, sav_close);
   ss_ovr_read(fileread, sav_read);
   ss_ovr_write(filewrite, sav_write);
   ss_ovr_position(fileposition, sav_position);
   ss_ovr_location(filelocation, sav_location);
   ss_ovr_length(filelength, sav_length);
   ss_ovr_eof(fileeof, sav_eof);

   dblflt := false; { set no double fault }

   { clear open files table }
   for fi := 1 to ss_maxhdl do opnfil[fi] := nil; { set unoccupied }
   { clear file logical number translator table }
   for fi := 1 to ss_maxhdl do xltfil[fi] := 0; { set unoccupied }

   { perform winsock startup }
   r := sc_wsastartup($0002, wsd);
   if r <> 0 then error(ewskini); { can't initalize Winsock }

   { diagnostic routines (come in and out of use) }
   refer(print);
   refer(printn);
   refer(prtstr);
   refer(prtnum);

end;

{******************************************************************************

Netlib shutdown

******************************************************************************}

begin

   88: { abort module }

   if not dblflt then begin { we haven't already exited }

      dblflt := true; { set we already exited }
      { close all open files and windows }
      for fi := 1 to ss_maxhdl do
         if opnfil[fi] <> nil then 
            if (opnfil[fi]^.han <> 0) or (opnfil[fi]^.net) then
               with opnfil[fi]^ do fileclose(fi)

   end

end.
