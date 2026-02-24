#!/bin/sh
if [ "$#" -lt 2 ]; then
    echo "usage: finder.sh <filesdir> <searchstr>"
    exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d "$filesdir" ]; then
    echo "filesdir is not a directory"
    exit 1
fi

X=$(find "$filesdir" -mindepth 1 -maxdepth 1 | wc -l)
Y=$(grep -R $searchstr $filesdir | wc -l)
echo "The number of files are $X and the number of matching lines are $Y"
