#!/bin/bash

#Exit if invoked with other than 2 arguments 
if [[ "$#" -ne 2 ]]
then
	echo "ERROR: Invalid number of arguments."
	echo "Total number of arguments should be 2."
	echo "The order of the arguments should be:"
	echo "	1)File directory path."
	echo "	2)Search string."
	exit 1
fi

filesdir=$1
searchstr=$2

num_files=$(find "${filesdir}/" -type f | wc -l)

if [[ -d "${filesdir}" ]]
then
	#grep -nr "${searchstr}" "${filesdir}"
	num_matching_lines=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)
else
	echo "It is not a directory"
	exit 1
fi

if [[ "$?" -eq 0 ]]
then
	echo "Success"
	echo "The number of files are ${num_files} and the number of matching lines are ${num_matching_lines}"
	exit 0
else
	echo "Failed: Expected ${searchstr} in ${filesdir} but none found"
	exit 1
fi
