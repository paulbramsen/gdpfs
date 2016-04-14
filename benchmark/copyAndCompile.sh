#!/bin/bash

# USAGE: ./copyAndCompile.sh in fuse directory with first argument 
# being directory of gdpfs to compile

TIMER='/usr/bin/time -f %e'

if [ -z $1 ]; then
    echo "USAGE: ./copyAndCompile.sh <GDPFS_DIR>"
    echo -e "\tmust be run in root of fuse mounted directory"
    echo -e "\t<GDPFS_DIR>: directory to make"
    exit
fi
DIR=$1

if [ ! -z "`ls gdpfs`" ]; then
    echo "Need to delete gdpfs directory first"
    exit
fi

accum=0.0
function timed() {
    new=`$TIMER $1 2>&1>/dev/null`
    accum=`awk 'BEGIN{print '$new'+'$accum'}'`
    echo $accum
}

timed "cp -r $DIR ."
cd gdpfs
timed "make"
echo "TIME IS: $accum seconds"
