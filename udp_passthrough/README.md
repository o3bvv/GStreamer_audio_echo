GStreamer UDP passthrough
----------------------------

This example shows how to make a passthrough audio stream from audio input to audo output using udp-sockets on localhost.
Audio data flows from server to client. These last can run localhost. They can be placed on separate workstations as well.
In this case the only thing you'll have to do is to change client's host address in server's settings.

It's equal to command-line commands:<br>
In **1st** terminal (server):

    $ gst-launch autoaudiosrc ! 'audio/x-raw-int, rate=(int)22050, depth=(int)16, endianness=(int)4321, channels=(int)1', signed=(boolean)true ! udpsink host=127.0.0.1 port=9559

In **2nd** terminal (client):

    $ gst-launch udpsrc port=9559 ! 'audio/x-raw-int, rate=(int)22050, depth=(int)16, endianness=(int)4321, channels=(int)1', signed=(boolean)true ! autoaudiosink

**Note**:<br>
You can use *gst-launch-0.10* (or something like that) instead of *gst-launch* if it's not found. Autocomplete will help you.
