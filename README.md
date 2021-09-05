a simple multicast ping program
===============================

mping intends to be an easy to use and script friendly program.  Similar
to the standard ping program, but unlike it, the response to multicast
ping is sent by another mping.

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
  mping [-dhqrsv] [-c COUNT] [-i IFNAME] [-p PORT] [-t TTL] [-w SEC] [-W SEC] [GROUP]

Options:
  -c COUNT    Stop after sending/receiving COUNT packets
  -d          Debug messages
  -h          This help text
  -i IFNAME   Interface to use for sending/receiving
  -p PORT     Multicast port to listen/send to, default 4321
  -q          Quiet output, only startup and and summary lines
  -r          Receiver mode, default
  -s          Sender mode
  -t TTL      Multicast time to live to send, default 1
  -v          Show program version and contact information
  -w DEADLINE Timeout before exiting, waiting for COUNT replies
  -W TIMEOUT  Time to wait for a response, in seconds, default 5

Defaults to use multicast group 225.1.2.3, UDP dst port 4321, outbound
interface is chosen by the routing table, unless -i IFNAME
```

Origin
------

Initially based on an example program from the book "Multicast Sockets",
but since then completely rewritten and extended.  Please send any bug
reports, feature requests, patches/pull requests, and documentation
fixes to [GitHub](https://github.com/troglobit/mping).
