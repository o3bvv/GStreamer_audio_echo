#include "common.c"

void createUdpSource();

void createElements(){
	pipeline = gst_pipeline_new ("audio-echo-receive");
	createUdpSource();
	sink     = gst_element_factory_make ("autoaudiosink", "audio-output");
}

void createUdpSource(){
	source = gst_element_factory_make ("udpsrc", "net-input");
	g_object_set (G_OBJECT (source), "port", UDP_PORT, NULL);
}
