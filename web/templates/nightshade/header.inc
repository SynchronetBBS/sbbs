<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">

 <!-- $Id: header.inc,v 1.3 2006/02/27 01:02:24 runemaster Exp $ -->

<head>
<title>%%title%%</title>
	<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1" />
	@@JS:if(http_request.virtual_path=="/nodelist.ssjs")'<meta http-equiv="refresh" content="30" />';@@
	<link rel="stylesheet" href="@@Theme_CSS_File@@" type="text/css" />
</head>

<body>
<!--[if gte IE 7]>
<style type="text/css">
	html>body #navcontainer {
	margin-left: 10px;
	margin-top: 10px;
	margin-bottom: 10px;
}
html>body #more_invisible {
	margin-right: 10px;
	margin-top: 10px;
}
#sectionLinks a:link { 
	margin-left: -39px;
	margin-right: 13px;
}
#sectionLinks a:active { 
	margin-left: -39px;
	margin-right: 13px;
}
#sectionLinks a:hover { 
	margin-left: -39px;
	margin-right: 13px;
}
#sectionLinks a:visited {
	margin-left: 0px;
	margin-right: 13px;
}
#usrlist {
 	width: 90%;
}
</style>
<![endif]-->

<!--[if gte IE 5.5000]>
	<script src="/js/pngfix.js" type="text/javascript">
	</script>
<![endif]-->

	<div id="banner">
		@@name_logo@@
		<div id="greeting">
	        @@user_greeting@@
		</div>
		<div class="cleardiv"></div>
	</div>

