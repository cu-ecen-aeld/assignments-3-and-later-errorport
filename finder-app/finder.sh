#!/bin/bash

filesdir=""
searchstr=""

if [ $# -lt 2 ]
then
    echo "Missing arguments! Usage: $0 <filesdir> <searchstr>"
    exit 1
else
    filesdir=$1
    searchstr=$2
fi

if [ ! -d $filesdir ]
then
    echo "The provided path [${filesdir}] does not point to a folder!"
    exit 1
else
    files_count=$(find ${filesdir} -type f | wc -l)
    matches_count=$(grep "${searchstr}" $(find ${filesdir} ) | wc -l)
    echo "The number of files are ${files_count} and the number of matching lines are ${matches_count}"
fi

