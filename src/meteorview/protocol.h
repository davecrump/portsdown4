int callback_beacon_server(struct lws*, enum lws_callback_reasons, void *, void *, size_t);

/* here we put all the abpve into a nice neat package that the server program can include in its setup */
#define LWS_PLUGIN_PROTOCOL_BEACON_SERVER \
	{                                 \
		"Beacon-Server",          \
		callback_beacon_server,   \
		1280,                     \
		4096,                     \
		0, NULL, 0                \
	}
