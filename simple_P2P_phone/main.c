#include <stdlib.h>
#include <stdio.h>
#include <gst/gst.h>

void getParametersOrExit(int argc, char *argv[]);
void checkParametersCountOrExit(int count);
void getParameters(int argc, char *argv[]);
void printParameters();


void createElementsOrExit();
void createElements();

void createAudioElements();
void createAudioSource();

void createUdpElements();
void createUdpSource();
void createUdpSink();

void createCodecElements();
void createEncoder();

void createPayDepayElements();

void exitOnInvalidElement();


void addAndLinkElementsOrExit();
void addAndLinkTxElementsOrExit();
void addAndLinkRxElementsOrExit();

void linkTxPadsOrExit();
void linkPads_Pay2Bin_OrExit();
void linkPads_Bin2Sink_OrExit();

void linkRxPadsOrExit();
void linkPads_Src2Bin_OrExit();
void linkPads_Bin2Depay_OrExit();

static void rtpBinPadAdded (GstElement * rtpbin, GstPad * new_pad, GstElement * depay);

void checkLinkingSuccessOrExit();
void checkPadsLinkingSuccessOrExit();


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

char* partnerHost;
int partnerPort = DEFAULT_UDP_PORT;
int localPort   = DEFAULT_UDP_PORT;

GMainLoop  *loop;
GstElement *pipeline;

GstElement *audioSource, *audioSink;
GstElement *udpSource,   *udpSink;
GstElement *encoder,     *decoder;
GstElement *rtpPay,      *rtpDepay;

GstElement *rtpbin;
GstPad     *srcpad, *sinkpad;

gboolean link_ok;
GstPadLinkReturn linkPad_ok;

int main(int argc, char *argv[]) {
    gst_init(NULL, NULL);
	
	getParametersOrExit(argc, argv);

	createElementsOrExit();
	addAndLinkElementsOrExit();

	loop = g_main_loop_new (NULL, FALSE);
	registerBusCall();

	runLoop();
	cleanUp(); // Normally never will be called

    return EXIT_NORMAL;
}

void getParametersOrExit(int argc, char *argv[]){
	checkParametersCountOrExit(argc);
	getParameters(argc, argv);
	printParameters();
}

void checkParametersCountOrExit(int count){
	g_print ("Checking parameter's count.\n");
	if (count < 2) {
		g_printerr("Not enough parameters. Exiting.\n");
		exit(EXIT_NOT_ENOUGH_PARAMETERS);
	}
}

void getParameters(int argc, char *argv[]){
	g_print ("Getting parameters.\n");
	g_print ("\tGetting partner's host.\n");

	partnerHost = argv[1];

	if (argc<3){
		return;
	}

	g_print ("\tGetting partner's port.\n");
	partnerPort = atoi(argv[2]);

	if (argc<4){
		return;
	}

	g_print ("\tGetting local port.\n");
	localPort = atoi(argv[3]);
}

void printParameters(){
	g_print ("Connection parameters:\n");
	g_print ("\tPartner's host: %s.\n", partnerHost);
	g_print ("\tPartner's port: %d.\n", partnerPort);
	g_print ("\tLocal port    : %d.\n", localPort);
}

void createElementsOrExit(){
	createElements();
	exitOnInvalidElement();
}

void createElements(){
	g_print ("Creating elements.\n");

	pipeline  = gst_pipeline_new ("simple-phone");
	
	createAudioElements();
	createUdpElements();
	createCodecElements();
	createPayDepayElements();

	g_print ("\tCreating RTP-bin.\n");
	rtpbin = gst_element_factory_make ("gstrtpbin", "rtpbin");
}

void createAudioElements(){
	g_print ("\tCreating audio elements.\n");
	createAudioSource();

	g_print ("\t\tCreating audio sink.\n");
	audioSink = gst_element_factory_make ("autoaudiosink", "audio-output");
}

void createAudioSource(){
	g_print ("\t\tCreating audio source.\n");

	GstCaps *caps = gst_caps_new_simple (
		"audio/x-raw-int",	     
		"rate",     G_TYPE_INT, 8000,
		"depth",    G_TYPE_INT, 16,		
		"channels", G_TYPE_INT, 1,
		NULL);

	audioSource = gst_element_factory_make ("autoaudiosrc", "audio-input");
	g_object_set (G_OBJECT (audioSource), "filter-caps", caps, NULL);
	gst_caps_unref (caps);
}

void createUdpElements(){
	g_print ("\tCreating UDP elements.\n");
	createUdpSource();
    createUdpSink();
}

void createUdpSource(){
	g_print ("\t\tCreating UDP source.\n");

	GstCaps *caps = gst_caps_new_simple (
		"application/x-rtp",	     
		"media",           G_TYPE_STRING, "audio",
		"clock-rate",      G_TYPE_INT,    8000,
		"encoding-name",   G_TYPE_STRING, "G726",
		"encoding-params", G_TYPE_STRING, "1",
		"channels",        G_TYPE_INT,    1,
		"payload",         G_TYPE_INT,    96,
		NULL);

	udpSource = gst_element_factory_make ("udpsrc", "net-input");

	g_object_set (G_OBJECT (udpSource), "port", localPort, NULL);
	g_object_set (G_OBJECT (udpSource), "caps", caps,      NULL);

	gst_caps_unref (caps);
}

