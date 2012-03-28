#include <stdio.h>
#include <gst/gst.h>

void createElementsOrExit();
void createElements();			// a pseudo-abstract method
void exitOnInvalidElement();

void linkSourceAndSink();

void registerBusCall();
static gboolean busCall (GstBus *bus, GstMessage *msg, gpointer data);

void runLoop();
void cleanUp();

#define UDP_PORT 9559

GMainLoop *loop;

GstElement *pipeline, *source, *sink;

int main(int argc, char *argv[]) {
    gst_init(NULL, NULL);
	
	loop = g_main_loop_new (NULL, FALSE);

	createElementsOrExit();
	registerBusCall();

	gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL);

	linkSourceAndSink();

	runLoop();
	cleanUp(); // Under normal conditions this method will never be called

    return 0;
}

void createElementsOrExit(){
	g_print ("Creating elements.\n");
	createElements();
	g_print ("Checking elements.\n");
	exitOnInvalidElement();
}

void exitOnInvalidElement(){
	if (!pipeline || !source || !sink) {
		g_printerr ("One element could not be created. Exiting.\n");
		exit(-1);
	}
}

void linkSourceAndSink(){
	GstCaps *caps = gst_caps_new_simple (
		"audio/x-raw-int",	     
		"rate",       G_TYPE_INT, 22050,
		"depth",      G_TYPE_INT, 16,
		"endianness", G_TYPE_INT, 4321,
		"channels",   G_TYPE_INT, 1,
		"signed",     G_TYPE_BOOLEAN, TRUE,
		NULL);

	gboolean link_ok = gst_element_link_filtered (source, sink, caps);

	if (!link_ok) {
    	g_printerr ("Failed to link source and sink.");
		exit(-2);
  	}
	
	gst_caps_unref (caps);
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
	g_print ("Starting loop.\n");
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
