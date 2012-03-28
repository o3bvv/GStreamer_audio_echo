#include "common.c"

void createUdpSink();

void createElements(){
	pipeline = gst_pipeline_new ("audio-echo-send");
	source   = gst_element_factory_make ("autoaudiosrc", "audio-input");
	createUdpSink();
}

void createUdpSink(){
	sink = gst_element_factory_make ("udpsink", "net-output");
	g_object_set (G_OBJECT (sink), "port", UDP_PORT, NULL);
	g_object_set (G_OBJECT (sink), "host", "127.0.0.1", NULL);
}
