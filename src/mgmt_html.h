static gchar *index_html = "<html>\n"
"<head>\n"
"<title>iTVEncoder web configure</title>\n"
"<link href=\"/gui.css\" rel=\"stylesheet\" type=\"text/css\"/>\n"
"<script  language=javascript type=\"text/javascript\">\n"
"function getConfigure() {\n"
"    getConfigureRequest = new XMLHttpRequest();\n"
"    getConfigureRequest.onreadystatechange = processRequest;\n"
"    getConfigureRequest.open (\"GET\", \"/configure/channels/\", true);\n"
"    getConfigureRequest.send ();\n"
"}\n"
/* save changes button onclick */
"function saveChanges () {\n"
"    var data = getConfigureRequest.responseXML;\n"
"    var serializer = new XMLSerializer();\n"
"    var postConfigureRequest = new XMLHttpRequest();\n"
/* check changes. */
"    var v = data.getElementsByTagName (\"var\");\n"
"    for (var i = 0; i < v.length; i++) {\n"
"        var id = v[i].getAttribute (\"id\");\n"
"        var type = v[i].getAttribute (\"type\");\n"
"        if (type === \"option\") {\n"
"            var value = document.getElementById (id).checked;\n"
"            if (value) {\n"
"                v[i].textContent = \"yes\";\n"
"            } else {\n"
"                v[i].textContent = \"no\";\n"
"            }\n" 
"        } else {\n" /* select */
"            var value = document.getElementById (id).value;\n"
"            v[i].textContent = value;\n"
"        }\n"
"    }\n"
/* POST */
"    var postRequestXML = serializer.serializeToString(data);\n"
"    postConfigureRequest.open (\"POST\", \"/configure/channels/\", true);\n"
"    postConfigureRequest.send (postRequestXML);\n"
//"    alert (postRequestXML);\n"
"}\n"
/* restart iTVEncoder */
"function restartiTVEncoder () {\n"
"    restartRequest = new XMLHttpRequest();\n"
"    getConfigureRequest.open (\"GET\", \"/kill\", true);\n"
"    getConfigureRequest.send ();\n"
"}\n"
/* restart channel */
"function restartChannel (i) {\n"
"    restartRequest = new XMLHttpRequest();\n"
"    getConfigureRequest.open (\"GET\", \"/channel/\" + i + \"/restart\", true);\n"
"    getConfigureRequest.send ();\n"
"}\n"
/* stop channel */
"function stopChannel (i) {\n"
"    restartRequest = new XMLHttpRequest();\n"
"    getConfigureRequest.open (\"GET\", \"/channel/\" + i + \"/stop\", true);\n"
"    getConfigureRequest.send ();\n"
"}\n"
/* start channel */
"function startChannel (i) {\n"
"    restartRequest = new XMLHttpRequest();\n"
"    getConfigureRequest.open (\"GET\", \"/channel/\" + i + \"/start\", true);\n"
"    getConfigureRequest.send ();\n"
"}\n"
/* process request responseXML. */
"function processRequest () {\n"
"    if (getConfigureRequest.readyState != 4) return;\n"
"    root = getConfigureRequest.responseXML.getElementsByTagName (\"root\");\n"
"    serverXML = getConfigureRequest.responseXML.getElementsByTagName (\"server\")[0];\n"
"    channelsXML = getConfigureRequest.responseXML.getElementsByTagName (\"channels\")[0];\n"
"    var itvencoder = document.getElementById (\"itvencoder\");\n"
/* server */
#if 0
"    var server = document.getElementById (\"server\");\n"
"    var v = serverXML.getElementsByTagName (\"var\");\n"
"    var dashbox = document.createElement (\"div\");\n"
"    dashbox.setAttribute ('class', 'dashbox');\n"
"    dashbox.innerHTML = dashbox.innerHTML + \"<h3>Server</h3>\";\n"
"    for (var i = 0; i < v.length; i++) {\n"
"        var fieldgroup = document.createElement (\"div\");\n"
"        fieldgroup.setAttribute (\"class\", \"field_group\");\n"
"        var name = v[i].getAttribute (\"name\");\n"
"        var id = v[i].getAttribute (\"id\");\n"
"        fieldgroup.innerHTML = \"<label for=\\\"\" + id  + \"\\\">\" + name + \"</lable>\";\n"
"        fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"text\\\" class=\\\"text\\\" maxlength=\\\"30\\\" value=\\\"\" + v[i].textContent + \"\\\"/ >\";\n"
"        dashbox.appendChild (fieldgroup);\n"
"    }\n"
"    dashbox.innerHTML = dashbox.innerHTML + \"<button type=\\\"button\\\" onclick=saveChanges()>Save Changes</button>\";\n"
"    dashbox.innerHTML = dashbox.innerHTML + \"<button type=\\\"button\\\" onclick=restartiTVEncoder()>Restart iTVEncoder</button>\";\n"
"    server.appendChild (dashbox);\n"
#endif
/* channels */
"    var channels = document.getElementById (\"channels\");\n"
"    var c = channelsXML.firstElementChild;\n"
/* n: channel index */
"    var n = 0;\n"
"    do {\n"
"        v = c.getElementsByTagName (\"var\");\n"
"        dashbox = document.createElement ('div');\n"
"        dashbox.setAttribute ('class', 'dashbox');\n"
"        dashbox.innerHTML = dashbox.innerHTML + \"<div class=hannel>\";\n"
"        dashbox.innerHTML = dashbox.innerHTML + \"<h3>\" + c.getAttribute (\"name\") + \"</h3>\";\n"
     /* onboot */
