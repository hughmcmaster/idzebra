#!/bin/sh
if test ! -f demo/zebra.cfg; then
	echo "Expected to find demo/zebra.cfg"
	exit 1
fi
test -d demo/register || mkdir demo/register
test -d demo/lock || mkdir demo/lock
test -d demo/tmp || mkdir demo/tmp
test -d demo/shadow || mkdir demo/shadow

../index/zebraidx -c demo/zebra.cfg init
../index/zebraidx -c demo/zebra.cfg -g demo1 -n update /usr/lib/perl5
