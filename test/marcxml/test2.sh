#!/bin/sh
# $Id: test2.sh,v 1.3 2004-06-15 09:43:33 adam Exp $

pp=${srcdir:-"."}

LOG=test2.log
rm -f $LOG
../../index/zebraidx -c $pp/zebra.cfg -l$LOG init
../../index/zebraidx -c $pp/zebra.cfg -l$LOG -t grs.marcxml.record update $pp/sample-marc
../../index/zebrasrv -c $pp/zebra.cfg -l$LOG unix:socket &
sleep 1
../api/testclient unix:socket '@and @attr 1=1003 jack @attr 1=4 computer' >tmp1
echo 'Result count: 2' >tmp2
kill `cat zebrasrv.pid` || exit 1
diff tmp1 tmp2 || exit 2
rm -f tmp1 tmp2