void createUdpSink(){
	g_print ("\t\tCreating UDP sink.\n");

	udpSink = gst_element_factory_make ("udpsink", "net-output");
	g_object_set (G_OBJECT (udpSink), "host", partnerHost, NULL);
	g_object_set (G_OBJECT (udpSink), "port", partnerPort, NULL);
	g_object_set (G_OBJECT (udpSink), "async", FALSE, "sync", FALSE, NULL);
}

void createCodecElements(){
	g_print ("\tCreating codec elements.\n");
	createEncoder();

	g_print ("\t\tCreating decoder.\n");
	decoder = gst_element_factory_make ("ffdec_g726", "G.726-decoder");
}

void createEncoder(){
	g_print ("\t\tCreating encoder.\n");

	encoder = gst_element_factory_make ("ffenc_g726", "G.726-coder");
	g_object_set (G_OBJECT (encoder), "bitrate", 32000, NULL);
}

void createPayDepayElements(){
	g_print ("\tCreating pay-depay elements.\n");

	g_print ("\t\tCreating RTP-pay.\n");
	rtpPay   = gst_element_factory_make ("rtpg726pay",   "rtp-pay");

	g_print ("\t\tCreating RTP-depay.\n");
	rtpDepay = gst_element_factory_make ("rtpg726depay", "rtp-depay");
}

void exitOnInvalidElement(){
	g_print ("Validating elements.\n");
	if (    !pipeline
		 || !audioSource
		 || !audioSink
		 || !udpSource
		 || !udpSink
		 || !encoder
		 || !decoder
		 || !rtpPay
		 || !rtpDepay) {

		g_printerr ("Some element could not be created. Exiting.\n");
		exit(EXIT_ELEMENT_CREATION_FAILURE);
	}
}

void addAndLinkElementsOrExit(){
	gst_bin_add (GST_BIN (pipeline), rtpbin);
	
	addAndLinkTxElementsOrExit();
	addAndLinkRxElementsOrExit();

	// must be called after all elements added to pipe
	linkTxPadsOrExit();
	linkRxPadsOrExit();
}

void addAndLinkTxElementsOrExit(){
	gst_bin_add_many (GST_BIN (pipeline), udpSource, rtpDepay, decoder, audioSink, NULL);
	link_ok = gst_element_link_many (rtpDepay, decoder, audioSink, NULL);
	checkLinkingSuccessOrExit();
}

void addAndLinkRxElementsOrExit(){
	gst_bin_add_many (GST_BIN (pipeline), audioSource, encoder, rtpPay, udpSink, NULL);
	link_ok = gst_element_link_many (audioSource, encoder, rtpPay, NULL);
	checkLinkingSuccessOrExit();
}

void linkTxPadsOrExit(){
	linkPads_Pay2Bin_OrExit();
	linkPads_Bin2Sink_OrExit();
}

void linkPads_Pay2Bin_OrExit(){
	sinkpad = gst_element_get_request_pad (rtpbin, "send_rtp_sink_0");
	srcpad = gst_element_get_static_pad (rtpPay, "src");
	linkPad_ok = gst_pad_link (srcpad, sinkpad);
	checkPadsLinkingSuccessOrExit();
	gst_object_unref (srcpad);
}

void linkPads_Bin2Sink_OrExit(){
	srcpad = gst_element_get_static_pad (rtpbin, "send_rtp_src_0");
	sinkpad = gst_element_get_static_pad (udpSink, "sink");
	linkPad_ok = gst_pad_link (srcpad, sinkpad);
	checkPadsLinkingSuccessOrExit();
	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);
}

void linkRxPadsOrExit(){
	linkPads_Src2Bin_OrExit();
	linkPads_Bin2Depay_OrExit();
}

void linkPads_Src2Bin_OrExit(){
	srcpad = gst_element_get_static_pad (udpSource, "src");
	sinkpad = gst_element_get_request_pad (rtpbin, "recv_rtp_sink_0");
	linkPad_ok = gst_pad_link (srcpad, sinkpad);
	checkPadsLinkingSuccessOrExit();
	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);
}

void linkPads_Bin2Depay_OrExit(){
	// must be last-called because of dynamic linking
	g_signal_connect (rtpbin, "pad-added", G_CALLBACK (rtpBinPadAdded), rtpDepay);
}

static void rtpBinPadAdded (GstElement * rtpbin, GstPad * new_pad, GstElement * depay){
	g_print ("New payload on pad: %s\n", GST_PAD_NAME (new_pad));

	sinkpad = gst_element_get_static_pad (depay, "sink"); 
	linkPad_ok = gst_pad_link (new_pad, sinkpad);
	checkPadsLinkingSuccessOrExit();

	gst_object_unref (sinkpad);
}

void checkLinkingSuccessOrExit(){
	if (!link_ok) {
    	g_printerr ("Failed to link elements.");
		exit(EXIT_ELEMENT_LINKING_FAILURE);
  	}
}

void checkPadsLinkingSuccessOrExit(){
	if (linkPad_ok != GST_PAD_LINK_OK) {
    	g_printerr ("Failed to link pads.");
		exit(EXIT_PADS_LINKING_FAILURE);
  	}
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

