#!/bin/bash

if [[ "$#" -ne 2 ]]
then
	echo "ERROR: Invalid number of arguments."
	echo "Total number of arguments should be 2."
	echo "The order of the arguments should be:"
	echo "	1)File  path."
	echo "	2)Content string."
	exit 1
fi

writefile=$1
writestr=$2

path=$(dirname "${writefile}")

mkdir -p "${path}"
echo "${writestr}" > "${writefile}"

if [[ "$?" -eq 0 ]]
then
	echo "Success"
	exit 0
else
	echo "ERROR: File could not be created"
	exit 1
fi
