// Client-side JS functions for ecWeb v3
// echicken -at- bbs.electronicchicken.com

/*	<iframe onload="loadIFrame(<id>, width)">

	- Resizes an IFrame to the height of its contents
	- If 'width' is true, will also resize to content width */
var loadIFrame = function(id, width) {
	document.getElementById(id).style.height = document.getElementById(id).contentWindow.document.body.scrollHeight;
	if(width == 1)
		document.getElementById(id).style.width = document.getElementById(id).contentWindow.document.body.scrollWidth;
}

/*	toggleVisibility(elementID)

	Switches the "display" style of element 'elementID' between 'block'
	and 'none'. */
var toggleVisibility = function(elementID) {
	if(document.getElementById(elementID).style.display == 'none') {
		document.getElementById(elementID).style.display = 'block';
		return true;
	} else {
		document.getElementById(elementID).style.display = 'none';
		return false;
	}
}

/* As above but twiddles text as well. */
var toggle_quote = function(container)
{
	if(container.className == "hiddenQuote")
		container.className = "";
	else
		container.className = "hiddenQuote";

}

/*	httpGet(url, async)

	Makes an HTTP GET request to 'url' (url may contain GET variables).

	If 'async' is false, it will not return until a response is received from
	the server or it times out.  If 'async' is true, it will return the
	XMLHttpRequest object, and your function must monitor it for a
	ReadyStateChange. */
var httpGet = function(url, async) {
	if(async === undefined)
		async = true;
	var XMLReq = new XMLHttpRequest();
	XMLReq.open('GET', url, async);
	XMLReq.send(null);
	return (async) ? XMLReq : XMLReq.responseText;
}

/*	httpPost(url, postData, divID)

	Makes an HTTP POST request to 'url' with the string 'postData' as its post
	data (eg. 'to=all&from=echicken&subject=Pizza&body=pepperoni') and 'divID'
	being the ID of a div to push post-status info into.

	If 'async' is false, it will not return until a response is received from
	the server or it times out.  If 'async' is true, it will return the
	XMLHttpRequest object, and your function must monitor it for a
	ReadyStateChange. */
var httpPost = function(url, headers, postData, async) {
	if(async === undefined)
		async = true;
	var XMLReq = new XMLHttpRequest();
	XMLReq.open('POST', url, async);
	for(var h in headers)
		XMLReq.setRequestHeader(headers[h][0], headers[h][1]);
	XMLReq.send(postData);
	return (async) ? XMLReq : XMLReq.responseText;
}

/*	httpLoad(url, elementID, popUp, async)

	Makes an HTTP GET request to 'url' ('url' may contain GET variables.)
	
	Pushes the server's response into element with ID 'elementID'.
	
	Makes the (presumed to already be hidden) element 'popUp' visible
	(see toggleVisibility(elementID)) and pushes loading status into it,
	hides it when loading is complete.
	
	If 'async' is false, it will not return until a response is received from
	the server or it times out.  If 'async' is true, it will return the
	XMLHttpRequest object, and your function must monitor it for a
	ReadyStateChange. */
var httpLoad = function(url, elementID, popUp, async) {
	toggleVisibility(popUp);
	document.getElementById(popUp).innerHTML = "Loading ...";
	if(async === undefined)
		async = true;
	var response = httpGet(url, async);
	if(async) {
		response.onreadystatechange = function() {
			if(response.readyState == 4 && elementID != 'none') {
				document.getElementById(elementID).innerHTML = response.responseText;
				toggleVisibility(popUp);
			}
		}
	} else {
		document.getElementById(elementID).innerHTML = response;
		toggleVisibility(popUp);
	}
}

/*	httpSend(url, elementID, headers, postData, async)

	Makes an HTTP POST request to 'url' with the string 'postData' as its post
	data (eg. 'to=all&from=echicken&subject=Pizza&body=pepperoni') and 'divID'
	being the ID of a div to push post-status info into.
	
	Places sending status into "elementID" (no 'popUp' this time, as it is
	presumed that elementID contains a form that we want to do away with.)
	
	Pushes the server's response into element with ID 'elementID'.
	
	If 'async' is false, it will not return until a response is received from
	the server or it times out.  If 'async' is true, it will return the
	XMLHttpRequest object, and your function must monitor it for a
	ReadyStateChange. */
var httpSend = function(url, elementID, headers, postData, async) {
	document.getElementById(elementID).innerHTML = "Sending ...";
	if(async === undefined)
		async = true;
	var response = httpPost(url, headers, postData, async);
	if(async) {
		response.onreadystatechange = function() {
			if(response.readyState == 4 && elementID != null)
				document.getElementById(elementID).innerHTML = response.responseText;
		}
	} else {
		document.getElementById(elementID).innerHTML = response;
	}
}

/*	addReply(url, sub, thread, number)

	ecWeb forum use only. */
