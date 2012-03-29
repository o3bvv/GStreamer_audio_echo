GStreamer G.726-encoded RTP passthrough
---------------------------------------

This example shows how to make a passthrough audio stream from audio input to
audo output using udp-sockets and **RTP-protocol** on localhost.
**G.726-encoded** audio data flows from server to client. These last can run
localhost. They can be placed on separate workstations as well. In this case the
only thing you'll have to do is to change client's host address in server's
settings.

It's equal to terminal commands:<br>
In **1st** terminal (server):

    $ gst-launch autoaudiosrc ! 'audio/x-raw-int, rate=8000, depth=16, channels=1' ! ffenc_g726 bitrate=32000 ! rtpg726pay ! udpsink host=127.0.0.1 port=9559

In **2nd** terminal (client):

    $ gst-launch udpsrc port=9559 caps="application/x-rtp, media=(string)audio, clock-rate=(int)8000, encoding-name=(string)G726, encoding-params=(string)1, channels=(int)1, payload=(int)96"  ! rtpg726depay ! ffdec_g726 ! autoaudiosink

**Notes**:<br>

- You can use *gst-launch-0.10* (or something like that) instead of *gst-launch*
if it's not found. Autocomplete will help you.<br>
- **gstreamer-ffmpeg** must be installed.
