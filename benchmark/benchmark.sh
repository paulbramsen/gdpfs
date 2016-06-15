#!/bin/bash

#
#  ----- BEGIN LICENSE BLOCK -----
#  GDPFS: Global Data Plane File System
#  From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#  Copyright (c) 2016, Regents of the University of California.
#  Copyright (c) 2016, Paul Bramsen, Sam Kumar, and Andrew Chen
#  All rights reserved.
#
#  Permission is hereby granted, without written agreement and without
#  license or royalty fees, to use, copy, modify, and distribute this
#  software and its documentation for any purpose, provided that the above
#  copyright notice and the following two paragraphs appear in all copies
#  of this software.
#
#  IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
#  SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
#  PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
#  EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#  REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#  FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
#  IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
#  OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
#  OR MODIFICATIONS.
#  ----- END LICENSE BLOCK -----
#

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


if [ -z $2 ] || [ $2 = "cptar" ]; then
    timed "cp -r $FILE ."
    timed "tar zxf $DIR.tar.gz"
fi
if [ -z $2 ] || [ $2 = "make" ]; then
    cd $DIR
    timed "make"
fi
echo "TIME IS: $accum seconds"
