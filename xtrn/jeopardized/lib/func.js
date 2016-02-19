// Where 'game' is a given user's game state, and 'round' is a number
function countAnswers(game, round) {
	var answers = 0;
	Object.keys(game.rounds[round]).forEach(
		function (category) {
			answers += game.rounds[round][category].length;
		}
	);
	return answers;
}

function compareAnswer(id, answer) {
	var res = JSON.parse(
		(new HTTPRequest()).Get(
			settings.WebAPI.url + '/clues/' + id + '/compare/' +
			encodeURIComponent(answer)
		)
	);
	return res.correct;
}

function notifySysop(subject, body) {

	var header = {
		to : 'Jeopardized',
		from : 'Jeopardized',
		to_ext : 1,
		from_ext : 1,
		subject : subject
	};

	var msgBase = new MsgBase('mail');
	msgBase.open();
	msgBase.save_msg(header, body);
	msgBase.close();

}