/*
 * ws protocol handler plugin for "server.c"
 *
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include "protocol.h"
#include "sdr_interface.h"
#include <string.h>

const struct lws_protocols *protocol;

/* when anything happens, it all comes into here. The key parts are the   reason   which is the reason we are here */
/* and the   *in   which is the data we have been sent by a client. Note I have made this non static so it will    */
/* build as a stand alone file not needing to be included in another C file                                        */
int
callback_beacon_server(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len)
{
        /* user   points to a version of our per_session data so we cast it into the right shape here */
	int m;

        /* now we have the right shape for the input data we can go ahead and find out why we are here and act accordingly */
	switch (reason) {
	/* we come in here as soon as we startup the protocol */
        case LWS_CALLBACK_PROTOCOL_INIT:
		printf("WS INIT\n");
                protocol = lws_get_protocol(wsi);
		break;

	/* we get here if a client connects to us */
        case LWS_CALLBACK_ESTABLISHED:
		printf("WS Established\n");
		break;

	/* if a client closes the connection to us we come in here */
        case LWS_CALLBACK_CLOSED:
		printf("WS Closed\n");
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
                /* it looks like clients immediatly say they are writeable but as we have no payload to start with */
                /* we don't actually send them anything for now */
//		printf("WS Writeable\n");

                m = lws_write(wsi, transmit_buffer + LWS_PRE, 4096, LWS_WRITE_BINARY);
                if (m < 4096) lwsl_err("WS Error %d  writing to ws\n",m);

		break;

	/* if a client sends us some data we receive it here */
        case LWS_CALLBACK_RECEIVE:
		printf("WS Receive\n");
                /* if we already have a message ready to send out then we destroy it ready for filling with the new one */
                /* we just received */

//		lws_callback_on_writable_all_protocol(lws_get_context(wsi), lws_get_protocol(wsi));

		break;

	default:
		break;
	}

	return 0;
}

