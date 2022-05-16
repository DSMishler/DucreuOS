# code originally by chris muncie
#!/bin/bash

# Make sure there is an argument
if [ $# -lt 2 ]
then
	echo "Usage: $0 [-{csf}] (string/file)"
	exit 1
fi

# .dsk is arbitrary, but I plan to use it
# so this should prevent me from murdering some other
# file on accident
if [ "${1##*.}" != "dsk" ]
then
	echo "$1 is not a dsk file, aborting"
	exit 1
fi

# If the second argument was -c, clear the entire disk
if [ "$2" == "-c" ]
then
	# Get the length and then write that many bytes from
	# /dev/zero. notrunc does nothing here, but whatever
	len=$(du -b $1 | awk '{printf "%d\n", $1}')
	dd if=<(head -c $len /dev/zero) of=$1 conv=notrunc
	exit 0
fi

# If second argument was -s, copy the thrid argument
# into the disk as a string, tack on a newline
if [ "$2" == "-s" ]
then
	if [ $# -lt 3 ]
	then
		echo "Usage: $0 [-{csf}] (string/file)"
		exit 1
	fi
	dd if=<(echo $3) of=$1 conv=notrunc
	exit 0
fi

# If second argument was -f, copy a file into the disk
# get filename from third argument
if [ "$2" == "-f" ]
then
	if [ $# -lt 3 ]
	then
		echo "Usage: $0 [-{csf}] (string/file)"
		exit 1
	fi
	len=$(du -b $3 | awk '{printf "%d\n", $1}')
	dd if=<(head -c $len $3) of=$1 conv=notrunc
	exit 0
fi
