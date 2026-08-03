# Entering data

Type into the sheet and what you type appears in the cell you are on,
with the entry marked so you can see it is not down yet. Nothing is
changed in the sheet until the entry is put down.

Enter puts the entry down and steps to the row below. Tab puts it down
and steps to the column right. Escape abandons the entry and leaves the
cell as it was.

Backspace takes back the last character of an entry. On a cell with no
entry under way it clears the cell instead, which is the quick way to
empty one cell. Delete clears the cell without starting an entry at all.

A cell holds what you typed, and that is what is saved. What the cell
shows may be something else: a formula shows its result, and a number
too wide for its column shows a row of marks rather than some of its
digits. Widening the columns shows the number again.

## What counts as a number

Anything the program can read as a number is one, and numbers are shown
against the right of their cell. Everything else is text, and text is
shown against the left. That is the only difference the program makes
between them, and it is made when the entry is put down, not later.

# Formulas

A cell whose contents begin with = is a formula, and what the cell shows
is the result of working it out. The formula itself is shown while the
cell is being entered, and again whenever the cell is entered afresh.

Formulas take:

- the four operators + - * /
- parentheses, to any depth
- numbers, written as you would write them
- the names of other cells, as A1 or BC120
- the functions, which are their own topic

So =A1+B1*2 is a formula, and so is =(A1+B1)/2, and multiplication binds
tighter than addition as it should.

A formula that cannot be worked out leaves its cell showing an error
mark rather than a wrong number. That is deliberate: a sheet that shows
a number is saying that number is right.

## Formulas that refer to each other

A formula may name a cell that is itself a formula, to any depth, and
they are worked out in the order that need requires rather than the
order they appear in. A formula that comes back round to itself, whether
directly or through a chain of others, is stopped rather than followed
forever, and every cell in the loop shows an error.

# Functions

SUM, AVG, MIN, MAX and COUNT each take a range and give one number.

- SUM adds every number in the range
- AVG averages them
- MIN and MAX give the smallest and the largest
- COUNT counts how many cells in the range hold numbers

So =SUM(B1:B10) adds the column, =AVG(B1:B10) averages it, and
=COUNT(B1:B10) says how many of those ten cells were filled in.

Cells in the range that are empty, or that hold text, are passed over
rather than counted as zero. This matters: the average of a column with
gaps in it is the average of what is there.

A function may stand anywhere a number may. =SUM(B1:B10)/12 is a formula
like any other, and so is =MAX(A1:A10)-MIN(A1:A10).

# Ranges

A range is two cell names with a colon between them, as B1:B10 or
A1:D20. It covers the rectangle that has those two cells at opposite
corners, so A1:D20 is four columns by twenty rows, which is eighty
cells, not two.

The corners may be given either way round: A1:D20 and D20:A1 are the
same range.

A range is not a value and cannot be used as one. It is only ever an
argument to a function.

# Moving about

The arrow keys move the current cell one at a time, and the sheet
follows when the cell would otherwise leave it. Page up and page down
move a screen at a time.

The scroll bars and the mouse wheel move the sheet without moving the
current cell. That is how to look somewhere else without losing your
place: the current cell stays where it was, and the arrow keys bring the
sheet back to it.

A click puts the current cell where it lands.

# Column width

Sheet/Wider Columns and Sheet/Narrower Columns change the width of every
column at once, from five to twenty digits of the display font. Columns
are all the same width; there is no setting one column alone.

A number too wide for its column is shown as a row of marks. This is not
a loss: the cell still holds the number, and widening the columns shows
it. Text is not held to its column at all, which is the next topic.

# Text past a cell

Text longer than its column prints on across the cells to the right of
it, as long as they are empty. That is what makes titles and labels
readable on a sheet whose columns are sized for numbers.

The first cell to the right that holds something stops it, and the text
is cut there. Nothing is lost by this either: the cell still holds all
of what was typed. Widening the columns, or clearing the cell that stops
it, shows more of it.

Where text crosses a cell edge it takes the line with it, as a printed
sheet does. The lines of the cells beyond where the text reaches are
left alone.

# Recalculation

Every formula in the sheet is worked out again whenever the sheet
changes, so there is normally nothing to do about it. A sheet on screen
is always a sheet that has been worked out.

Sheet/Recalculate forces it, which is worth having when a sheet has been
loaded from a file written by another program and you want to see this
program's own reading of the formulas in it.

# Files

File/Open and File/Save read and write Open Document spreadsheets, the
.ods files of OpenOffice and LibreOffice, which is ISO/IEC 26300.

Formulas are saved as formulas rather than as the numbers they last
worked out to, so a sheet written here opens in those programs with its
formulas live, and one written by them opens here the same way.

File/New starts an empty sheet and forgets the file name, so the next
save asks where to put it. File/Save As always asks.

## What is not saved

Column width, the current cell, and where the sheet is scrolled to are
this program's own and are not written to the file. Anything the file
holds that this program has no use for -- fonts, colors, borders, more
than one sheet in the book -- is not read and is not written back, so
saving a file from another program here will lose it. Work on a copy.

# Finding a cell

Edit/Find looks through the sheet for text and makes the first cell
holding it the current cell. It searches on from the current cell and
comes round to it, so repeating the find walks through every cell that
matches.

Edit/Go To takes a cell name, as B12, and goes straight there. On a
sheet of any size this beats scrolling.

# Limits

The sheet is seventy eight columns, A through BZ, by a thousand rows,
and a cell holds up to eighty characters.

These are fixed. The window shows what fits and the scroll bars move it
over the rest. Nothing is allocated for a cell until the cell is used,
so an empty sheet of this size costs nothing to have.

# About this help

The text you are reading is not built into the program. It is a file,
spreadsheet.md, written in the plain part of markdown: a line beginning
with a single # names a topic, and everything after it belongs to that
topic until the next one or the end of the file.

That means the help can be rewritten, corrected or translated without
touching the program, which is the point of doing it this way. Within a
topic, a blank line separates paragraphs, a line beginning with ## is a
heading inside the topic, and a line beginning with - is an item in a
list. Everything else is ordinary text, wrapped to the width of the
window.

The window itself is a plain window of the program's own, not a dialog
box. Every event in this system names the window it came from, so the
program's main loop tells this window's events from the sheet's and
hands them to one routine. There is nothing else to it, which is why a
program of this size can afford a real help window.
