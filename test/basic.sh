#!/bin/sh

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

print "Creating world ..."
ip link set lo up
ip link set lo multicast on
echo "Links:"
ip -br l | awk '$0="  "$0'
echo "Addresses:"
ip -br a | awk '$0="  "$0'

print "Phase 1: Verify too few pings ..."
../mping -r -c 2 -i lo &
PID=$!
sleep 1

../mping -s -c 3 -i lo -W 2
rc=$?

kill -9 $PID 2>/dev/null
[ $rc -eq 0 ] && FAIL
echo

print "Phase 2: Verify successful ping ..."
../mping -qr -c 3 -i lo &
PID=$!
sleep 1

../mping -qs -c 3 -i lo -W 3
rc=$?

kill -9 $PID 2>/dev/null
[ $rc -ne 0 ] && FAIL
OK
