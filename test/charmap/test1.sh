#!/bin/sh
# $Id: test1.sh,v 1.3.2.1 2005-03-11 21:10:12 adam Exp $

pp=${srcdir:-"."}

LOG=test1.log
rm -f $LOG
if ../../index/zebraidx -c $pp/zebra.cfg -l $LOG -V|grep Expat >/dev/null; then
	../../index/zebraidx -c $pp/zebra.cfg -l$LOG init
else
	exit 0
fi
../../index/zebraidx -c $pp/zebra.cfg -l$LOG update $pp/*.xml
../../index/zebrasrv -c $pp/zebra.cfg -l$LOG unix:socket &
sleep 1
# ae
../api/testclient unix:socket '@term string æ' >tmp1
echo 'Result count: 1' >tmp2
kill `cat zebrasrv.pid` || exit 1
diff tmp1 tmp2 || exit 2
rm -f tmp1 tmp2
