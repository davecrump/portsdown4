var float_buffer;
let colourMap;
let wf;
let sp;

function get_appropriate_ws_url(extra_url)
{
	var pcol;
	var u = document.URL;

	/*
	 * We open the websocket encrypted if this page came on an
	 * https:// url itself, otherwise unencrypted
	 */

	if (u.substring(0, 5) === "https") {
		pcol = "wss://";
		u = u.substr(8);
	} else {
		pcol = "ws://";
		if (u.substring(0, 4) === "http")
			u = u.substr(7);
	}

	u = u.split("/");

	/* + "/xxx" bit is for IE10 workaround */

	return pcol + u[0] + "/" + extra_url;
}

function new_ws(urlpath, protocol)
{
	return new WebSocket(urlpath, protocol);
}

document.addEventListener("DOMContentLoaded", function() {
	
	var ws = new_ws(get_appropriate_ws_url(""), "Beacon-Server");
	ws.binaryType = "arraybuffer"; /* we don't want a blob */
        try {
		ws.onopen = function() {
		};
	
		ws.onmessage = function got_packet(msg) {
                    byte_buffer = new Uint8Array(msg.data);
                    size = msg.data.byteLength;
//                    document.getElementById("r").value = size.toString();
//                    a=byte_buffer[0];
//                    b=byte_buffer[1];
//                    c=byte_buffer[2];
//                    e=byte_buffer[4092];
//                    f=byte_buffer[4093];
//                    g=byte_buffer[4094];
//                    h=byte_buffer[4095];
//                    document.getElementById("r").value = size.toString() + ": " +
//                                                         a.toString()   + ", " +
//                                                         b.toString()   + ", " +
//                                                         c.toString()   + ", " +
//                                                         d.toString()   + ", " +
//                                                         e.toString()   + ", " +
//                                                         f.toString()   + ", " +
//                                                         g.toString()   + ", " +
//                                                         h.toString();
//                console.log("rx:",a,b,c);
                add_into_buffer(byte_buffer);
                wf.addLine(byte_buffer);
	}
		ws.onclose = function(){
		};
	} catch(exception) {
		alert("<p>Error " + exception);  
	}
	
colourMap = new ColourMap();
wf = new Waterfall("waterfall", colourMap);

//  sp = new Spectrum("spectrum", colourMap);
}, false);

