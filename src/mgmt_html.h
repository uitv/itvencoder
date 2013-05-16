static gchar *index_html = "<html>\n"
"<head>\n"
"<title>iTVEncoder web configure</title>"
"<link href=\"/gui.css\" rel=\"stylesheet\" type=\"text/css\"/>"
"<script  language=javascript type=\"text/javascript\">\n"
"function getConfigure() {\n;"
"xmlhttp = new XMLHttpRequest();\n"
"xmlhttp.onreadystatechange = processRequest;\n"
"xmlhttp.open (\"GET\", \"/configure/\", true);\n"
"xmlhttp.send ();\n"
"}\n"
/* process request responseXML. */
"function processRequest () {\n"
"if (xmlhttp.readyState != 4) return;\n"
"root = xmlhttp.responseXML.getElementsByTagName (\"root\");\n"
"server = xmlhttp.responseXML.getElementsByTagName (\"server\")[0];\n"
"channels = xmlhttp.responseXML.getElementsByTagName (\"channels\")[0];\n"
"var itvencoder = document.getElementById (\"itvencoder\");\n"
/* server */
"var server = document.getElementById (\"server\");\n"
"var v = server.getElementsByTagName (\"var\");"
"server.innerHTML = server.innerHTML + \"<div class=server><ul>Server</ul>\";\n"
"for (var i = 0; i < v.length; i++) {\n"
"    server.innerHTML = server.innerHTML + \"<li>\" + v[i].getAttribute (\"name\") + \":\" + v[i].textContent + \"</li>\";\n"
"}\n"
"return;"
"itvencoder.innerHTML = itvencoder.innerHTML + \"</div>\";\n"
/* channels */
"itvencoder.innerHTML = itvencoder.innerHTML + \"<ul>Channels</ul>\";\n"
"c = channels.firstElementChild;\n"
"do {\n"
"    v = c.getElementsByTagName (\"var\");\n"
"    itvencoder.innerHTML = itvencoder.innerHTML + \"<ul>Channel</ul>\";\n"
"    onboot = c.firstElementChild;\n"
"    itvencoder.innerHTML = itvencoder.innerHTML + \"<li>\" + onboot.getAttribute (\"name\") + \"</li>\";\n"
     /* channel source */
"    var source = c.getElementsByTagName (\"source\")[0];\n"
"    itvencoder.innerHTML = itvencoder.innerHTML + \"<ul>Source</ul>\";\n"
"    var v1 = source.getElementsByTagName (\"var\");\n"
"    for (var i = 0; i < v1.length; i++) {\n"
"        itvencoder.innerHTML = itvencoder.innerHTML + \"<li>\" + v1[i].getAttribute (\"name\") + \":\" + v1[i].textContent + \"</li>\";\n"
"    }\n"
     /* channel encoder */
"    var encoder = c.getElementsByTagName (\"encoder\")[0];\n"
"    itvencoder.innerHTML = itvencoder.innerHTML + \"<ul>encoders</ul>\";\n"
"    var  e = encoder.firstElementChild;\n"
"    do {\n"
"        v1 = e.getElementsByTagName (\"var\");\n"
"        itvencoder.innerHTML = itvencoder.innerHTML + \"<ul>encoder</ul>\";\n"
"        for (var i = 0; i < v1.length; i++) {\n"
"            itvencoder.innerHTML = itvencoder.innerHTML + \"<li>\" + v1[i].getAttribute (\"name\") + \":\" + v1[i].textContent + \"</li>\";\n"
"        }\n"
"        e = e.nextElementSibling;\n"
"    } while (e != null);\n"
"    c = c.nextElementSibling;\n"
"} while (c != null);\n"
"//for (var i = 0; i < channels.childElementCount; i++) {\n"
"}\n"
"</script>\n"
"</head>\n"
"<body onload = \"getConfigure();\" TEXT-ALIGN=center;>\n"
"<div class=fixed-top>\n"
"<header>iTVEncoder Web Management</header>\n"
"</div>\n"
"<div id=itvencoder class=itvencoder>\n"
"<div id=server class=server>\n"
"</div>\n"
"</div>\n"
"</body>\n";


static gchar *gui_css = ""
".fixed-top {\n"
"    position: fixed;\n"
"    margin: 0 0;\n"
"    top: 0;\n"
"    left: 0;\n"
"    padding: 0 0;\n"
"    width: 100%;\n"
"    background-color: #f5f5f5;\n"
"}"
"header {\n"
"    font-size: 40px;\n"
"    text-align: center;\n"
"}\n"
".itvencoder {\n"
"    top: 100;\n"
"    margin: 50;\n"
"    width: 100%;\n"
"    text-align: center;\n"
"}\n";