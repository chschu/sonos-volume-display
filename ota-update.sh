#!/bin/bash -e

host=$1
binfile=$2

if [ -z "${host}" -o -z "${binfile}" ]
then
	echo "Syntax: $0 <host> <binfile>"
	exit 1
fi

espota.py -d -i "${host}" -f "${binfile}"
