<%@page language="java"%>
<jsp:useBean id="d" scope="application" class="de.otto.lhotse.track.rdr.testapp.Data"/>

<h2>Include level 2</h2>

<p>This included page simulates generation of marketing entry tags.</p>
<esi:include src="/ts-rcv?me_campid=<jsp:getProperty name="d" property="hexInt"/>"/>
<esi:include src="/ts-rcv?me_entry=<jsp:getProperty name="d" property="entryType"/>"/>
<esi:include src="/ts-rcv?me_creaid=<jsp:getProperty name="d" property="hexInt"/>"/>
<esi:include src="/ts-rcv?me_publid=<jsp:getProperty name="d" property="hexInt"/>"/>
<esi:include src="/ts-rcv?me_iwl=<jsp:getProperty name="d" property="hexInt"/>"/>
<esi:include src="/ts-rcv?me_actseaterm=<jsp:getProperty name="d" property="searchTerms"/>"/>
<esi:include src="/ts-rcv?me_actseaeng=<jsp:getProperty name="d" property="searchEngine"/>"/>
<esi:include src="/ts-rcv?me_kochannel=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kopartn=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kosubpartn=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kocampna=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kosemkon=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kosemsor=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kosemcam=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kosemadg=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kosemkey=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kosemmat=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kopsmsor=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kopsmart=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_konltyp=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_konlvar=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_konlfla=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_konlecm=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kokoopla=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kowerbem=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kocreatyp=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kocreafor=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kocreathe=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_kocreamot=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_origdom=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_gootid=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_psmasid=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_psmart=<jsp:getProperty name="d" property="string"/>"/>
<esi:include src="/ts-rcv?me_hkz=<jsp:getProperty name="d" property="string"/>"/>
