/* generic vote */
function Vote(name,vote,utc) {
	this.name = name;
	this.vote = vote;
	this.time = utc;
}

/* generic voting topic */
function Issue(id,text,utc) {
	var status = 0;
	this.id = id;
	this.text = text;
	this.time = utc;
	this.votes = {};
	
	this.__defineGetter__("status",function() {
		if(status == 0)
			return "open";
		else
			return "closed";
	});
	
	this.open = function() {
		status = 0;
	}
	
	this.close = function() {
		status = 1;
	}
	
	this.vote = function(name,vote){
		if(status == 0) {
			this.votes[name.toUpperCase()] = new Vote(name,vote,Date.now());
			return true;
		}
		else {
			return false;
		}
	}
}

/* consolidate votes into an array */
function crunchVotes(issue) {
	var votes = [];
	for each(var v in issue.votes) {
		votes.push(v);
	}
	return votes;
}

/* count the number of votes on a topic */
function countVotes(issue) {
	return crunchVotes(issue).length;
}

/* push topic results into an array of sexy text */
function showVotes(issue) {
	var votes = crunchVotes(issue);
	var vlist = [ "Issue #" + issue.id + ": (" + issue.status + ") " + issue.text	];
	for each(var v in votes) {
		vlist.push("Name: " + v.name + " Vote: " + v.vote);
	}
	return votes;
}

