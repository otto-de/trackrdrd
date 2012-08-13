<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
"http://www.w3.org/TR/html4/strict.dtd">

<%@page language="java" %>
<jsp:useBean id="d" scope="application" class="de.otto.lhotse.track.rdr.testapp.Data"/>

<html>
    <head>
        <title>LHOTSE Track Reader Test Driver</title>
    </head>
<body>
<h1>LHOTSE Track Reader Test Driver</h1>
<p>This test app generates data to be written to the Varnish log, and read
from there by the tracking reader. Its functions are:</p>
<ul>
    <li>generate values for tracking tags</li>
    <li>call ESI includes for tracking</li>
    <li>call ESI includes for page structure</li>
</ul>

<h2>Outer (enclosing) page</h2>

<p>The outer page simulates generation of data presumably obtained from
    JavaScript, and includes the next page.</p>

<esi:include src="/tr/rcv?ot_browinw=<%=d.nextInt(1200)%>&ot_browinh=<%=d.nextInt(1200)%>"/>

<esi:include src="/tr/rcv?ot_dispres=<%=d.nextInt(1920)%>x<%=d.nextInt(1080)%>"/>

<esi:include src="/tr/rcv?ot_dispcol=<jsp:getProperty name="d" property="colorDepth"/>"/>

<esi:include src="/tr/rcv?ot_flash=<%=d.nextInt(12)%>&ot_java=<jsp:getProperty name="d" property="boolean"/>"/>

<esi:include src="/tr/rcv?ot_js=1.<%=d.nextInt(9)%>&ot_cookies=<jsp:getProperty name="d" property="boolean"/>"/>
 
<esi:include src="include1.jsp"/>

</body>
</html>
