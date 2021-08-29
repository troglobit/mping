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
  mping -r|-s [-v] [-i IFNAME] [-a GROUP] [-p PORT] [-t TTL]

Options:
  -r, -s       Receiver or sender. Required argument, mutually exclusive
  -i IFNAME    Interface to use for sending/receiving
  -a GROUP     Multicast group to listen/send on, default 239.255.255.1
  -p PORT      Multicast port to listen/send on, default 10000
  -t TTL       Multicast time to live to send, default 1
  -?, -h       This help text
  -v           Verbose mode
  -V           Display version
```

Origin
------

Initially based on an example program from the book "Multicast Sockets",
but since then completely rewritten and extended.
