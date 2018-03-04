fhz2mqtt
========

A FHZ to MQTT bridge.

Supported devices
-----------------

#### FHT 80 b
Example MQTT packets:

    -> /fhz/set/fht/9601/mode auto
    <- /fhz/fht/9601/ack/mode auto
    -> /fhz/set/fht/9601/mode manual
    <- /fhz/fht/9601/ack/mode manual
    <- /fhz/fht/9601/status/is-valve 0%
    <- /fhz/fht/9601/status/is-valve 0%
    <- /fhz/fht/9601/status/is-valve 0%
    <- /fhz/fht/9601/status/is-temp 24.3
    <- /fhz/fht/9601/status/window open
    -> /fhz/set/fht/9601/desired-temp 25
    <- /fhz/fht/9601/ack/desired-temp 25.0
    <- /fhz/fht/9601/status/is-valve 4%
    <- /fhz/fht/9601/status/is-valve 67%
    <- /fhz/fht/9601/status/is-temp 22.80
    <- /fhz/fht/9601/status/window close
    <- /fhz/fht/9601/status/battery ok
