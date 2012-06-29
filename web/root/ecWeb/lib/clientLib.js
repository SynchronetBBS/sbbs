/* clientLib.js, from ecWeb v2 for Synchronet BBS 3.15+
   by Derek Mullin (echicken -at- bbs.electronicchicken.com) */

// Some client-side javascript functions, mostly used with forumLib.ssjs.

function toggleVisibility(elementID) {
	if(document.getElementById(elementID).style.display == 'none') {
		document.getElementById(elementID).style.display = 'block';
	} else {
		document.getElementById(elementID).style.display = 'none';
	}
	return;
}

function addClass(elementID, cn) {
	document.getElementById(elementID).className += ' ' + cn;
	return;
}

function noReturn(evt) {
	var theEvent = evt || window.event;
        var key = theEvent.keyCode || theEvent.which;
	if(key == '13') {
		theEvent.returnValue = false;
		theEvent.preventDefault();
		return;
	}
}

function httpPost(url, postData, divID) {
	var XMLReq = new XMLHttpRequest();
	XMLReq.open('POST', url, true);
	XMLReq.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
	XMLReq.setRequestHeader('Content-length', postData.length);
	XMLReq.setRequestHeader('Connection', 'close');
	XMLReq.send(postData);
	document.getElementById(divID).innerHTML = "Submitting your reply . . .";
	XMLReq.onreadystatechange = function() { if(XMLReq.readyState == 4 && divID != 'none')  document.getElementById(divID).innerHTML = XMLReq.responseText; }
}

/* This is really only good for message reply forms at the moment, but at this
   point that's good enough for me. */
function submitForm(formID, url, divID) {
	var theForm = document.getElementById(formID);
	var postData = "action=postMessage&subBoardCode=" + theForm.elements['subBoard'].value;
	postData += "&irtMessage=" + theForm.elements['irtMessage'].value;
	postData += "&subject=" + theForm.elements['subject'].value;
	postData += "&to=" + theForm.elements['to'].value;
	postData += "&from=" + theForm.elements['from'].value;
	postData += "&body=" + theForm.elements['body'].value;
	httpPost(url, postData, divID);
	return;
}

function updatePointer(subBoardCode, url, mg, sb) {
	postData = "action=updatePointer&subBoardCode=" + subBoardCode + "&mg=" + mg + "&sb=" + sb;
	httpPost(url, postData, 'none');
}

// This does a very basic validation of the newuser form. Could easily become more complex.
function validateNewUserForm() {
	var theForm = document.getElementById('newUser');
	var sexCheck = 0;
	var returnValue = true;
	for(e in theForm.elements) {
		if(theForm.elements[e].id == 'alias') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'password1') {
			if(theForm.elements[e].value.length < 4) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'password2') {
			if(theForm.elements[e].value != theForm.password1.value) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Password mismatch';
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'realName') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'location') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}

		}
		if(theForm.elements[e].id == 'handle') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}

		}
		if(theForm.elements[e].id == 'streetAddress') {
			if(theForm.elements[e].value.length < 6) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'phone') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'computer') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'sexM') {
			if(!theForm.elements[e].checked) sexCheck++;
		}
		if(theForm.elements[e].id == 'sexF') {
			if(!theForm.elements[e].checked) sexCheck++;
		}
		if(theForm.elements[e].id == 'birthDate') {
			if(theForm.elements[e].value.match(/\d\d-\d\d-\d\d/) == null) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Invalid date (DD-MM-YY)';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'company') {
			if(theForm.elements[e].value.length < 1) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
		if(theForm.elements[e].id == 'netmail') {
			if(theForm.elements[e].value.length < 5) {
				theForm.elements[e].style.backgroundColor = '#660000';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = 'Too short';
				returnValue = false;
			} else {
				theForm.elements[e].style.backgroundColor = '#003300';
				document.getElementById(theForm.elements[e].id + 'Error').innerHTML = '';
			}
		}
	}
	if(sexCheck == 2) {
		document.getElementById('sexError').innerHTML = ' &lt;- Sex please.';
		returnValue = false;
	}
	return returnValue;
}
