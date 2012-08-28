import std;

backend testapp {
	.host = "[::1]";
	.port = "8080";
}

sub vcl_recv {
	if (!req.http.X-Origin) {
		set req.http.X-Origin = server.identity + req.xid;
	}
	if (req.url !~ "^/ts-processor" && req.esi_level == 0) {
		std.log("track " + req.xid + " ot_tcv=0.1-SNAPSHOT");
		std.log("track " + req.xid + " mandantenID=1008");
		set req.http.X-Correlation-ID =
			std.random(0, 340282366920938463463374607431768211456);
		std.log("track " + req.xid + " correlationID=" +
			req.http.X-Correlation-ID);
		if (req.http.Cookie ~ "JSESSIONID") {
			std.log("track " + req.xid + " me_sid=" +
				regsub(req.http.Cookie,
				       "^.*\bJSESSIONID\s*=(\S+)\b.*$",
				       "\1"));
		}
		if (req.http.Cookie ~ "VisitorID") {
			set req.http.X-VID = regsub(req.http.Cookie,
						"^.*\bVisitorID\s*=(\S+)\b.*$",
						"\1");
                        std.log("track " + req.xid + " ot_vid=" +
                                req.http.X-VID + "&me_vid=" + req.http.X-VID +
				"&ot_NewVid=false&me_vid=false");
                }
		else {
			std.log("track " + req.xid +
				" ot_vid=&me_vid=&ot_NewVid=true&me_newvid=true");
		}
		/* IPv6 ?? */
		set req.http.X-Anon-IP = regsub(client.ip, "\d+$", "XXX");
		std.log("track " + req.xid + " ot_ip=" + req.http.X-Anon-IP +
			"&me_ip=" + req.http.X-Anon-IP);

		if (req.http.User-Agent ~ ".") {
			std.log("track " + req.xid + " ot_agent=" +
				regsuball(req.http.User-Agent, " ", "%20") +
                                "&me_agent=" +
				regsuball(req.http.User-Agent, " ", "%20"));
		}
		if (req.http.Referer ~ ".") {
			std.log("track " + req.xid + " me_referrer=" +
				req.http.Referer);
		}
		if (req.http.Accept-Language ~ ".") {
			std.log("track " + req.xid + " ot_lang=" +
				req.http.Accept-Language);
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

sub vcl_fetch {
	if (req.http.X-Origin) {
		set beresp.http.X-Origin = req.http.X-Origin;
	}
	set beresp.do_esi = true;
}

sub vcl_error {
	if (obj.status == 204 && obj.response == "track") {
		synthetic {""};
		return(deliver);
	}
}

