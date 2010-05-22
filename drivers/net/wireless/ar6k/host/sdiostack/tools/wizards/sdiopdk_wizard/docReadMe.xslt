<?xml version="1.0" encoding="UTF-8" ?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
xmlns:msxsl="urn:schemas-microsoft-com:xslt"
xmlns:tns="urn:thisnamespace:tns"
xmlns:xtn="extl"
xmlns:lxslt="http://xml.apache.org/xslt"
    extension-element-prefixes="xtn"
	exclude-result-prefixes="tns xsl msxsl xtn">

<!--Copyright 2004-2005 Atheros Communications, Inc.-->
<!--XSL used to create ReadMe top-level documentaion-->

<!-- display titles with (tm) and (r) replaced with entities-->
<xsl:param name="title1"	select="CodePalette"/>
<xsl:param name="title2" 	select="Help"/>
<!-- The help file link -->
<xsl:param name="helpFile"/>
<!-- The name of the company developing this-->
<xsl:param name="companyName"/>
<!-- The name of the company's product-->
<xsl:param name="companyProductName"/>
<!--the compile date string of this transform -->
<xsl:param name="CompileDate" select="'DateUnknown'"/>
<!--the compile date string of this transform -->
<xsl:param name="CompileTime" select="'TimeUnknown'"/>
<!--the version of the wizard that create this file -->
<xsl:param name="version" />
<!--the version increment of the wizard that create this file -->
<xsl:param name="versionIncrement" />
<!--the version of the OS this file was built for -->
<xsl:param name="targetOSversion" />
<!--tag line for top bar -->
<xsl:param name="taglineupper"/>
<!--link for tag line for top bar -->
<xsl:param name="taglineupperlink"/>
<!--tag line for bottom bar -->
<xsl:param name="taglinelower"/>
<!--link for tag line for lower bar -->
<xsl:param name="taglinelowerlink"/>
<!--copyright year -->
<xsl:param name="copyrightyear" select="2004"/>

<xsl:output method="html" />

<xsl:template match="/">
    <xsl:apply-templates select="documentationPackage"/>
</xsl:template>

<xsl:template match="documentationPackage">
<xsl:text disable-output-escaping="yes">
&lt;!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN"&gt;
</xsl:text>
<html><!-- InstanceBegin template="/Templates/Reference_Function.dwt" codeOutsideHTMLIsLocked="false" -->
<head>
<!-- InstanceBeginEditable name="doctitle" -->
<title><xsl:value-of select="$title2"/></title>
<!-- InstanceEndEditable -->
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1"/>

<!-- InstanceBeginEditable name="head" --><!-- InstanceEndEditable -->

<link href="CodeTHelp.css" rel="stylesheet" type="text/css"/>
</head>
<body bgcolor="#FFFFFF" leftmargin="0" topmargin="0" marginwidth="0" marginheight="0">
    <a name="TopTopic"/>
    <table width="100%" border="0" cellspacing="0" cellpadding="4">
    <tr> 
        <td width="40%" rowspan="2" bgcolor="#0000FF">
            <a href="http://www.codetelligence.com" target="_blank">
                <img src="Images/codetelligence_lrg.gif" name="image" width="252" height="40" border="0"/>
            </a><br/>
            <xsl:if test="$taglineupper">
                <a target="_blank"> <xsl:attribute name="href"><xsl:value-of select="$taglineupperlink"/></xsl:attribute> 
                <font color="#FFFFFF"><em><strong><font face="Arial, Helvetica, sans-serif">
                    <xsl:value-of select="$taglineupper"/></font></strong></em></font>
                </a>
            </xsl:if>
        </td>
        <td width="50%" height="62" bgcolor="#0000FF">
        <font color="#FFFFFF" size="5" face="Arial, Helvetica, sans-serif"><strong><xsl:call-template name="insert.specialChars"><xsl:with-param name="text" select="$title1"/></xsl:call-template><br/>
