#!/bin/sh
# $Id: buildconf.sh,v 1.22 2006-06-28 13:26:13 adam Exp $
if automake --version|head -1 |grep '1\.[4-7]'; then
    echo "automake 1.4-1.7 is active. You should use automake 1.8 or later"
    if test -f /etc/debian_version; then
        echo " sudo apt-get install automake1.9"
        echo " sudo update-alternatives --config automake"
    fi
    exit 1
fi

set -x
# I am tired of underquoted warnings for Tcl macros
aclocal -I m4 2>&1 | grep -v aclocal/tcl.m4
libtoolize --automake --force 
automake -a 
autoconf
set -
if [ -f config.cache ]; then
	rm config.cache
fi

enable_configure=false
enable_help=true
sh_flags=""
conf_flags=""
case $1 in
    -d)
	sh_flags="-g -Wall -O0 -Wdeclaration-after-statement"
	enable_configure=true
	enable_help=false
	shift
	;;
    -c)
	sh_flags=""
	enable_configure=true
	enable_help=false
	shift
	;;
esac

if $enable_configure; then
    if test -n "$sh_flags"; then
	CFLAGS="$sh_flags" ./configure --disable-shared $*
    else
	./configure $*
    fi
fi
if $enable_help; then
    cat <<EOF

Build the Makefiles with the configure command.
  ./configure [--someoption=somevalue ...]

For help on options or configuring run
  ./configure --help

Build and install binaries with the usual
  make
  make check
  make install

Build distribution tarball with
  make dist

Verify distribution tarball with
  make distcheck

Or just build the Debian packages without configuring
  dpkg-buildpackage -rfakeroot

When building from a CVS checkout, you need these Debian packages:
  docbook, docbook-xml, docbook-xsl, xsltproc,
  libyaz-dev, libexpat1-dev, tcl8.4-dev, libbz2-dev
and if you want the Alvis/XSLT filter, you also need:
  libxslt1-dev
EOF
fi
