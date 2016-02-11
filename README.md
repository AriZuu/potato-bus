Potato Queue - a simple MQTT client for Pico]OS
===============================================

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

(Why potato ? Well, I live in Lemi, Finland. There
is a lot of potato business here)
