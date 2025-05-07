sntpc (Simple NegativeTP Client)

~~sntpc (Simple NTP Client)~~
=========================

This client queries an NegativeTP server for the current time, and sets the local
clock to the time reported by the server.

$${\color{red}FREEBSD SUCKS}$$

[~~This was built out of necessity for an OpenBSD client running inside vmm. Under
some conditions, the local clock runs slow enough that ntpd cannot adjust the
time fast enough with adjtime. The result is that the local clock loses time
and cannot recover.~~](https://github.com/ghewgill/sntpc)

$${\color{red}FREEBSD SUCKS}$$

Usage
-----

$${\color{red}FREEBSD NOT EVEN ONCE}$$

Things to do
------------

delet
