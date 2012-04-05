#include <stdlib.h>
#include <stdio.h>
#include <gst/gst.h>

#include "dynamicConnection.h"

void getParametersOrExit(int argc, char *argv[]);
void getParameters(int argc, char *argv[]);
void printParameters();

void createPrimaryElements();
void createRtpBin();
void createUdpSource();

void addPrimaryElements();

void linkPrimaryElements();
void linkPads_src2Bin();
void linkRtpBinCallbacks();
void linkRtpBin_PAD_ADDED_callback();
void linkRtpBin_PAD_REMOVED_callback();

static void rtpBinPadAdded (GstElement * rtpbin, GstPad * new_pad, gpointer user_data);
static void rtpBinPadRemoved (GstElement * rtpbin, GstPad * pad, gpointer user_data);

GstElement* createRtpDecoderBin();
GstElement* createRtpDecoderBinElement();
GstElement* createRtpSrcQueue();
GstElement* createRtpDepay();
GstElement* createDecoder();
void createRtpDecoderPads(GstElement* bin, GstElement* sinkPadOwner, GstElement* srcPadOwner);
void createRtpDecoderSinkPad(GstElement* bin, GstElement* padOwner);
void createRtpDecoderSrcPad(GstElement* bin, GstElement* padOwner);

GstElement* createRtpOutputBin(gchar* host);
GstElement* createRtpOutputBinElement();
GstElement* createRtpSinkQueue();
GstElement* createUdpSink(gchar* host);
void createRtpOutputSinkPad(GstElement* bin, GstElement* padOwner);

gchar* getOneNewHost ();
gchar* getPeerHostOrZero (GObject* source);

void registerConnection(GstPad* rtpBinPad, GstElement* decoderBin, GstElement* outputBin, gchar* host);

void createMixingBinOnDemand();
gboolean isMixingBinNotCreated();
void createMixingBin();
GstElement* createMixingBinElement();
GstElement* createLiveAdder();
GstElement* createEncoder();
GstElement* createRtpPay();
GstElement* createOutputTee();

void deleteMixingBinOnDemand();
void deleteMixingBin();

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
GstElement *adder, *encoder, *pay, *tee;

DynamicConnectionList connectionList;

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
	linkRtpBinCallbacks();
}

void linkPads_src2Bin(){
	g_print ("\tLinking UDP-source and RTP-bin.\n");

	GstPad* srcpad = gst_element_get_static_pad (udpSource, "src");
	GstPad* sinkpad = gst_element_get_request_pad (rtpBin, "recv_rtp_sink_%d");

	g_assert (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);

	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);
}

void linkRtpBinCallbacks(){
	linkRtpBin_PAD_ADDED_callback();
	linkRtpBin_PAD_REMOVED_callback();
}

void linkRtpBin_PAD_ADDED_callback(){
	g_print ("\tAdding RTP-bin \"pad-added\" callback.\n");
	g_signal_connect (rtpBin, "pad-added", G_CALLBACK (rtpBinPadAdded), NULL);
}

void linkRtpBin_PAD_REMOVED_callback(){
	g_print ("\tAdding RTP-bin \"pad-removed\" callback.\n");
	g_signal_connect (rtpBin, "pad-removed", G_CALLBACK (rtpBinPadRemoved), NULL);
}

static void rtpBinPadAdded (GstElement * rtpbin, GstPad * new_pad, gpointer user_data){
	g_print ("New payload on pad: %s\n", GST_PAD_NAME (new_pad));

	pipeline_pause();

	GstElement* rtpDecoder = createRtpDecoderBin();

	gchar* host = getOneNewHost();
	g_print ("\tSelected peer's host: %s.\n", host);

	GstElement* rtpOutput = createRtpOutputBin(host);

	g_print ("\tLinking pad and RTP-decoder.\n");
	GstPad* sinkpad = gst_element_get_static_pad (rtpDecoder, "sink");
	g_assert (sinkpad);
	g_assert (gst_pad_link (new_pad, sinkpad) == GST_PAD_LINK_OK);
	gst_object_unref (sinkpad);

	createMixingBinOnDemand();

	g_print ("\tLinking RTP-decoder and mixing bin.\n");
	sinkpad = gst_element_get_request_pad (adder, "sink%d");
	GstPad* srcpad  = gst_element_get_static_pad (rtpDecoder, "src");
	g_assert (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);

	g_print ("\tLinking mixing bin and RTP-output.\n");
	sinkpad = gst_element_get_static_pad (rtpOutput, "sink");
	srcpad  = gst_element_get_request_pad (tee, "src%d");
	g_assert (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK);
	gst_object_unref (srcpad);
	gst_object_unref (sinkpad);

	pipeline_run();

	registerConnection(new_pad, rtpDecoder, rtpOutput, host);
}

