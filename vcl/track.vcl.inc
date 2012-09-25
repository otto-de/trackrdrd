import std;
import urlcode;

sub vcl_recv_track {
        # Track URLs, Cookie and User-Agent headers
        # Exceptions: ESI-Includes, the TS processor, status calls,
        #             and static resources
	if (req.esi_level == 0 &&
            req.url !~
   "^/(?:ts-processor|[^/]+(?:/api)?/internal/status)|\.(?:gif|jpe?g|swf|png|css|js)$"
           ) {
                std.log("track " + req.xid + " url=" + urlcode.encode(req.url));
		if (req.http.Cookie) {
			std.log("track " + req.xid + " cookie="
                                + urlcode.encode(req.http.Cookie));
		}
		if (req.http.User-Agent) {
			std.log("track " + req.xid + " ua="
                                + urlcode.encode(req.http.User-Agent));
		}
                if (req.http.X-Origin) {
                        std.log("track " + req.xid + " X-Origin="
                                + urlcode.encode(req.http.X-Origin));
                }
		if (req.http.X-Forwarded-For) {
			std.log("track " + req.xid + " X-Forwarded-For="
				+ urlcode.encode(req.http.X-Forwarded-For));
		}
	}
	if (req.url ~ "^/ts-rcv") {
		if (req.url ~ "\?") {
			std.log("track " + req.xid + " " +
                        	regsub(req.url, "^.+\?(.+)$", "\1"));
		}
		error 204 "track";
	}
}

sub vcl_track_response {
	if (obj.status == 204 && obj.response == "track") {
		synthetic {""};
		return(deliver);
	}
}

