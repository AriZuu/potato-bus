Potato Bus - a simple MQTT client for Pico]OS
=============================================

This is a simple MQTT client for Pico]OS + LwIP.
Currently it supports only QOS 0, as my need was
to publish/subscribe data from sensors to/from embedded
devices. If the message is lost, there will be the next
update from sensor.

It should be simple to add missing features if necessary,
maybe I'll do it some day.

Library is split into two layers. Lower one is a packet
reader/writer without dependencies to Pico]OS, so it should
be usable for any envrionment. Higher layer is actual
client, which uses BSD socket layer provided by picoos-lwip 
library.

For simple application example, see example.c

I use mostly JSON as MQTT message format. To handle that
library includes a simple JSON parser/generator which does 
not require use of dynamic memory allocation.

API for MQTT and JSON modules is documented [here][1].

There is another JSON parser included in microjson subdirectory.
I found the parser from [here][2]. It has been modified to
optionally skip unrecognized attributes. There is also
some support for generating json.

(Why 'potato' ? Well, I live in Lemi, Finland. There
is a lot of potato business here)

[1]: http://arizuu.github.io/potato-bus
[2]: http://www.catb.org/esr/microjson/

