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
GstElement* createRtpDecoderBin();
GstElement* createRtpSrcQueue();
GstElement* createRtpDepay();
GstElement* createDecoder();
void createRtpDecoderPads(GstElement* bin, GstElement* sinkPadOwner, GstElement* srcPadOwner);
void createRtpDecoderSinkPad(GstElement* bin, GstElement* padOwner);
void createRtpDecoderSrcPad(GstElement* bin, GstElement* padOwner);

void checkMultiplexingPart();
gboolean isLiveAdderNotCreated();
void createMultiplexingPart();
void createLiveAdder();
void createTestAudioSink();

void registerBusCall();
static gboolean busCall(GstBus *bus, GstMessage *msg, gpointer data);

void runLoop();
void cleanUp();

void pipeline_run();
void pipeline_pause();
void pipeline_stop();

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
GstElement *liveAdder, *testAudioSink;

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
	GstPad* sinkpad = gst_element_get_request_pad (rtpBin, "recv_rtp_sink_%d");

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

	pipeline_pause();

	GstElement* rtpDecoder = createRtpDecoder();

	g_print ("\tLinking pad and RTP-decoder.\n");
	GstPad* sinkpad = gst_element_get_static_pad (rtpDecoder, "sink");
	g_assert (gst_pad_link (new_pad, sinkpad) == GST_PAD_LINK_OK);
	gst_object_unref (sinkpad);

	checkMultiplexingPart();

	g_print ("\tRTP-decoder and multiplexing part.\n");
			sinkpad = gst_element_get_request_pad (liveAdder, "sink%d");
	GstPad* srcpad  = gst_element_get_static_pad (rtpDecoder, "src");
	g_assert (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);

	pipeline_run();
}


GstElement* createRtpDecoder(){
	// PIPELINE MUST BE PAUSED OR STOPPED
	
	g_print ("\tCreating RTP-decoder.\n");

	GstElement* bin 	= createRtpDecoderBin();
	GstElement* queue   = createRtpSrcQueue();
	GstElement* depay   = createRtpDepay();
	GstElement* decoder = createDecoder();

	gst_bin_add_many (GST_BIN (bin), queue, depay, decoder, NULL);
	
	createRtpDecoderPads(bin, queue, decoder);	

	g_print ("\t\tAdding to pipeline.\n");
	gst_bin_add (GST_BIN (pipeline), bin);
	g_assert (gst_element_link_many (queue, depay, decoder, NULL));

	return bin;
}

GstElement* createRtpDecoderBin(){
	g_print ("\t\tCreating bin.\n");
	GstElement* elem = gst_bin_new (NULL);
	g_assert(elem);
	return elem;
}

GstElement* createRtpSrcQueue(){
	g_print ("\t\tCreating RTP source queue.\n");
	GstElement* elem = gst_element_factory_make ("queue", NULL);
	g_assert(elem);
	return elem;
}

GstElement* createRtpDepay(){
	g_print ("\t\tCreating RTP depay loader.\n");
	GstElement* elem = gst_element_factory_make ("rtpg726depay", NULL);
	g_assert(elem);
	return elem;
}

GstElement* createDecoder(){
	g_print ("\t\tCreating G.726 decoder.\n");
	GstElement* elem = gst_element_factory_make ("ffdec_g726", NULL);
	g_assert(elem);
	return elem;
}

void createRtpDecoderPads(GstElement* bin, GstElement* sinkPadOwner, GstElement* srcPadOwner){
	g_print ("\t\tAdding ghost pads.\n");
	createRtpDecoderSinkPad(bin, sinkPadOwner);
	createRtpDecoderSrcPad(bin, srcPadOwner);
}

void createRtpDecoderSinkPad(GstElement* bin, GstElement* padOwner){
	g_print ("\t\t\tAdding sink pad.\n");
	GstPad* pad = gst_element_get_static_pad (padOwner, "sink");
	gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
	gst_object_unref (GST_OBJECT (pad));
}

void createRtpDecoderSrcPad(GstElement* bin, GstElement* padOwner){
	g_print ("\t\t\tAdding source pad.\n");
	GstPad* pad = gst_element_get_static_pad (padOwner, "src");
	gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad));
	gst_object_unref (GST_OBJECT (pad));
}

void checkMultiplexingPart(){
	if (isLiveAdderNotCreated()){
		createMultiplexingPart();
	}
}

gboolean isLiveAdderNotCreated(){
	return liveAdder == 0;
}

void createMultiplexingPart(){
	g_print ("\tCreating multiplexing part.\n");

	createLiveAdder();
	createTestAudioSink();

	gst_bin_add_many (GST_BIN (pipeline), liveAdder, testAudioSink, NULL);
	g_assert (gst_element_link (liveAdder, testAudioSink));
}

void createLiveAdder(){
	g_print ("\t\tCreating live adder.\n");
	liveAdder = gst_element_factory_make ("liveadder", "adder");
	g_assert (liveAdder);
}

void createTestAudioSink(){
	g_print ("\t\tCreating audio sink.\n");
	testAudioSink = gst_element_factory_make ("autoaudiosink", "audio-output");
	g_assert (testAudioSink);
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
	pipeline_run();	
	g_print ("Running...\n");
	g_main_loop_run (loop);
}

void cleanUp(){
	g_print ("Returned from main loop.\n");
	pipeline_stop();

	g_print ("Deleting pipeline\n");
	gst_object_unref (GST_OBJECT (pipeline));
}

void pipeline_run(){
	g_print ("Starting pipeline.\n");
	gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

void pipeline_pause(){
	g_print ("Pausing pipeline.\n");
	gst_element_set_state (pipeline, GST_STATE_PAUSED);
}

void pipeline_stop(){
	g_print ("Stopping pipeline.\n");
	gst_element_set_state (pipeline, GST_STATE_NULL);
}

