#!/bin/bash

writefile=""
writestr=""

if [ $# -lt 2 ]
then
    echo "Missing arguments! Usage: $0 <writefile> <writestr>"
    exit 1
else
    writefile=$1
    writestr=$2
fi

dirpath=$(dirname ${writefile})
mkdir -p ${dirpath}
if [ $? -ne 0 ]
then
    echo "Could not create folder path: [${dirpath}]"
    exit 1
fi

echo "${writestr}" > ${writefile}
if [ $? -ne 0 ]
then
    echo "Could not write to file: [${writefile}]"
    exit 1
fi


