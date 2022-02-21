#!/bin/bash
INFILE=$1.txt
OUTFILE=$1.c
TABNAME=$1
SIZE=128

rm -f ${OUTFILE}
echo "const unsigned int ${TABNAME}[${SIZE}] = {" >> ${OUTFILE}
sed  '$ ! {s|$|,|g}' ${INFILE} >> ${OUTFILE}
echo "};" >> ${OUTFILE}


