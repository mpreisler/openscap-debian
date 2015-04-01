#!/bin/bash

set -e
set -o pipefail

name=$(basename $0 .sh)
result=$(mktemp -t ${name}.out.XXXXXX)
stderr=$(mktemp -t ${name}.out.XXXXXX)
echo "Stderr file = $stderr"
echo "Result file = $result"

line1='^\W*part /var$'

$OSCAP xccdf generate fix --template urn:redhat:anaconda:pre \
	--output $result $srcdir/${name}.xccdf.xml 2>&1 > $stderr
[ -f $stderr ]; [ ! -s $stderr ]; :> $stderr
grep "$line1" $result
[ "`grep -v "$line1" $result | sed 's/\W//g'`"x == x ]
rm $result
