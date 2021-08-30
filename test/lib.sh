#!/bin/sh
# Helper functions for testing SMCRoute

# Test name, used everywhere as /tmp/$NM/foo
NM=$(basename "$0" .sh)

# Print heading for test phases
print()
{
    printf "\e[7m>> %-80s\e[0m\n" "$1"
}

SKIP()
{
    print "TEST: SKIP"
    [ $# -gt 0 ] && echo "$*"
    exit 77
}

FAIL()
{
    print "TEST: FAIL"
    [ $# -gt 0 ] && echo "$*"
    exit 99
}

OK()
{
    print "TEST: OK"
    [ $# -gt 0 ] && echo "$*"
    exit 0
}

check_dep()
{
    if [ -n "$2" ]; then
	if ! $@; then
	    SKIP "$* is not supported on this system."
	fi
    elif ! command -v "$1"; then
	SKIP "Cannot find $1, skipping test."
    fi
}