<xsl:call-template name="insert.specialChars"><xsl:with-param name="text" select="$title2"/></xsl:call-template>
</strong></font>
        </td>
    </tr>
    </table>
    <div align="center"><font size="+1" face="Arial, Helvetica, sans-serif"><br/>
    <xsl:if test="$helpFile">
        <u onmouseover='this.style.cursor = "hand"'>
            <xsl:attribute name="onclick">javascript:window.showHelp("ms-its:<xsl:value-of select="substring-after(translate($helpFile, '\','/'), 'file:/')"/>");</xsl:attribute><font color='#0000FF'>See Help file for build and compile information.</font>
        </u>
        <br/>
    </xsl:if>
    Created for:<xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text><xsl:value-of select="$companyName"/><xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text>-<xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text><xsl:value-of select="$companyProductName"/><br/>
    <font size="-1">Created on:<xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text><xsl:value-of select="$CompileTime"/><xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text><xsl:value-of select="$CompileDate"/><xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text><xsl:if test="$version">version:<xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text><xsl:value-of select="$version"/>-<xsl:value-of select="$versionIncrement"/></xsl:if></font><br/>
    <xsl:if test="$targetOSversion">
        <font face="Arial, Helvetica, sans-serif"><xsl:value-of select="$targetOSversion"/></font><br/>
    </xsl:if>
    <b>Generated Files</b></font></div>
    <table width="100%" border="1" cellspacing="10" cellpadding="0">
            <xsl:apply-templates select="documentation"/>
    <!--add the fixed items -->
    </table>
    <xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text><br/>
    <table width="100%" border="1" cellspacing="0" cellpadding="2" >
    <tr> <td height="50">
            <xsl:if test="$taglinelower">
                <b><a target="_blank"> <xsl:attribute name="href"> <xsl:value-of select="$taglinelowerlink"/></xsl:attribute> 
                <font size="+1" color="#FFFFFF"><em><strong><font face="Arial, Helvetica, sans-serif">
                    <xsl:value-of select="$taglinelower"/></font></strong></em></font>
                </a></b>
            </xsl:if>
    <xsl:text disable-output-escaping='yes'>&amp;nbsp;</xsl:text><div align="right"><a href="#TopTopic">Back to top</a></div></td></tr>
    <tr bgcolor="#0000FF"> 
        <td> 
        <font color="#FFFFFF" face="Arial, Helvetica, sans-serif"><strong><xsl:text disable-output-escaping="yes">&amp;copy;</xsl:text><xsl:value-of select="$copyrightyear"/> </strong></font><font color="#FFFFFF" face="Arial, Helvetica, sans-serif"><strong>Code<em>telligence</em>,
        Inc.   </strong></font><a href="http://www.codetelligence.com" target="_blank"><font color="#FFFFFF" face="Arial, Helvetica, sans-serif">www.codetelligence.com</font></a></td>
    </tr>
    </table>
</body>
<!-- InstanceEnd --></html>
</xsl:template>

<xsl:template match="documentation">
    <tr>
    <td>
    <xsl:value-of select="file"/>
    </td>
    <td>
    <xsl:value-of select="abstract"/>
    </td>
    <td><font size="-1">
        <xsl:call-template name="make.seeReferenceFileLink">
	        <xsl:with-param name="link" select="file"/>
	    </xsl:call-template><xsl:text disable-output-escaping='yes'>&amp;nbsp;</xsl:text>
	    <a> <xsl:attribute name="href"><xsl:call-template name="make.overviewFileName">
	    <xsl:with-param name="fileWithExt" select="file"/>
	</xsl:call-template></xsl:attribute>
	    overview
	</a></font><xsl:text disable-output-escaping='yes'>&amp;nbsp;</xsl:text>
	<xsl:call-template name="make.todo"/></td>
    </tr>
    <!--add the fixed items -->
</xsl:template>


<xsl:template match="function">
</xsl:template>

<!-- make TODO display -->
<xsl:template name="make.todo">
    <!-- see if we have an TODOs for this item-->
    <xsl:variable name="todos" select="todo"/>
    <xsl:if test="$todos">
        <!-- put in show/hide buttons -->
        <span>
		    <xsl:attribute name="id"><xsl:value-of select="normalize-space(file)"/>Button</xsl:attribute>
		    <xsl:attribute name="onClick">var div = window.document.getElementById('<xsl:value-of select="normalize-space(file)"/>Div');div.style.display = 'inline';var div2 = window.document.getElementById('<xsl:value-of select="normalize-space(file)"/>Button');div2.style.display ='none';</xsl:attribute>
		    <u><font color="#0000FF" size="-1">Show TODOs&gt;</font></u></span> 
  		    <span style="display:none"><xsl:attribute name="id"><xsl:value-of select="normalize-space(file)"/>Div</xsl:attribute>
  		    <div>
		        <xsl:attribute name="id"><xsl:value-of select="normalize-space(file)"/>Button2</xsl:attribute>
		        <xsl:attribute name="onClick">var div = window.document.getElementById('<xsl:value-of select="normalize-space(file)"/>Button');div.style.display = 'inline';var div2 = window.document.getElementById('<xsl:value-of select="normalize-space(file)"/>Div');div2.style.display ='none';</xsl:attribute>
	    <u><font color="#0000FF" size="-1">Hide TODOs&gt;</font><br/></u>
        <!-- first display the items before any functions -->
        <xsl:for-each select="todo[
                    count(preceding-sibling::function) = 0]">                                               
            <pre>TODO:<xsl:value-of select="."/></pre>
        </xsl:for-each>
        <!-- the display the items that are function associated -->
        <xsl:for-each select="function">
            <xsl:variable name="functionName" select="functionName"/>
            <xsl:for-each select="following-sibling::todo[
                        count(preceding-sibling::function[1] | current()) = 1]">                                               
                <pre>TODO:<xsl:value-of select="$functionName"/>: <xsl:value-of select="."/></pre>
            </xsl:for-each>
        </xsl:for-each>
	    </div>
	    </span>        
        
    </xsl:if>
</xsl:template>

<xsl:template match="todo">
    <xsl:value-of select="."/><br/>
</xsl:template>


<!-- get the function anchor name -->
<xsl:template name="get.anchorName">
   <!-- we use the section name(as the page) and function position, 1 based, to make the position tag names -->
   <!-- functionNode - node of the functionName-->
    <xsl:param name="functionNode"/>PAGE_<xsl:value-of select="translate($functionNode/../../@name, '/.', '__')"/>FUNC_<xsl:choose><xsl:when test="count(/*/descendant::functionName[text() = $functionNode/text()]) > 1"><xsl:value-of select="$functionNode"/><xsl:value-of select="count($functionNode/../preceding-sibling::*/functionName[$functionNode])"/></xsl:when><xsl:otherwise><xsl:value-of select="$functionNode"/></xsl:otherwise></xsl:choose> 
</xsl:template>

<!-- get the next link name -->
<xsl:template name="get.nextLinkName">
    <xsl:choose>
        <xsl:when test="following-sibling::node()[local-name()='function']"><!-- we have a sibling in this file, return local link -->#<xsl:call-template name="get.anchorName">
                <xsl:with-param name="functionNode" select="following-sibling::node()[local-name()='function'][1]/functionName"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:when test="../following-sibling::node()[local-name()='documentation']"><!-- we have a new file as a link --><xsl:call-template name="make.fileName">
                        <xsl:with-param name="fileWithExt" select="../following-sibling::node()[local-name()='documentation'][1]/file"/>
            </xsl:call-template>#<xsl:call-template name="get.anchorName">
                <xsl:with-param name="functionNode" select="../following-sibling::node()[local-name()='documentation'][1]/function[last()]/functionName"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
            <!-- no following link -->
        </xsl:otherwise>
    </xsl:choose>
    <!--xsl:value-of select="../documentation/@name"/-->
</xsl:template>

<!-- get the next document link name -->
<xsl:template name="get.nextDocumentLinkName">
    <xsl:choose>
        <xsl:when test="following-sibling::node()[local-name()='documentation']"><!-- we have a new file as a link --><xsl:call-template name="make.overviewFileName">
                        <xsl:with-param name="fileWithExt" select="following-sibling::node()[local-name()='documentation'][1]/file"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
            <!-- no following link -->
        </xsl:otherwise>
    </xsl:choose>
    <!--xsl:value-of select="../documentation/@name"/-->
</xsl:template>

<!-- get the previous link name  -->
<xsl:template name="get.previousLinkName">
    <xsl:choose>
        <xsl:when test="preceding-sibling::node()[local-name()='function']"><!-- we have a sibling in this file, return local link -->#<xsl:call-template name="get.anchorName">
                <xsl:with-param name="functionNode" select="preceding-sibling::node()[local-name()='function'][1]/functionName"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:when test="../preceding-sibling::node()[local-name()='documentation']"><!-- we have a new file as a link --><xsl:call-template name="make.fileName">
                        <xsl:with-param name="fileWithExt" select="../preceding-sibling::node()[local-name()='documentation'][1]/file"/>
            </xsl:call-template>#<xsl:call-template name="get.anchorName">
                <xsl:with-param name="functionNode" select="../preceding-sibling::node()[local-name()='documentation'][1]/function[last()]/functionName"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
            <!-- no following link -->
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<!-- get the previous document link name  -->
<xsl:template name="get.previousDocumentLinkName">
    <xsl:choose>
        <xsl:when test="preceding-sibling::node()[local-name()='documentation']"><!-- we have a new file as a link --><xsl:call-template name="make.overviewFileName">
                        <xsl:with-param name="fileWithExt" select="preceding-sibling::node()[local-name()='documentation'][1]/file"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
            <!-- no following link -->
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<!-- make the next link-->
<xsl:template name="make.nextLink">
    <xsl:variable name="tagName">
    <xsl:call-template name="get.nextDocumentLinkName"/></xsl:variable>
    <xsl:if test="string-length($tagName) > 0">
        <a><xsl:attribute name="href"><xsl:value-of select="$tagName"/></xsl:attribute><img src="Images/rightarrow.gif" width="27" height="32" border="0"/></a>
    </xsl:if>
</xsl:template>

<!-- make the previous link-->
<xsl:template name="make.previousLink">
    <xsl:variable name="tagName">
    <xsl:call-template name="get.previousDocumentLinkName"/></xsl:variable>
    <xsl:if test="string-length($tagName) > 0">
        <a><xsl:attribute name="href"><xsl:value-of select="$tagName"/></xsl:attribute><img src="Images/leftarrow.gif" width="27" height="32" border="0"/></a>
    </xsl:if>
</xsl:template>

<!-- make see also link-->
<xsl:template name="make.seeReferenceFileLink">
    <xsl:param name="link"/>
    <xsl:variable name="linkName" select="normalize-space($link)"/>
    <xsl:variable name="linkNameNode" select="/documentationPackage/documentation/file[normalize-space(text())=$linkName]"/>
    <xsl:choose>
        <xsl:when test="$linkNameNode">
            <xsl:if test="$linkNameNode/../function"><!-- need at least one function to have a reference section-->
                <a><xsl:attribute name="href">
                        <xsl:call-template name="make.fileName">
                            <xsl:with-param name="fileWithExt" select="$linkNameNode/../@name"/>
                        </xsl:call-template>
                    </xsl:attribute>
                    reference
                </a>
            </xsl:if>    
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="$link"/>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<!-- make file name, replace the extension with HTM and put in doc directory -->
<xsl:template name="make.fileName">
    <xsl:param name="fileWithExt"/><!-- the file name with an extension --><xsl:choose>
            <xsl:when test="string-length(substring-after($fileWithExt, '/')) > 0">
                <xsl:value-of select="substring-after(concat(substring-before(normalize-space($fileWithExt), '.'), '.Xhtm'), '/' )"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="concat(translate(normalize-space($fileWithExt), '.', '_'), '.XXhtm')"/>
            </xsl:otherwise>
        </xsl:choose>
</xsl:template>
<!-- make overviewfile name, replace the extension with HTM, add _overview and put in doc directory -->
<xsl:template name="make.overviewFileName">
    <xsl:param name="fileWithExt"/><!-- the file name with an extension --><xsl:choose>
            <xsl:when test="string-length(substring-after($fileWithExt, '/')) > 0">
                <xsl:value-of select="substring-after(concat(substring-before(normalize-space($fileWithExt), '.'), '_overview.htm'), '/' )"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="concat(translate(normalize-space($fileWithExt), '.', '_'), '_overview.htm')"/>
            </xsl:otherwise>
        </xsl:choose>
</xsl:template>


<xsl:template name="insert.specialChars">
    <xsl:param name="text"/>
    <xsl:variable name="firstChange">
        <xsl:call-template name="replace.chars">
            <xsl:with-param name="text" select="$text"/>
            <xsl:with-param name="replace">(tm)</xsl:with-param>
            <xsl:with-param name="with">&#8482;</xsl:with-param>
            <xsl:with-param name="makeSmall" select="'1'"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:call-template name="replace.chars">
        <xsl:with-param name="text" select="$firstChange"/>
        <xsl:with-param name="replace">(r)</xsl:with-param>
        <xsl:with-param name="with">&#174;</xsl:with-param>
        <xsl:with-param name="makeSmall" select="'1'"/>
    </xsl:call-template>
</xsl:template>

  <xsl:template name="replace.chars">
    <xsl:param name="text"/>
    <xsl:param name="replace"/>
    <xsl:param name="with"/>
    <xsl:param name="makeSmall" select="'1'"/>

    <xsl:choose>
      <xsl:when test="string-length($replace) = 0">
        <xsl:value-of select="$text"/>
      </xsl:when>
      <xsl:when test="contains($text, $replace)">

    <xsl:variable name="before" select="substring-before($text, $replace)"/>
    <xsl:variable name="after" select="substring-after($text, $replace)"/>

    <xsl:value-of select="$before"/>
    <xsl:choose>
        <xsl:when test="$makeSmall = '1'">
            <font size="smaller"><xsl:value-of select="$with"/></font>
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="$with"/>
        </xsl:otherwise>
    </xsl:choose>
        <xsl:call-template name="replace.chars">
            <xsl:with-param name="text" select="$after"/>
            <xsl:with-param name="replace" select="$replace"/>
            <xsl:with-param name="with" select="$with"/>
            <xsl:with-param name="makeSmall" select="$makeSmall"/>
    </xsl:call-template>
      </xsl:when> 
      <xsl:otherwise>
        <xsl:value-of select="$text"/>  
      </xsl:otherwise>
    </xsl:choose>            
  </xsl:template>

</xsl:stylesheet>