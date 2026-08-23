"""Check the chapter banners of doc/ami.odt against the three rules they follow.

Every chapter opens with a banner: the blue box carrying the chapter number,
its title, and the cat. The rules are

  1. every chapter has one,
  2. the title keeps clear of the cat, which sits inside the box, and
  3. the chapter starts on an odd page, so it opens on a right hand leaf.

Rules 1 and 3 hold by themselves as long as the heading carries the Chapter
Banner style: that style names the ChapterStart page, whose layout is right
only, so LibreOffice breaks to the next odd page and inserts a blank leaf when
it has to. Rule 2 holds because the cat's frame wraps text to its left. The
point of this program is to prove all three of a rendered book rather than
trust them.

Usage:

    python3 doc/checkbanners.py [doc/ami.pdf]

With no argument the book is rendered from doc/ami.odt into a temporary
directory first, which wants soffice. Exits nonzero if any rule is broken.
"""
import os, re, subprocess, sys, tempfile

ODT = "doc/ami.odt"

# The cat's left edge, in points from the left of the page. The text column runs
# from the 1.0425in margin for 6.4146in, and the frame is 1.7398in wide against
# its right edge, which puts the cat's left edge here. A title word reaching
# past it is a title printed over the cat.
CATLEFT = 406.0

# the chapters, in the order they appear
TITLES = [
    "Introduction: What is Ami", "Using C++ With Ami", "System Services Library",
    "Terminal Interface Library", "Character Windows Management Library",
    "Graphical Interface Library", "Windows Management Library", "Widgets Library",
    "Sound Library", "Networking Library", "Option: Command Line Option Processing",
    "Config: The configuration database", "Print modules", "Remote mode",
    "The remote display protocol", "Widget Creation", "Sound module plugins",
    "Example Applications", "Libc and alternatives", "Directory layout",
    "Installing and building Ami", "Testing Ami", "Windows Specific Details",
    "Linux Specific details", "Mac OS X specific details", "License",
]

def render(tmp):
    """the book as a pdf, from the odt"""
    subprocess.run(["soffice", "--headless", "--convert-to", "pdf",
                    "--outdir", tmp, ODT], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return os.path.join(tmp, "ami.pdf")

def pages(pdf):
    """the text of each page, and the words of each page with their boxes"""
    txt = subprocess.run(["pdftotext", pdf, "-"], check=True,
                         capture_output=True).stdout.decode("utf8", "replace")
    box = subprocess.run(["pdftotext", "-bbox", pdf, "-"], check=True,
                         capture_output=True).stdout.decode("utf8", "replace")
    words = [re.findall(r'<word xMin="([\d.]+)" yMin="([\d.]+)" '
                        r'xMax="([\d.]+)" yMax="([\d.]+)">([^<]*)</word>', p)
             for p in re.findall(r'<page [^>]*>(.*?)</page>', box, re.S)]
    return txt.split("\f"), words

def printed(page):
    """the page number the page prints, which is the last number standing alone"""
    n = re.findall(r'(?m)^\s*(\d{1,4})\s*$', page)
    return int(n[-1]) if n else None

def main():
    tmp = None
    if len(sys.argv) > 1: pdf = sys.argv[1]
    else:
        tmp = tempfile.mkdtemp()
        pdf = render(tmp)
    text, words = pages(pdf)
    faults = 0
    for n, title in enumerate(TITLES, 1):
        opening = " ".join(title.split()[:2])
        found = [i for i, p in enumerate(text)
                 if re.search(r'(?m)^%d %s' % (n, re.escape(opening)), p)]
        if not found:
            print("%2d  %-40s no chapter page" % (n, title[:40])); faults += 1
            continue
        i = found[-1]
        page = printed(text[i])
        # the banner is the large text on the page
        big = [(float(a), float(b), float(c), float(d), w)
               for a, b, c, d, w in words[i] if float(d)-float(b) > 14]
        if not big:
            print("%2d  %-40s no banner" % (n, title[:40])); faults += 1
            continue
        over = [w for a, b, c, d, w in big if c > CATLEFT]
        note = ""
        if over: note += "  over the cat: " + " ".join(over)
        if page is None: note += "  no page number"
        elif page % 2 == 0: note += "  starts on an even page"
        if note: faults += 1
        print("%2d  %-40s page %-5s right %5.1f%s"
              % (n, title[:40], page, max(x[2] for x in big), note))
    print()
    print("%d chapters, %d faults" % (len(TITLES), faults))
    if tmp: subprocess.run(["rm", "-rf", tmp])
    return 1 if faults else 0

if __name__ == "__main__":
    sys.exit(main())
