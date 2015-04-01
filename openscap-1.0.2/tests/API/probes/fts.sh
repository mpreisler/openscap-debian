#!/bin/bash
#
# Copyright 2011 Red Hat Inc., Durham, North Carolina.
# All Rights Reserved.
#
# Authors:
#      Daniel Kopecek <dkopecek@redhat.com>
#      Tomas Heinrich <theinric@redhat.com>

function gen_tree {
	echo "Generating tree for traversal" >&2

	mkdir -p $ROOT/{d1/{d11/d111,d12},d2/d21}
	touch $ROOT/{d1/{d11/{d111/f1111,f111,f112,f113},d12/f121,f11},d2/{d21/f211,f21}}
}

function oval_fts {
	echo "=== $1 ==="
	shift
	./oval_fts_list "$@" | sort | tee ${tmpdir}/oval_fts_list.out | \
		sed "s|${ROOT}/||" | tr '\n' ',' > ${tmpdir}/oval_fts_list.out2
	if [ $? -ne 0 ]; then
		echo "oval_fts_list failed"
		return 2
	fi

	shift 4
	echo -e "expected result:\n$1\noval_fts_list.out2:"
	cat ${tmpdir}/oval_fts_list.out2
	echo
	if [ "$(cat ${tmpdir}/oval_fts_list.out2 | openssl md5)" == \
		"$(echo -n $1 | openssl md5)" ]; then
		return 0
	else
		return 1
	fi
}

set -e -o pipefail

name=$(basename $0 .sh)
tmpdir=$(mktemp -t -d "${name}.XXXXXX")
ROOT=${tmpdir}/ftsroot
echo "Temp dir: ${tmpdir}."
gen_tree $ROOT

while read args; do
	[ -z "${args%%#*}" ] && continue
	eval oval_fts $args
done <<EOF
test1 \
'' '' \
'((filepath :operation 5) "'$ROOT'/d1/d12/f121")' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
d1/d12/f121,

test2 \
'' '' \
'((filepath :operation 11) "^'$ROOT'/d1/.*/f1111")' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
d1/d11/d111/f1111,

test3 \
'((path :operation 5) "'$ROOT'/d2")' \
'((filename :operation 5) "f21")' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
d2/f21,

test4 \
'((path :operation 5) "'$ROOT'/d1/d11")' \
'((filename :operation 11) "^f11[23]$")' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
d1/d11/f112,d1/d11/f113,

test5 \
'((path :operation 11) "^'$ROOT'/d1/d1[12]$")' \
'((filename :operation 11) "^f..1$")' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
d1/d11/f111,d1/d12/f121,

test6 \
'((path :operation 11) "^'$ROOT'/d1/.*")' \
'((filename :operation 5) "f1111")' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
d1/d11/d111/f1111,

test7 \
'((path :operation 5) "'$ROOT'/d1")' \
'((filename :operation 5) "f112")' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "down" :recurse_file_system "all"))' \
d1/d11/f112,

test8 \
'((path :operation 5) "'$ROOT'/d1")' \
'((filename :operation 11) "^f.*1$")' \
'' \
'((behaviors :max_depth "1" :recurse "symlinks and directories" :recurse_direction "down" :recurse_file_system "all"))' \
d1/d11/f111,d1/d12/f121,d1/f11,

test9 \
'((path :operation 5) "'$ROOT'/d1/d11/d111")' \
'((filename :operation 5) "f11")' \
'' \
'((behaviors :max_depth "3" :recurse "symlinks and directories" :recurse_direction "up" :recurse_file_system "all"))' \
d1/f11,

test10 \
'((path :operation 5) "'$ROOT'/d2")' \
'((filename :operation 11) "^f21.*$")' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "down" :recurse_file_system "all"))' \
d2/d21/f211,d2/f21,

test11 \
'((path :operation 5) "'$ROOT'/d2")' \
'((filename :operation 11) "^f21.*$")' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks" :recurse_direction "down" :recurse_file_system "all"))' \
d2/f21,

test12 \
'((path :operation 5) "'$ROOT'/d1")' \
'((filename :operation 5))' \
'' \
'((behaviors :max_depth "0" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
d1/,

test13 \
'((path :operation 5) "'$ROOT'/d1")' \
'((filename :operation 5))' \
'' \
'((behaviors :max_depth "0" :recurse "symlinks and directories" :recurse_direction "down" :recurse_file_system "all"))' \
d1/,

test14 \
'((path :operation 5) "'$ROOT'/d1")' \
'((filename :operation 5))' \
'' \
'((behaviors :max_depth "0" :recurse "symlinks and directories" :recurse_direction "up" :recurse_file_system "all"))' \
d1/,

test15 \
'((path :operation 5) "'$ROOT'/d1")' \
'((filename :operation 5))' \
'' \
'((behaviors :max_depth "1" :recurse "symlinks and directories" :recurse_direction "down" :recurse_file_system "all"))' \
d1/,d1/d11/,d1/d12/,

test16 \
'((path :operation 5) "'$ROOT'/d1/d11")' \
'((filename :operation 5))' \
'' \
'((behaviors :max_depth "1" :recurse "symlinks and directories" :recurse_direction "up" :recurse_file_system "all"))' \
d1/,d1/d11/,

test17 \
'((path :operation 5) "'$ROOT'/d1/d11")' \
'((filename :operation 5))' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "down" :recurse_file_system "all"))' \
d1/d11/,d1/d11/d111/,

# support for empty string as a pattern in 'filename' entity
test18 \
'((path :operation 5) "'$ROOT'/d2")' \
'((filename :operation 11) "")' \
'' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
d2/f21,

# don't return nonexistent filepath
test19 \
'' '' \
'((filepath :operation 5) "/nonexistent")' \
'((behaviors :max_depth "-1" :recurse "symlinks and directories" :recurse_direction "none" :recurse_file_system "all"))' \
# intentionally left blank

# test for a regression caused by the errno check that follows calls to fts_open()
test20 \
'((path :operation 5) "'$ROOT'/d1/d11/d111")' \
'((filename :operation 5) "f1111")' \
'' \
'((behaviors :max_depth "-1" :recurse "directories" :recurse_direction "down" :recurse_file_system "local"))' \
d1/d11/d111/f1111,

EOF

rm -rf $tmpdir