"        onboot = c.firstElementChild;\n"
"        var fieldgroup = document.createElement (\"div\");\n"
"        fieldgroup.setAttribute (\"class\", \"field_group\");\n"
"        fieldgroup.setAttribute (\"align\", \"left\");\n"
"        var name = onboot.getAttribute (\"name\");\n"
"        var id = onboot.getAttribute (\"id\");\n"
"        if (onboot.textContent === \"yes\") {\n"
"            fieldgroup.innerHTML = \"<input id=\\\"\" + id + \"\\\"type=\\\"checkbox\\\" class=\\\"checkbox\\\" checked=\\\"checked\\\"/ >\";\n"
"        } else {\n"
"            fieldgroup.innerHTML = \"<input id=\\\"\" + id + \"\\\"type=\\\"checkbox\\\" class=\\\"checkbox\\\"/ >\";\n"
"        }\n"
"        fieldgroup.innerHTML = fieldgroup.innerHTML + \"<label for=\\\"\" + id  + \"\\\">\" + name + \"</lable>\";\n"
"        dashbox.appendChild (fieldgroup);\n"
     /* channel source */
"        var source = c.getElementsByTagName (\"source\")[0];\n"
"        var v1 = source.getElementsByTagName (\"var\");\n"
"        var pipeline = document.createElement (\"div\");\n"
"        pipeline.setAttribute (\"class\", \"pipeline\");\n"
"        pipeline.innerHTML = pipeline.innerHTML + \"<h3 align=left>Source</h3>\";\n"
"        for (var i = 0; i < v1.length; i++) {\n"
"            var fieldgroup = document.createElement (\"div\");\n"
"            fieldgroup.setAttribute (\"class\", \"field_group\");\n"
"            var name = v1[i].getAttribute (\"name\");\n"
"            var id = v1[i].getAttribute (\"id\");\n"
"            var type = v1[i].getAttribute (\"type\");\n"
"            fieldgroup.innerHTML = \"<label for=\\\"\" + id  + \"\\\">\" + name + \"</lable>\";\n"
"            if (type === \"string\") {\n"
"                fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"text\\\" class=\\\"text\\\" maxlength=\\\"30\\\" value=\\\"\" + v1[i].textContent + \"\\\"/ >\";\n"
"            } else if (type === \"option\") {\n"
"                if (v1[i].textContent === \"yes\") {\n"
"                    fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"checkbox\\\" class=\\\"checkbox\\\" checked=\\\"checked\\\"/ >\";\n"
"                } else {\n"
"                    fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"checkbox\\\" class=\\\"checkbox\\\"/ >\";\n"
"                }\n"
"            } else if (type === \"number\") {\n"
"                fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"text\\\" class=\\\"text\\\" maxlength=\\\"30\\\" value=\\\"\" + v1[i].textContent + \"\\\"/ >\";\n"
"            } else {\n"
"                var select = document.createElement (\"select\");\n"
"                select.setAttribute (\"id\", id);\n"
"                type = type.replace (\"[\", \"\");\n"
"                type = type.replace (\"]\", \"\");\n"
"                options = type.split (\",\");\n"
"                for (option in options) {\n"
"                    var str = options[option].trim();\n"
"                    if (str === v1[i].textContent) {\n"
"                        select.innerHTML = select.innerHTML + \"<option value=\\\"\" + str + \"\\\" selected>\" + str + \"</option>\";\n"
"                    } else {\n"
"                        select.innerHTML = select.innerHTML + \"<option value=\\\"\" + str + \"\\\">\" + str + \"</option>\";\n"
"                    }\n"
"                }\n"
"                fieldgroup.appendChild (select);\n"
"            }\n"
"            pipeline.appendChild (fieldgroup);\n"
"        }\n"
"        dashbox.appendChild (pipeline);\n"
     /* channel encoder */
