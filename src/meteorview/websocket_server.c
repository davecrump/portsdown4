/* server.c
 * 
 * a simple server for websockets that will send data to all attached clients
 * example is aimed at getting the meteor beacon working sending audio to all clients
 * 
 * Heather Nickalls
 * 
 * Copyright 2023
 *
 * Version History:
 * ----------------
 * 0.01 26/Jan/2023 - Initial version based on minimal_lws_server example from LWS
 * 0.02 28/Jan/2023 - added in first pass of websockets access
 *
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

/* we will include the protocol (and all its handlers etc as a C file - nasty but ... */
#define LWS_PLUGIN_STATIC
#include "protocol.h"

/* it looks like we need to define 2 protocols, the first is always http    */
/* this tells the program how to serve http files                           */
/* the second is our web sockets protocol that we can name as we see fit    */
/* the third line is a list terminator and just has a null protocol defined */
static struct lws_protocols protocols[] = {
	{ "http", lws_callback_http_dummy, 0, 0, 0, NULL, 0},
	LWS_PLUGIN_PROTOCOL_BEACON_SERVER,
	LWS_PROTOCOL_LIST_TERM
};

/* this is used to get us out of the infinte loop that calls the service routine */
/* it will be set to 1 if we do a ctrl-c (sigint)                                */
static int interrupted;

/* this tells the program how to mount an hhtp file. In this case we serve up    */
/* the file   index.html   in the directory ./mount-origin (in the directory the */
/* prog is run from                                                              */
static const struct lws_http_mount mount = {
	/* .mount_next */		NULL,		/* linked-list "next" */
	/* .mountpoint */		"/",		/* mountpoint URL */
	/* .origin */			"./mount-origin",  /* serve from dir */
	/* .def */			"gb3mba.html",	/* default filename */
	/* .protocol */			NULL,
	/* .cgienv */			NULL,
	/* .extra_mimetypes */		NULL,
	/* .interpret */		NULL,
	/* .cgi_timeout */		0,
	/* .cache_max_age */		0,
	/* .auth_mask */		0,
	/* .cache_reusable */		0,
	/* .cache_revalidate */		0,
	/* .cache_intermediaries */	0,
	/* .cache_no */			0,
	/* .origin_protocol */		LWSMPRO_FILE,	/* files in a dir */
	/* .mountpoint_len */		1,		/* char count */
	/* .basic_auth_login_file */	NULL,
};

/* this handles the sining (ctrl-C and will indicate we should stop our infinte loop */
void sigint_handler(int sig)
{
        if (sig) interrupted = 1;
}

struct lws_context *context;

/* so here we are in main ... */
int websocket_create()
{
	/* in order to set up a websocket, we first create a contect structure (called info in this case) */
        /* once we have filled in all the fields we do a call to lws_create_context  that will set it all */
        /* up for us and install all the handlers                                                         */
        struct lws_context_creation_info info;
	/* n is used to store our return code from the service routine, it can be -ve so int not uint */
        int n = 0;
        int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;

        /* this tells us to go to the signal handler if we press ctrl-c */
	signal(SIGINT, sigint_handler);

	/* here we setup the level of loogin we are going to do ... see logs variable declared above */
        lws_set_log_level(logs, NULL);
	/* this logs a message to the user telling them what to do with the program */
        lwsl_user("LWS Meteor Beacon ws server | visit http://localhost:7681 (-s = use TLS / https)\n");

        /* now we are ready to create out    info   structure that we will send into the   create   function */
        memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = 7681;   /* this is the URL port that we are going to set up our socket on */
	info.mounts = &mount; /* this tells us where to find the http files, see the mount structure above */
	info.protocols = protocols; /* this is where the real work is being done ... see the protocols.c file */
	info.vhost_name = "localhost"; /* this tells us what name to use for our nost ... TBD */
        /* this seems to be just setting up some flags */
	info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	/* so now it is all filled in we simple pass it into the create function and it will return a */
        /* context or an error (null)                                                                    */
        context = lws_create_context(&info);
	if (!context) {
                /* if we got null back then we have failed and give up */
		lwsl_err("lws init failed\n");
		return 1;
	}

        /* if all has gone well, then we infinite loop calling   service   . We pass in out */
        /* conects and a timeout .... that seems to be 0 for most of these examples         */
        /* note the way out of this loop is for us to press ctrl-c and the sigint will      */
        /* handle it for us (by setting interrupted to 1). Or service might return an error */
        /* i.e n<0                                                                          */
	while (n >= 0 && !interrupted) n = lws_service(context, 0);

	/* if we get ctrl-c then we clean up with   destroy   and we are all done           */
        lws_context_destroy(context);

	return 0;
}
