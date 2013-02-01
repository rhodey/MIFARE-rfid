MIFARE-rfid
===========

A small linux daemon that can interface with the YHY638A RFID reader/writer
and program ISO14443 MIFARE Classic tags. This daemon was originally written
for a very specific application, I will be working to adapt it for general
use in the near future.

The YHY638A can be found online from many wholesale websites such as
Aliexpress for about $30 a piece. It is not always listed by name but its
appearance is distinct enough that you should have no trouble finding it.
The manufacturer claims that it comes with an "SDK", do not be fooled they
only provide you with a set of windows DLLs. I found this out the hard way
and from that discovery this software was born.

Believe it or not, but the entire serial communication protocol between
the host PC and the USB reader/writer was reversed using only the dated
Windows utility Portmon. I was able to sniff the communictation between
the YHY638A and the manufacturers example Windows application and then
simply studied the captures in notepad++. The RFID reader does almost
zero processing, all the handshaking and MIFARE "encryption" is handled
completely by the daemon.

http://en.wikipedia.org/wiki/ISO/IEC_14443
http://en.wikipedia.org/wiki/MIFARE#MIFARE_Classic

A lesson in bad research practices: It wasn't until I was 95% of the way
through writing this daemon that I discovered I was using MIFARE Classic
tags and MIFARE Classic had already been reverse-engineered.
