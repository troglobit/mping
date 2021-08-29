mping: a simple multicast ping program 
======================================

The intention of this program is to be easy to use and script friendly.
Similar to the standard ping program, but unlike unicast ping, which the
TCP/IP stack responds to, the response to a multicast ping must be sent
by a receiving mping.

By default, mping starts in receiver mode, joining the group given as
command line argument.  To start as a sender, use `-s` and remember to
set the `-t TTL` value greater than the number of routing "hops" when
testing in a routed topology.


Usage
-----

```
Usage:
  mping [-hrsvV] [-i IFNAME] [-p PORT] [-t TTL] [GROUP]

Options:
  -h         This help text
  -i IFNAME  Interface to use for sending/receiving
  -p PORT    Multicast port to listen/send on, default 4321
  -r         Receiver mode, default
  -s         Sender mode
  -t TTL     Multicast time to live to send, default 1
  -v         Verbose mode
  -V         Show program version and contact information

Defaults to use multicast group 225.1.2.3, UDP dst port 4321, outbound
interface is chosen by the routing table, unless -i IFNAME
```

Origin
------

Initially based on an example program from the book "Multicast Sockets",
but since then completely rewritten and extended.
