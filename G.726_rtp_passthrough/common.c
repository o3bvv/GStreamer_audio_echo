#include <stdio.h>
#include <gst/gst.h>

void createElementsOrExit();
void createElements();		 	// a pseudo-abstract method
void exitOnInvalidElement();

void linkElements();			// a pseudo-abstract method

void registerBusCall();
static gboolean busCall (GstBus *bus, GstMessage *msg, gpointer data);

void runLoop();
void cleanUp();

#define UDP_PORT 9559

#define EXIT_NORMAL 0
#define EXIT_ELEMENT_CREATION_FAILURE -1
#define EXIT_ELEMENT_LINKING_FAILURE  -2

GMainLoop *loop;

GstElement *pipeline, *source, *codec, *payDepay, *sink;

int main(int argc, char *argv[]) {
    gst_init(NULL, NULL);
	
	loop = g_main_loop_new (NULL, FALSE);

	createElementsOrExit();
	registerBusCall();

	gst_bin_add_many (GST_BIN (pipeline), source, sink, codec, payDepay, NULL);

	g_print ("Linking elements.\n");
	linkElements();

	runLoop();
	cleanUp(); // Under normal conditions this method will never be called

    return EXIT_NORMAL;
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
		exit(EXIT_ELEMENT_CREATION_FAILURE);
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