"        var encoder = c.getElementsByTagName (\"encoders\")[0];\n"
"        var e = encoder.firstElementChild;\n"
/* m encoder index */
"        var m = 0;\n"
"        do {\n"
"            v1 = e.getElementsByTagName (\"var\");\n"
"            var pipeline = document.createElement (\"div\");\n"
"            pipeline.setAttribute (\"class\", \"pipeline\");\n"
"            pipeline.innerHTML = pipeline.innerHTML + \"<h3 align=left>\" + e.getAttribute (\"name\") + \"</h3>\";\n"
"            for (var i = 0; i < v1.length; i++) {\n"
"                var fieldgroup = document.createElement (\"div\");\n"
"                fieldgroup.setAttribute (\"class\", \"field_group\");\n"
"                var name = v1[i].getAttribute (\"name\");\n"
"                var id = v1[i].getAttribute (\"id\");\n"
"                var type = v1[i].getAttribute (\"type\");\n"
"                fieldgroup.innerHTML = \"<label for=\\\"\" + id  + \"\\\">\" + name + \"</lable>\";\n"
"                if (type === \"string\") {\n"
"                    fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"text\\\" class=\\\"text\\\" maxlength=\\\"30\\\" value=\\\"\" + v1[i].textContent + \"\\\"/ >\";\n"
"                } else if (type === \"option\") {\n"
"                    if (v1[i].textContent === \"yes\") {\n"
"                        fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"checkbox\\\" class=\\\"checkbox\\\" checked=\\\"checked\\\"/ >\";\n"
"                    } else {\n"
"                        fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"checkbox\\\" class=\\\"checkbox\\\"/ >\";\n"
"                    }\n"
"                } else if (type === \"number\") {\n"
"                    fieldgroup.innerHTML = fieldgroup.innerHTML + \"<input id=\\\"\" + id + \"\\\"type=\\\"text\\\" class=\\\"text\\\" maxlength=\\\"30\\\" value=\\\"\" + v1[i].textContent + \"\\\"/ >\";\n"
"                } else {\n"
"                    var select = document.createElement (\"select\");\n"
"                    select.setAttribute (\"id\", id);\n"
"                    type = type.replace (\"[\", \"\");\n"
"                    type = type.replace (\"]\", \"\");\n"
"                    options = type.split (\",\");\n"
"                    for (option in options) {\n"
"                        var str = options[option].trim();\n"
"                        if (str === v1[i].textContent) {\n"
"                            select.innerHTML = select.innerHTML + \"<option value=\\\"\" + str + \"\\\" selected>\" + str + \"</option>\";\n"
"                        } else {\n"
"                            select.innerHTML = select.innerHTML + \"<option value=\\\"\" + str + \"\\\">\" + str + \"</option>\";\n"
"                        }\n"
"                    }\n"
"                    fieldgroup.appendChild (select);\n"
"                }\n"
"                pipeline.appendChild (fieldgroup);\n"
"            }\n"
"            dashbox.appendChild (pipeline);\n"
"            e = e.nextElementSibling;\n"
"            m++;\n"
"        } while (e != null);\n"
"        dashbox.innerHTML = dashbox.innerHTML + \"<button type=\\\"button\\\" onclick=saveChanges()>Save Changes</button>\";\n"
"    dashbox.innerHTML = dashbox.innerHTML + \"<button type=\\\"button\\\" onclick=stopChannel(\" + n + \")>Stop Channel</button>\";\n"
"    dashbox.innerHTML = dashbox.innerHTML + \"<button type=\\\"button\\\" onclick=startChannel(\" + n + \")>Start Channel</button>\";\n"
"    dashbox.innerHTML = dashbox.innerHTML + \"<button type=\\\"button\\\" onclick=restartChannel(\" + n + \")>Restart Channel</button>\";\n"
"        channels.appendChild (dashbox);\n"
"        c = c.nextElementSibling;\n"
"        n++;\n"
"    } while (c != null);\n"
"}\n"
"</script>\n"
"</head>\n"
"<body onload = \"getConfigure();\" TEXT-ALIGN=center;>\n"
"<div class=fixed-top>\n"
"<header>iTVEncoder Web Management</header>\n"
"</div>\n"
"<div id=itvencoder class=itvencoder>\n"
"</br>\n"
"<div id=server class=server>\n"
"</div>\n"
"<div id=channels class=channels>\n"
"</div>\n"
"</div>\n"
"</body>\n";


