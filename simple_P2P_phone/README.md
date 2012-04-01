GStreamer Simple P2P Phone
--------------------------

------------

**Synopsis**

    simple_phone partner's_host [partner's_port] [your_port]

------------

**Description**

This example shows how to make a simple P2P phone using **RTP**-protocol and
**G.726**-encoding. To run this example you'd better to have a network of 2
workstations. Each workstation is a server and client at the same time.

------------

**Parameters**

* partner's_host - IPv4 address of the workstation you want connect to.<br/>
* partner's_port - UDP-port number on the workstation you want connect to.<br/>
* your_port - UDP-port number on your workstation.<br/>

By default port numbers are equal and their value is [9559].

------------

**Pipeline**

The pipeline can be illustrated in such way:

                      .--------------.
                      | autoaudiosrc |
                      '--------------'            
                              |
                             \|/
                              '
                         .---------.
                         | encoder |
                         '---------'
                              |
                             \|/
                              '       .---------.
                         .---------.  |         |   .----------.  host = partner's_host
                         | RTP pay | -------------> | UDP sink |  port = partner's_port
                         '---------'  |         |   '----------'  async=false sync=false 
                                      | RTP bin |
                      .------------.  |         |   .-----------.
    port = your_port  | UDP source | -------------> | RTP depay |
                      '------------'  |         |   '-----------'
                                      '---------'         |
                                                         \|/
                                                          '
                                                     .---------.
                                                     | decoder |
                                                     '---------'
                                                          |
                                                         \|/
                                                          '
                                                  .---------------.
                                                  | autoaudiosink |
                                                  '---------------'

Pipeline's bash-description:

	$ PARTNERS_HOST=192.168.1.2
    $ PARTNERS_PORT=9559

    $ MY_PORT=$PARTNERS_PORT

    $ gst-launch gstrtpbin name=rtpbin \
        autoaudiosrc ! 'audio/x-raw-int, rate=8000, depth=16, channels=1' \
            ! ffenc_g726 bitrate=32000 \
            ! rtpg726pay \
            ! rtpbin.send_rtp_sink_0 rtpbin.send_rtp_src_0 \
            ! udpsink host=$PARTNERS_HOST port=$PARTNERS_PORT sync=false async=false \
        udpsrc port=$MY_PORT caps="application/x-rtp, media=(string)audio, clock-rate=(int)8000, encoding-name=(string)G726, encoding-params=(string)1, channels=(int)1, payload=(int)96" \
            ! rtpbin.recv_rtp_sink_0 rtpbin. \
            ! rtpg726depay \
            ! ffdec_g726 \
            ! autoaudiosink 

------------

**Notes**:<br>

- You can use *gst-launch-0.10* (or something like that) instead of *gst-launch*
if it's not found. Autocomplete will help you.<br>
- **gstreamer-ffmpeg** must be installed.
