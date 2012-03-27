CPP=g++
LIBS=`pkg-config gstreamer-0.10 --libs`
CFLAGS=-Wall `pkg-config gstreamer-0.10 --cflags`

main: main.cpp
	$(CPP) $(LIBS) $(CFLAGS) -o gst_audio_echo main.cpp
