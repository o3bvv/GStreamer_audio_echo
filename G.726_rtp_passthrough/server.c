#include "common.c"

void createSource();
void createUdpSink();
void createEncoder();

void createElements(){
	pipeline = gst_pipeline_new ("audio-echo-send");
	createSource();
	createEncoder();
	payDepay = gst_element_factory_make ("rtpg726pay",   "rtp-pay");
	createUdpSink();
}

void createSource(){
	g_print ("Creating source.\n");

	GstCaps *caps = gst_caps_new_simple (
		"audio/x-raw-int",	     
		"rate",     G_TYPE_INT, 8000,
		"depth",    G_TYPE_INT, 16,		
		"channels", G_TYPE_INT, 1,
		NULL);

	source = gst_element_factory_make ("autoaudiosrc", "audio-input");
	g_object_set (G_OBJECT (source), "filter-caps", caps, NULL);
	gst_caps_unref (caps);
}

void createEncoder(){
	g_print ("Creating encoder.\n");

	codec = gst_element_factory_make ("ffenc_g726", "G.726-coder");
	g_object_set (G_OBJECT (codec), "bitrate", 32000, NULL);
}

void createUdpSink(){
	g_print ("Creating sink.\n");

	sink = gst_element_factory_make ("udpsink", "net-output");
	g_object_set (G_OBJECT (sink), "port", UDP_PORT, NULL);
	g_object_set (G_OBJECT (sink), "host", "127.0.0.1", NULL);
}

void linkElements(){
	gboolean link_ok = gst_element_link_many (source, codec, payDepay, sink, NULL);

	if (!link_ok) {
    	g_printerr ("Failed to link elements.");
		exit(EXIT_ELEMENT_LINKING_FAILURE);
  	}
}
