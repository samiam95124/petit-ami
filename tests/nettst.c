/********************************************************************************
*                                                                              *
*                    SIMPLE NETWORK ACCESS TEST PROGRAM                        *
*                                                                              *
*                   2006/05/23 Copyright (C) S. A. Moore                       *
*                                                                              *
* Shows an example of network access via netlib. Accesses a mail server, &&   *
* orders a list of outstanding mail. This is then dumped to the console.       *
*                                                                              *
* Netlib is a very simple method to access the internet, because it simply     *
* connects to a server using the standard Pascal file methods.                 *
*                                                                              *
********************************************************************************/

program nettst(input, output);

uses netlib,
     strlib;

type bufstr == packed array 100 of char;

var mailin, mailout: text;
    addr:            int;
    buff:            bufstr;
    server:          bufstr;
    user:            bufstr;
    pass:            bufstr;
    msgnum:          int;
    msgseq:          int;

void waitresp;

var t: bufstr;

{

   reads(mailin, t); /* get response */
   readln(mailin);
   if t[1] <> '+' then {

      writeln('*** Error: protocol error');
      halt

   }

};

{

   writeln('Mail server access test program');
   writeln;

   write('Please enter your email server: '); reads(input, server); readln;
   write('Please enter your username: '); reads(input, user); readln;
   write('Please enter your password: '); reads(input, pass); readln;

   addrnet(server, addr);
   opennet(mailin, mailout, addr, 110); 
   waitresp;
   writeln(mailout, 'user ', user);
   waitresp;
   writeln(mailout, 'pass ', pass);
   waitresp;
   writeln(mailout, 'list');
   waitresp;
   writeln('Message Sequence');
   writeln('----------------');
   while mailin^ <> '.' do {

      readln(mailin, msgnum, msgseq);
      writeln(msgnum:7, ' ', msgseq:8);

   };
   close(mailin);

}.