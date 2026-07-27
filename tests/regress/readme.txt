This directory is the run sandbox for bin/regress. services_test lists the
current directory, so it runs here, where the contents are fixed. The lib
symlink lets the test binaries find the Petit-Ami shared libraries, which
are loaded relative to the current directory.