var addReply = function(url, sub, thread, number) {
	var response = httpGet(url + "?sub=" + sub + "&getmessage=" + number, false);
	var outDivID = "sub-" + sub + "-message-" + number + "-" + "replyBox";
	var divID = "sub-" + sub + "-thread-" + thread + "-" + number + "-reply";
	var formID = "sub-" + sub + "-thread-" + thread + "-" + number + "-reply-form";
	response = JSON.parse(response);
	var out = "<br /><br />";
	out += "<div id='" + divID + "'>";
	out += "<form id='" + formID + "'>";
	out += "<input type='hidden' name='postmessage' value='true' />";
	out += "<input type='hidden' name='sub' value='" + sub + "' />";
	out += "<input type='hidden' name='irt' value='" + number + "' />";
	out += "To:<br /><input class='border' type='text' name='to' value='" + response.header.from + "'" + ((sub == "mail")?" readonly":"") + " /><br />";
	out += "From:<br />";
	out += "<select name='from'>";
	out += "<option value='" + response.user.alias + "'>" + response.user.alias + "</option>";
	out += "<option value='" + response.user.name + "'>" + response.user.name + "</option>";
	out += "</select>";
	out += "<br />";
	out += "Subject:<br /><input class='border' type='text' name='subject' value='" + response.header.subject + "' size='80' /><br />";
	out += "<textarea class='border' name='body' cols='80' rows='10'>>" + response.body.replace(/\n/g, "\n>");
	if(typeof response.sig != "undefined")
		out += "\r\n\r\n\r\n" + response.sig;
	out += "</textarea><br />";
	out += "<input type='button' value='Submit' onclick='submitForm(\"" + url + "\", \"" + divID + "\", \"" + formID + "\")'>";
	out += "</form>";
	out += "</div>";
	document.getElementById(outDivID).innerHTML = out;
}

var addPost = function(url, sub, alias, name) {
	var divID = "sub-" + sub + "-newMsgBox";
	var formID = divID + "-newMsgForm";
	var out = "<br /><br />";
	out += "<form id='" + formID + "'>";
	out += "<input type='hidden' name='postmessage' value='true' />";
	out += "<input type='hidden' name='sub' value='" + sub + "' />";
	out += "<input type='hidden' name='irt' value='0' />";
	out += "To:<br /><input class='border' type='text' name='to' value='" + ((sub == "mail")?"":"All") + "' /><br />";
	out += "From:<br />";
	out += "<select name='from'>";
	out += "<option value='" + alias + "'>" + alias + "</option>";
	out += "<option value='" + name + "'>" + name + "</option>";
	out += "</select>";
	out += "<br />";
	out += "Subject:<br /><input class='border' type='text' name='subject' size='80' /><br />";
	out += "<textarea class='border' name='body' cols='80' rows='10'>";
	out += "</textarea><br />";
	out += "<input type='button' value='Submit' onclick='submitForm(\"" + url + "\", \"" + divID + "\", \"" + formID + "\")'>";
	out += "</form>";
	document.getElementById(divID).innerHTML = out;
}

/*	submitForm(url, elementID, formID)

	Submits the form 'formID' to 'url' and pushes the response text into the
	element with id 'elementID'. */
var submitForm = function(url, elementID, formID) {
	var theForm = document.getElementById(formID);
	var postData = "";
	for(var e in theForm.elements)
		postData += theForm.elements[e].name + "=" + theForm.elements[e].value + "&";
	var headers = {
		1 : ['Content-type', 'application/x-www-form-urlencoded'],
		2 : ['Content-length', postData.length],
		3 : ['Connection', 'close']
	};
	toggleVisibility(formID);
	httpSend(url, elementID, headers, postData, false);
}

/*	loadThreads(url, sub, async)

	ecWeb forum use only. */
var loadThreads = function(url, sub, async) {
	if(toggleVisibility("sub-" + sub))
		httpLoad(url + "?sub=" + sub, "sub-" + sub, "sub-" + sub + "-info", async);
}

/* loadThread(url, sub, htread, async)

	ecWeb forum use only. */
var loadThread = function(url, sub, thread, async) {
	if(toggleVisibility("sub-" + sub + "-thread-" + thread))
		httpLoad(url + "?sub=" + sub + "&thread=" + thread, "sub-" + sub + "-thread-" + thread, "sub-" + sub + "-thread-" + thread + "-info", async);
}

var deleteMessage = function(url, sub, msg, elementId) {
	httpLoad(url + "?sub=" + sub + "&deletemessage=" + msg, elementId, elementId, true);
}

// This sucks, but I don't feel like revisiting it just now. (Copied from v2)
var validateNewUserForm = function() {
	var theForm = document.getElementById('newUser');
	var sexCheck = 0;
	var returnValue = true;
	for(e in theForm.elements) {
		if(theForm.elements[e].id == 'alias') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'password1') {
			if(theForm.elements[e].value.length < 4) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'password2') {
			if(theForm.elements[e].value != theForm.password1.value) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Password mismatch';
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'realName') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'location') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}

		}
		if(theForm.elements[e].id == 'handle') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}

		}
		if(theForm.elements[e].id == 'streetAddress') {
			if(theForm.elements[e].value.length < 6) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'phone') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'computer') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'sex') {
			if(!theForm.elements[e].checked)
				sexCheck++;
		}
		if(theForm.elements[e].id == 'birthDate') {
			if(theForm.elements[e].value.match(/\d\d\/\d\d\/\d\d/) == null) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = format('Invalid date (%s)'
					,(system.settings&SYS_EURODATE) ? "DD/MM/YY" : "MM/DD/YY");
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'company') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'netmail') {
			if(theForm.elements[e].value.length < 5) {
				theForm.elements[e].style.backgroundColor = '#FA5882';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#82FA58';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
	}
	if(sexCheck == 2) {
		document.getElementById('sexError').innerHTML = ' &lt;- Sex please.'; // She said that.
		returnValue = false;
	}
	return returnValue;
}
