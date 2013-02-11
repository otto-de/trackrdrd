# VMOD std *must* be imported by the including VCL
import urlcode;
import header;

sub vcl_recv_track {
        # Track URLs, Cookie and User-Agent headers
        # Exceptions: ESI-Includes, the TS processor, status calls,
        #             and static resources
	if (req.esi_level == 0 &&
            req.url !~
   "^/(?:ts-processor|[^/]+(?:/api)?/internal/status)|\.(?:gif|jpe?g|swf|png|css|js|ico)$"
           ) {
		if (! req.url ~ "^/ts-rcv") {
                	std.log("track " + req.xid +
                        	" url=" + urlcode.encode(req.url));
		}
		if (req.http.Cookie) {
                        if (req.http.Cookie ~ "visitorId"
                            && req.http.Cookie ~ "BrowserId") {
			    std.log("track " + req.xid + " http_Cookie="
                                    + urlcode.encode(req.http.Cookie));
                        }
                        else {
                             set req.http.X-VisitorId
                                 = header.get(req.http.Cookie, "visitorId");
                             set req.http.X-BrowserId 
                                 = header.get(req.http.Cookie, "BrowserId");
                             if (req.http.X-VisitorId) {
			        std.log("track " + req.xid + " visitorId="
                                        + urlcode.encode(req.http.X-VisitorId));
                             }
                             if (req.http.X-BrowserId) {
			        std.log("track " + req.xid + " BrowserId="
                                        + urlcode.encode(req.http.X-BrowserId));
                             }
                             unset req.http.X-VisitorId;
                             unset req.http.X-BrowserId;
                        }
		}
		if (req.http.User-Agent) {
			std.log("track " + req.xid + " http_User-Agent="
                                + urlcode.encode(req.http.User-Agent));
		}
                if (req.http.X-Origin) {
                        std.log("track " + req.xid + " http_X-Origin="
                                + urlcode.encode(req.http.X-Origin));
                }
		if (req.http.X-Forwarded-For) {
			std.log("track " + req.xid + " http_X-Forwarded-For="
				+ urlcode.encode(req.http.X-Forwarded-For));
		}
		if (req.http.X-Forwarded-Proto) {
			std.log("track " + req.xid + " http_X-Forwarded-Proto="
				+ urlcode.encode(req.http.X-Forwarded-Proto));
		}
		if (req.http.X-Real-IP) {
			std.log("track " + req.xid + " http_X-Real-IP="
				+ urlcode.encode(req.http.X-Real-IP));
		}
		if (req.http.Host) {
			std.log("track " + req.xid + " http_Host="
				+ urlcode.encode(req.http.Host));
		}
	}
	if (req.url ~ "^/ts-rcv") {
		if (req.url ~ "\?") {
			std.log("track " + req.xid + " " +
                        	regsub(req.url, "^.+\?(.+)$", "\1"));
		}
		set req.url = "/ts-processor/204";
		unset req.http.Cookie;
	}
}

sub vcl_fetch_track {
        if (req.url == "/ts-processor/204" && beresp.status == 204) {
                set beresp.ttl = 4294967295s;
                return(deliver);
        }
}

