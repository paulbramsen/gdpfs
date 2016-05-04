#!/bin/bash

# USAGE: ./copyAndCompile.sh in fuse directory with first argument 
# being directory of gdpfs to compile

TIMER='/usr/bin/time -f %e'

if [ -z $1 ]; then
    echo "USAGE: $0 <test>"
    echo -e "\tmust be run in root of fuse mounted directory"
    echo -e "\t<test>: gdpfs, redis, or sqlite"
    exit
fi

FILELOC=$(dirname $0)
if [ FILELOC = "." ]; then
    FILELOC=$(which $0)
    FILELOC=$(dirname $FILELOC)
fi

if [ $1 = "gdpfs" ]; then
    DIR="gdpfs"
elif [ $1 = "redis" ]; then
    DIR="redis-3.2"
elif [ $1 = "oltp" ]; then
    DIR="oltpbench"
elif [ $1 = "sqlite" ]; then
    DIR="sqlite-autoconf-3120200"
else
    echo "Unknown test $1"
    exit 1
fi

FILE=$FILELOC/$DIR.tar.gz

if [ ! -z "`ls gdpfs > /dev/null 2> /dev/null`" ]; then
    echo "Need to delete gdpfs directory first"
    exit
fi

accum=0.0
function timed() {
    echo "Executing $1"
    output=`$TIMER $1 2>&1 >/dev/tty`
    new=`echo $output | tail -n 1 | rev | cut -d ' ' -f 1 | rev`
    echo "Took $new seconds"
    accum=`awk "BEGIN{print $new+$accum}"`
}

if [ $1 = "oltp" ]; then
    timed "cp -r $FILE ."
    timed "tar zxf $DIR.tar.gz"
    cd $DIR
    timed "./oltpbenchmark -b tpcc -c config/sample_tpcc_config.xml --create=true --load=true --execute=true -s 5 -o outputfile"
else
    timed "cp -r $FILE ."
    timed "tar zxf $DIR.tar.gz"
    cd $DIR
    timed "make"
    echo "TIME IS: $accum seconds"
fi
