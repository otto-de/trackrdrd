<%@page language="java"%>
<jsp:useBean id="d" scope="application" class="de.otto.lhotse.track.rdr.testapp.Data"/>

<h2>Include level 1</h2>

<p>This included page simulates generation of customer data (order, shopping cart,
    customer age & gender, etc.), and includes the next page.</p>

<esi:include src="/tr/rcv?ot_ArtQuantity=<%=d.nextInt(10)%>&ot_Login=<jsp:getProperty name="d" property="boolean"/>"/>
<esi:include src="/tr/rcv?ot_Bid=<jsp:getProperty name="d" property="hexInt"/>"/>
<esi:include src="/tr/rcv?ot_Oid=<jsp:getProperty name="d" property="hexInt"/>"/>
<esi:include src="/tr/rcv?ot_Login=<jsp:getProperty name="d" property="boolean"/>"/>
<esi:include src="/tr/rcv?ot_DsOptOut=<jsp:getProperty name="d" property="boolean"/>"/>
<esi:include src="/tr/rcv?ot_ktnr=<jsp:getProperty name="d" property="hexInt"/>"/>
<esi:include src="/tr/rcv?ot_custtype=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/tr/rcv?ot_age=<%=d.nextInt(80)%>&ot_sex=<jsp:getProperty name="d" property="gender"/>"/>

<esi:include src="include2.jsp"/>
