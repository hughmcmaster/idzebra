#!/bin/sh
LOG=test1.log
../../index/zebraidx -l $LOG init
../../index/zebraidx -l $LOG update f1.xml
