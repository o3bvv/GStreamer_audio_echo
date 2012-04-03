#include <stdlib.h>
#include <stdio.h>
#include <gst/gst.h>

void getParametersOrExit(int argc, char *argv[]);
void getParameters(int argc, char *argv[]);
void printParameters();

void createPrimaryElements();
void createRtpBin();
void createUdpSource();

void addPrimaryElements();

void linkPrimaryElements();
void linkPads_src2Bin();
void linkRtpBin_PAD_ADDED_callback();

static void rtpBinPadAdded (GstElement * rtpbin, GstPad * new_pad, gpointer user_data);
GstElement* createRtpDecoder();

void registerBusCall();
static gboolean busCall(GstBus *bus, GstMessage *msg, gpointer data);

void runLoop();
void cleanUp();

#define EXIT_NORMAL 0
#define EXIT_NOT_ENOUGH_PARAMETERS    -1
#define EXIT_ELEMENT_CREATION_FAILURE -2
#define EXIT_ELEMENT_LINKING_FAILURE  -3
#define EXIT_PADS_LINKING_FAILURE     -4

#define DEFAULT_UDP_PORT 9559

int listenPort = DEFAULT_UDP_PORT;

GMainLoop  *loop;

GstElement *pipeline;
GstElement *rtpBin, *udpSource;
GstElement *liveadder;

int main(int argc, char *argv[]) {
    gst_init(NULL, NULL);

	getParametersOrExit(argc, argv);

	createPrimaryElements();
	addPrimaryElements();
	linkPrimaryElements();

	loop = g_main_loop_new (NULL, FALSE);
	registerBusCall();

	runLoop();
	cleanUp(); // Normally never will be called

    return EXIT_NORMAL;
}

void getParametersOrExit(int argc, char *argv[]){
	getParameters(argc, argv);
	printParameters();
}

void getParameters(int argc, char *argv[]){

	if (argc < 2) {
		return;
	}

	g_print ("Getting parameters.\n");
	g_print ("\tGetting port to listen.\n");
	listenPort = atoi(argv[1]);
}

void printParameters(){
	g_print ("Connection parameters:\n");
	g_print ("\tPort to listen: %d.\n", listenPort);
}

void createPrimaryElements(){
	g_print ("Creating primary elements.\n");

	g_print ("\tCreating pipeline.\n");
	pipeline  = gst_pipeline_new ("simple-phone");
	
	createUdpSource();
	createRtpBin();
}

void createRtpBin(){
	g_print ("\tCreating RTP-bin.\n");
	rtpBin = gst_element_factory_make ("gstrtpbin", "rtpbin");
	g_assert (rtpBin);
	g_object_set (G_OBJECT (rtpBin), "autoremove", TRUE, NULL);
}

void createUdpSource(){
	g_print ("\t\tCreating UDP source.\n");

	udpSource = gst_element_factory_make ("udpsrc", "net-input");
	g_assert (udpSource);

	GstCaps *caps = gst_caps_new_simple (
		"application/x-rtp",	     
		"media",           G_TYPE_STRING, "audio",
		"clock-rate",      G_TYPE_INT,    8000,
		"encoding-name",   G_TYPE_STRING, "G726",
		"encoding-params", G_TYPE_STRING, "1",
		"channels",        G_TYPE_INT,    1,
		"payload",         G_TYPE_INT,    96,
		NULL);
	g_assert (caps);	

	g_object_set (G_OBJECT (udpSource), "caps", caps,      NULL);
	g_object_set (G_OBJECT (udpSource), "port", listenPort, NULL);

	gst_caps_unref (caps);
}

void addPrimaryElements(){
	g_print ("Adding primary elements.\n");
	gst_bin_add_many (GST_BIN (pipeline), rtpBin, udpSource, NULL);
}

void linkPrimaryElements(){
	g_print ("Linking primary elements.\n");
	linkPads_src2Bin();
	linkRtpBin_PAD_ADDED_callback();
}

void linkPads_src2Bin(){
	g_print ("\tLinking UDP-source and RTP-bin.\n");

	GstPad* srcpad = gst_element_get_static_pad (udpSource, "src");
	GstPad* sinkpad = gst_element_get_request_pad (rtpBin, "recv_rtp_sink_0");

	g_assert (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);

	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);
}

void linkRtpBin_PAD_ADDED_callback(){
	g_print ("\tAdding RTP-bin \"pad-added\" callback.\n");
	g_signal_connect (rtpBin, "pad-added", G_CALLBACK (rtpBinPadAdded), NULL);
}

static void rtpBinPadAdded (GstElement * rtpbin, GstPad * new_pad, gpointer user_data){
	g_print ("New payload on pad: %s\n", GST_PAD_NAME (new_pad));

	GstElement* rtpDecoder = createRtpDecoder();

	g_print ("\tLinking pad and RTP-decoder.\n");
	GstPad* sinkpad = gst_element_get_static_pad (rtpDecoder, "sink");
	g_assert (gst_pad_link (new_pad, sinkpad) == GST_PAD_LINK_OK);
	gst_object_unref (sinkpad);
}


GstElement* createRtpDecoder(){
	g_print ("\tCreating RTP-decoder.\n");

	GstElement *bin, *queue, *depay, *decoder;

	g_print ("\t\tCreating bin.\n");
	bin = gst_bin_new (NULL);
	g_assert(bin);

	g_print ("\t\tCreating queue.\n");
	queue   = gst_element_factory_make ("queue",        NULL);
	g_assert(queue);

	g_print ("\t\tCreating depay.\n");
	depay   = gst_element_factory_make ("rtpg726depay", NULL);
	g_assert(depay);

	g_print ("\t\tCreating G.726 decoder.\n");
	decoder = gst_element_factory_make ("ffdec_g726",   NULL);
	g_assert(decoder);

	gst_bin_add_many (GST_BIN (bin), queue, depay, decoder, NULL);

	g_print ("\t\tAdding ghost pads.\n");
	GstPad* pad = gst_element_get_static_pad (queue, "sink");
	gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
	gst_object_unref (GST_OBJECT (pad));

	pad = gst_element_get_static_pad (decoder, "src");
	gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad));
	gst_object_unref (GST_OBJECT (pad));

	g_print ("\t\tAdding to pipeline.\n");
	gst_bin_add (GST_BIN (pipeline), bin);
	g_assert (gst_element_link_many (queue, depay, decoder, NULL));

	return bin;
}

void registerBusCall(){
	g_print ("Registering bus call.\n");
	GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus, busCall, loop);
	gst_object_unref (bus);
}

static gboolean busCall (GstBus *bus, GstMessage *msg, gpointer data) {

	GMainLoop *loop = (GMainLoop *) data;

	switch (GST_MESSAGE_TYPE (msg)) {

		case GST_MESSAGE_EOS:
			g_print ("End of stream\n");
			g_main_loop_quit (loop);
			break;

		case GST_MESSAGE_ERROR: {
			gchar  *debug;
			GError *error;

			gst_message_parse_error (msg, &error, &debug);
			g_free (debug);

			g_printerr ("Error: %s\n", error->message);
			g_error_free (error);

			g_main_loop_quit (loop);
			break;
		}

		default:
			break;
	}

	return TRUE;
}

void runLoop(){
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	
	g_print ("Running...\n");
	g_main_loop_run (loop);
}

void cleanUp(){
	g_print ("Returned, stopping playback\n");
	gst_element_set_state (pipeline, GST_STATE_NULL);

	g_print ("Deleting pipeline\n");
	gst_object_unref (GST_OBJECT (pipeline));
}

