#ifndef DYNAMIC_CONNECTION_H
#define DYNAMIC_CONNECTION_H

#include <gst/gst.h>
#include <string.h> 
#include <stdlib.h> 

typedef struct {
	GstPad* rptBinPad;
	GstElement* decoderBin;
	GstElement* outputBin;
	gchar* host;
} DynamicConnection;

struct DCLE {
	DynamicConnection* connection;
	struct DCLE* next;
};

typedef struct DCLE DynamicConnectionListElement;

typedef struct {
	DynamicConnectionListElement* head;
	int size;
} DynamicConnectionList;

void dynamicConnectionList_addFirst(DynamicConnectionList* list, DynamicConnection* connection){

	DynamicConnectionListElement* newElement = 
		(DynamicConnectionListElement*) malloc( sizeof(DynamicConnectionListElement));

	newElement->connection = connection;
	newElement->next = list->head;

	list->head = newElement;
	list->size++;

	g_print ("Added new connection to list. New size: %d.\n", list->size);
}

DynamicConnection* dynamicConnectionList_removeByRtpBinPad(DynamicConnectionList* list, GstPad* pad){

	DynamicConnectionListElement* previousElement = 0;
	DynamicConnectionListElement* currentElement  = list->head;

	while (currentElement){

		DynamicConnection* connection = currentElement->connection;
		DynamicConnectionListElement* nextElement = currentElement->next;

		if (connection->rptBinPad == pad){

			if (previousElement){
				previousElement->next = nextElement;
			} else {
				list->head = nextElement;
			}

			free(currentElement);			
			list->size--;

			g_print ("Connection removed from list. New size: %d.\n", list->size);

			return connection;
		}

		previousElement = currentElement;
		currentElement  = nextElement;
	}

	g_print ("No matches in connections list.\n");
	return 0;
}

gboolean dynamicConnectionList_isEmpty(DynamicConnectionList* list){
	return list->size == 0;
}

gboolean dynamicConnectionList_isHostNotRegistered(DynamicConnectionList* list, gchar* host){
	DynamicConnectionListElement* elem = list->head;
	
	while (elem){
		gchar* currentHost = elem->connection->host;
		if (strcmp(currentHost, host)==0){
			g_print ("Host is already registered: %s.\n", host);
			return FALSE;
		}
		elem = elem->next;
	}
	return TRUE;
}

#endif