GstElement* createRtpDecoderBin(){
	g_print ("\tCreating RTP-decoder.\n");

	GstElement* bin 	= createRtpDecoderBinElement();
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

GstElement* createRtpDecoderBinElement(){
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

gchar* getOneNewHost (){
	GObject *session;
	GValueArray *arr;
	GValue *val;
	guint i;

	g_signal_emit_by_name (rtpBin, "get-internal-session", 0, &session);
	g_object_get (session, "sources", &arr, NULL);

	gchar* host = 0;
	gchar** tokens;
	gchar* socketDescr;

	for (i = 0; i < arr->n_values; i++) {
		val = g_value_array_get_nth (arr, i);
		GObject *source = (GObject *)g_value_get_object (val);      
		socketDescr = getPeerHostOrZero(source);

		if (!socketDescr){
			continue;
		}

		if (dynamicConnectionList_isHostNotRegistered(&connectionList, socketDescr)){
			tokens = g_strsplit(socketDescr,":",0);
			int len = g_utf8_strlen(tokens[0], -1)-1;
			host = malloc(len*sizeof(gchar));
			g_utf8_strncpy(host, tokens[0]+1, len);
			break;
		}
	}

	g_value_array_free (arr);
	g_object_unref (session);

	return host;
}

gchar* getPeerHostOrZero (GObject* source){
	GstStructure* stats;
	g_object_get (source, "stats", &stats, NULL);

	gchar* host = 0;

	const GValue* val = gst_structure_get_value(stats, "rtp-from");
	if (!val){
		return host;
	}

	host = g_strdup_value_contents (val);
	gst_structure_free (stats);

	return host;
}

GstElement* createRtpOutputBin(gchar* host){
	g_print ("\tCreating RTP-output.\n");

	GstElement* bin   = createRtpOutputBinElement();
	GstElement* queue = createRtpSinkQueue();
	GstElement* sink  = createUdpSink(host);

	gst_bin_add_many (GST_BIN (bin), queue, sink, NULL);
	
	createRtpOutputSinkPad(bin, queue);	

	g_print ("\t\tAdding to pipeline.\n");
	gst_bin_add (GST_BIN (pipeline), bin);
	g_assert (gst_element_link_many (queue, sink, NULL));

	return bin;
}

GstElement* createRtpOutputBinElement(){
	g_print ("\t\tCreating bin.\n");
	GstElement* elem = gst_bin_new (NULL);
	g_assert(elem);
	return elem;
}

GstElement* createRtpSinkQueue(){
	g_print ("\t\tCreating RTP sink queue.\n");
	GstElement* elem = gst_element_factory_make ("queue", NULL);
	g_assert(elem);
	return elem;
}

GstElement* createUdpSink(gchar* host){
	g_print ("\t\tCreating UDP sink.\n");

	GstElement* elem = gst_element_factory_make ("udpsink", "rtp-output");
	g_assert(elem);
	g_object_set (G_OBJECT (elem), "host", host, NULL);
	g_object_set (G_OBJECT (elem), "port", DEFAULT_UDP_PORT, NULL);
	g_object_set (G_OBJECT (elem), "async", FALSE, "sync", FALSE, NULL);
	return elem;
}

void createRtpOutputSinkPad(GstElement* bin, GstElement* padOwner){
	g_print ("\t\t\tAdding sink pad.\n");
	GstPad* pad = gst_element_get_static_pad (padOwner, "sink");
	gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
	gst_object_unref (GST_OBJECT (pad));
}

void registerConnection(GstPad* rtpBinPad, GstElement* decoderBin, GstElement* outputBin, gchar* host){
	DynamicConnection* dCon = (DynamicConnection*) malloc( sizeof(DynamicConnection));
	dCon->rptBinPad  = rtpBinPad;
	dCon->decoderBin = decoderBin;
	dCon->outputBin  = outputBin;
	dCon->host = host;
	dynamicConnectionList_addFirst(&connectionList, dCon);
}

void createMixingBinOnDemand(){
	if (isMixingBinNotCreated()){
		createMixingBin();
	}
}

gboolean isMixingBinNotCreated(){
	return adder == 0;
}

void createMixingBin(){
	g_print ("\tCreating mixing bin.\n");

	adder   = createLiveAdder();
	encoder = createEncoder();
	pay 	= createRtpPay();
	tee 	= createOutputTee();

	g_print ("\t\tAdding to pipeline.\n");
	gst_bin_add_many (GST_BIN (pipeline), adder, encoder, pay, tee, NULL);
	g_assert (gst_element_link_many (adder, encoder, pay, tee, NULL));	
}

GstElement* createMixingBinElement(){
	g_print ("\t\tCreating bin element.\n");
	GstElement* elem = gst_bin_new (NULL);
	g_assert(elem);
	return elem;
}

GstElement* createLiveAdder(){
	g_print ("\t\tCreating live adder.\n");
	GstElement* elem = gst_element_factory_make ("liveadder", "adder");
	g_assert (elem);
	return elem;
}

GstElement* createEncoder(){
	g_print ("\t\tCreating encoder.\n");

	GstElement* elem = gst_element_factory_make ("ffenc_g726", "G.726-coder");
	g_object_set (G_OBJECT (elem), "bitrate", 32000, NULL);
	g_assert (elem);
	return elem;
}

GstElement* createRtpPay(){
	g_print ("\t\tCreating RTP-pay.\n");
	GstElement* elem = gst_element_factory_make ("rtpg726pay", "rtp-pay");
	g_assert (elem);
	return elem;
}

GstElement* createOutputTee(){
	g_print ("\t\tCreating output tee.\n");
	GstElement* elem = gst_element_factory_make ("tee", "output-tee");
	g_assert (elem);
	return elem;
}

static void rtpBinPadRemoved (GstElement * rtpbin, GstPad * pad, gpointer user_data){
	g_print ("Removing pad: %s\n", GST_PAD_NAME (pad));

	DynamicConnection* dCon = dynamicConnectionList_removeByRtpBinPad(&connectionList, pad);	
	g_assert (dCon);

	GstElement* decoderBin = dCon->decoderBin;
	GstElement* outputBin  = dCon->outputBin;
	free(dCon);

	pipeline_pause();

	g_print ("\tUnlinking RTP-decoder and mixing bin.\n");
	GstPad* srcpad  = gst_element_get_static_pad (decoderBin, "src");
	GstPad* sinkpad = gst_pad_get_peer(srcpad);
	g_assert (gst_pad_unlink (srcpad, sinkpad));
	gst_object_unref (sinkpad);
	gst_object_unref (srcpad);

	g_print ("\tUnlinking RTP-output and mixing bin.\n");
	sinkpad = gst_element_get_static_pad (outputBin, "sink");
	srcpad  = gst_pad_get_peer(sinkpad);
	g_assert (gst_pad_unlink (srcpad, sinkpad));
	gst_object_unref (sinkpad);
	gst_object_unref (srcpad);

	g_print ("\tStopping RTP-decoder.\n");
	gst_element_set_state (decoderBin, GST_STATE_NULL);
	gst_bin_remove (GST_BIN (pipeline), decoderBin);

	g_print ("\tStopping RTP-output.\n");
	gst_element_set_state (outputBin, GST_STATE_NULL);
	gst_bin_remove (GST_BIN (pipeline), outputBin);

	deleteMixingBinOnDemand();

	g_print ("\tPad removed.\n");
	pipeline_run();
}

void deleteMixingBinOnDemand(){
	if (dynamicConnectionList_isEmpty(&connectionList)){
		deleteMixingBin();
	}
}

void deleteMixingBin(){
	g_print ("\tDeleting mixing bin.\n");

	gst_element_set_state (adder, GST_STATE_NULL);
	gst_bin_remove (GST_BIN (pipeline), adder);

	gst_element_set_state (encoder, GST_STATE_NULL);
	gst_bin_remove (GST_BIN (pipeline), encoder);

	gst_element_set_state (pay, GST_STATE_NULL);
	gst_bin_remove (GST_BIN (pipeline), pay);

	gst_element_set_state (tee, GST_STATE_NULL);
	gst_bin_remove (GST_BIN (pipeline), tee);

	adder = 0;
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

