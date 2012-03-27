GStreamer direct passthrough
----------------------------

This example shows how to make a direct passthrough audio stream from audio input to audo output.

It's equal to command-line command:

    $ gst-launch-0.10 autoaudiosrc ! autoaudiosink

**Note**:<br>
You can use *gst-launch-0.10* (or something like that) instead of *gst-launch* if it's not found. Autocomplete will help you.
