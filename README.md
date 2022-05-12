a simple multicast ping program
===============================

mping aspires to be an easy to use and script friendly program with
support for both IPv4 and IPv6.  Similar to the standard ping program,
but unlike it, the response to multicast ping is sent by another mping.

```
      mping --> Hello!
     .----.                  ._         Hi! <-- mping
     |    |               .-(`  )               .----.
     |    |--------------:(      ))             |    |
     '----'              `(    )  ))------------|    |
     Host A               ` __.:'               '----'
	                                            Host B
                              ^
                              |
Multicast friendly network ---'
```

> A multicast "friendly" network is one that supports multicast routing,
> and/or supports IGMP/MLD snooping.  Remember: the reverse path is in
> the unicast routing table (e.g., default route), the MULTICAST flag
> on your interfaces, and ... the TTL in routed networks!

By default, mping starts in receiver mode, joining the group given as
command line argument.  To start as a sender, use `-s` and remember to
set the `-t TTL` value greater than the number of routing "hops" when
testing in a routed topology.


Usage
-----

```
Usage:
  mping [-6dhqrsv] [-c COUNT] [-i IFNAME] [-p PORT] [-t TTL] [-w SEC] [-W SEC] [GROUP]

Options:
  -6          Use IPv6 instead of IPv4, see below for defaults
  -c COUNT    Stop after sending/receiving COUNT packets
  -d          Debug messages
  -h          This help text
  -i IFNAME   Interface to use for sending/receiving
  -p PORT     Multicast port to listen/send to, default 4321
  -q          Quiet output, only startup and and summary lines
  -r          Receiver mode, default
  -s          Sender mode
  -t TTL      Multicast time to live to send, IPv6 hops, default 1
  -v          Show program version and contact information
  -w DEADLINE Timeout before exiting, waiting for COUNT replies
  -W TIMEOUT  Time to wait for a response, in seconds, default 5

Defaults to use multicast group 225.1.2.3, UDP dst port 4321, unless -6 in which
case a multicast group ff2e::42 is used.  When a group argument is given, the
address family is chosen from that.  The selected outbound interface is chosen
by querying the routing table, unless -i IFNAME
```

> **Note:** the `mping` receiver (currently) also needs to set the TTL
> value, this is crucial in a routed setup or the reply is dropped.


Origin
------

Initially based on an example program from the book "Multicast Sockets",
but since then completely rewritten and extended.  Please send any bug
reports, feature requests, patches/pull requests, and documentation
fixes to [GitHub](https://github.com/troglobit/mping).