static gchar *gui_css = ""
"header {\n"
"    font-size: 40px;\n"
"    text-align: center;\n"
"}\n"
".fixed-top {\n"
"    position: fixed;\n"
"    margin: 0 0;\n"
"    top: 0;\n"
"    left: 0;\n"
"    padding: 0 0;\n"
"    width: 100%;\n"
"    background-color: #f5f5f5;\n"
"}"
".itvencoder {\n"
"    width: 100%;\n"
"    margin-top: 50;\n"
"    text-align: center;\n"
"}\n"
".server {\n"
"    width: 600;\n"
"    margin: 0 auto;\n"
"}\n"
".channels {\n"
"    width: 600;\n"
"    margin: 0 auto;\n"
"}\n"
".channel {\n"
"    background-color: #f5f5f5;\n"
"    width: 100%;\n"
"    border-radius: 10px;\n"
"    border-shadow: 5px;\n"
"}\n"
".dashbox {\n"
"    padding: 12px 12px 12px 12px;\n"
"    margin: 0px 0px 15px 0px;\n"
"    padding: 0px 0px 5px 0px;\n"
"    border: 3px solid #999;\n"
"    background-color: #fcfcfc;\n"
"    color: #333;\n"
"    font-size: 12px;\n"
"}\n"
".dashbox h3 {\n"
"    padding: 3px 3px 3px 4px;\n"
"    margin: 0px;\n"
"    color: #222;\n"
"    background-color: #e5e5e5;\n"
"    font-size: 18px;\n"
"}\n"
".field_group {\n"
"    padding: 3px 3px 3px 4px;\n"
"    width: 100%;\n"
"    text-align: left;\n"
"}\n"
".pipeline {\n"
"    padding: 0px 0px 0px 0px;\n"
"    margin: 5px 10px 10px 10px;\n"
"    border: 1px dotted #5e5e5e;\n"
"    background-color: #fcfcfc;\n"
"    color: #333;\n"
"    font-size: 12px;\n"
"}\n"
".pipeline h3 {\n"
"    font-size: 16px;\n"
"}\n"
".field_group label,\n"
".field_group input,\n"
".field_group select {\n"
"    display: inline-block;\n"
"}\n"
".server label,\n"
".pipeline label {\n"
"    width: 200;\n"
"    font-size: 14px;\n"
"    font-weight: bold;\n"
"    text-align: right;\n"
"    margin-right: 10px;\n"
"}\n"
".pipeline h3 {\n"
"    padding: 2px 2px 2px 3px;\n"
"    margin: 0px;\n"
"    color: #222;\n"
"}\n"
;
