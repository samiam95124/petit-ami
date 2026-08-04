# What this program is

A mail reader that keeps your mail here rather than on somebody's
server. It reads mail from IMAP servers into files on this machine, and
everything it shows you it finds by reading those files again.

The rule it is built to, and the one that explains most of what follows:
**the server folders are watched, not managed.** They are read and
mirrored here. Nothing is created, moved, renamed or flagged on any
server. The folders you make are local, they live here, and the servers
are never told about them. The one change to a server this program will
ever make is taking mail off it, and that is not written yet.

## Why keep mail locally

Three reasons, and they build on each other. You get a copy that is
yours, in a format any mail program can read. You stop paying for
storage you could keep here. And once the mail is a file on your disk,
anything can be applied to it -- your own rules, your own tools, without
asking anybody's permission.

# Getting started

Config/Servers asks for an account: where the mail is read from, who to
log in as, and how much of each folder to take at a time. Get Mail
fetches. That is the whole of it.

For Gmail the password is not your account password. Google requires an
application password, which it issues per program to an account that has
two step verification turned on, from myaccount.google.com/apppasswords.
It is sixteen characters; the spaces it is shown with are not part of
it.

The mail server is imap.gmail.com on port 993. The sending server is
smtp.gmail.com on port 465 -- not 587, which begins in the clear and
expects STARTTLS, which this cannot do.

## Messages

How many of each folder to fetch. Start small. A mailbox of forty
thousand messages is fetched one message at a time, and the first look
at it should not be an afternoon. Fetching again takes only what is new,
so a small number costs nothing later.

# More than one account

Config/Servers holds several. Next walks through them, Add starts
another, Remove takes one away. Each has a name of its own, which is how
its folders are labelled in the pane and what its directory in the store
is called.

Mail is gathered from every account. Where it goes afterwards is not the
accounts' business: the local folders are one set, shared. A folder
called Bills is about bills, not about which company carried the letter.

Renaming an account renames its directory, so nothing has to be fetched
again.

# Looking by itself

The program looks at the servers every so often while it is open --
every fifteen seconds unless the form says otherwise. Set it to zero and
it will only fetch when you ask.

A look on the timer does not ask the servers what folders they have.
Folders do not come and go by the quarter minute, and asking costs a
connection to each server. Get Mail asks; the timer only fetches.

# Folders

Down the left, in sections: one for each account's folders, then one for
the folders that are yours. They are separate lists rather than one list
marked up, because two accounts may each have an INBOX and a local Trash
is not the server's Trash. Which is which has to be plain at a glance.

The number beside a folder is how many messages are in it here.

## Local folders

Yours. Made here, filled here, never mentioned to any server. Right
click a message to make one.

# Reading

The list shows, for each message: who it is from, what kind of mail it
is, the subject, as much of the message as fits, and when it arrived.
Click one to open it in a window of its own.

The wheel moves one message a notch. The arrows, the page keys and the
scroll bar all work. In an open message the wheel moves three lines,
since text is not read a message at a time.

Mail is not text any more, so there is enough MIME here to read it:
encoded subject lines, quoted printable, base64, multipart gone into for
the plain text part, and html with the tags taken out where plain text
is not on offer.

# The second mouse button

On a message, three things, all of them local:

- **Move to local Trash** -- that message, to a local folder called
  Trash. Not the server's Trash, which is untouched.
- **Local folder for a place** -- everything in this folder from that
  domain. LinkedIn writes from four addresses and they are all LinkedIn.
- **Local folder for a name** -- everything from that display name.
  Facebook writes under eight of them, one per friend, and sometimes
  that is what you want kept apart.

The menu says how many messages each would take, because which one is
right depends on the sender, and the count is what makes it a choice
rather than a guess. The menu is cancelled by the next click.

# What kind of mail this is

Every message is given a category, shown in its own column. The rules
are in a file, mail.cat, not in the program: a category and what it
matches, one to a line, first match wins. Change them, add to them or
throw them away without a compiler.

The rule that does the most work is the last one. Machine-sent mail says
so in its headers -- List-Unsubscribe, List-Id, Precedence: bulk -- so
mail carrying none of those, having matched nothing more particular, was
written by a person. That division is the one that matters in a mailbox,
and headers get it right nearly always.

# The store

One directory for each account, one called local for yours:

    ~/.amimail/
        account
        google/    INBOX.mbox  All_Mail.mbox  Sent_Mail.mbox ...
        local/     Bills.mbox  Family.mbox ...

Each mailbox is mbox: the message exactly as it arrived, headers and
all, with a line before it saying who it is from and when. Any mail
program can read it, and this one can read theirs.

Beside each mailbox are two small files. One remembers how far that
folder has been read, so fetching again takes only what is new. The
other holds a digest of every message in it.

## Digests

Every message carries the SHA-256 of what it is. It is how this program
knows a message it already has, whoever sent it and whatever folder it
arrives in: mail moved between folders keeps its digest, mail fetched
twice has the same one, and the same mail from a second server has the
same one. A message already here is not written again.

The account file holds a password and is written so that only its owner
can read it.

# What is not here yet

Sending. Config/Check Sending proves an account could send -- it
connects, says hello, logs in and says goodbye -- so that the sending
half has somewhere to stand when it is written. Nothing is sent.

Deleting from a server. That is the one change to a server this program
will make, and it will not be made until the mail here has been checked
against what is there, message by message.
