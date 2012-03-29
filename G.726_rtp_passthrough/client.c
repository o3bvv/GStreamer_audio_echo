#include "common.c"

void createUdpSource();

void createElements(){
	pipeline = gst_pipeline_new ("audio-echo-receive");
	createUdpSource();
	payDepay = gst_element_factory_make ("rtpg726depay",  "rtp-depay");
	codec	 = gst_element_factory_make ("ffdec_g726",    "G.726-decoder");
	sink     = gst_element_factory_make ("autoaudiosink", "audio-output");
}

void createUdpSource(){
	g_print ("Creating source.\n");

	GstCaps *caps = gst_caps_new_simple (
		"application/x-rtp",	     
		"media",           G_TYPE_STRING, "audio",
		"clock-rate",      G_TYPE_INT,    8000,
		"encoding-name",   G_TYPE_STRING, "G726",
		"encoding-params", G_TYPE_STRING, "1",
		"channels",        G_TYPE_INT,    1,
		"payload",         G_TYPE_INT,    96,
		NULL);

	source = gst_element_factory_make ("udpsrc", "net-input");

	g_object_set (G_OBJECT (source), "port", UDP_PORT, NULL);
	g_object_set (G_OBJECT (source), "caps", caps, NULL);

	gst_caps_unref (caps);
}

void linkElements(){
	gboolean link_ok = gst_element_link_many (source, payDepay, codec, sink, NULL);

	if (!link_ok) {
    	g_printerr ("Failed to link elements.");
		exit(EXIT_ELEMENT_LINKING_FAILURE);
  	}
}
