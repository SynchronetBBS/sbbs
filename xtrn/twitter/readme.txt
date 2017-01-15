Twitter utilities for Synchronet BBS 3.16c+
echicken -at- bbs.electronicchicken.com, January 2017

Contents:

	1) Introduction
	2) Create a Twitter application
	3) Add your Twitter configuration to modopts.ini
	4) Send a test tweet
	5) Do things
	6) Support

1) Introduction

	This project is in its early stages.  Its purpose is to provide a set of
	utilities that will enable your BBS to interact with the Twitter REST API.
	Additionally, a library (twitter.js) is provided which you can use to
	develop your own custom integrations.

	At the moment the only included utility is 'tweet.js', which can be used to
	send a tweet.  You can run this via jsexec, or by using the 'bbs.exec()' or
	'system.exec()' javascript methods, or by configuring it as an external
	program which is triggered by a certain event.

	The 'twitter.js' library contains methods for posting tweets, searching for
	tweets, or reading a list of tweets by a particular user.  If you have
	questions about how to write scripts that use this library, or want to
	request additional features, let me know.  (See the Support section below.)

	Be sure that you have updated the following two files before attempting to
	use this:

		exec/load/http.js
		exec/load/oauth.js

	You can find these files on the Synchronet CVS repository, cvs.synchro.net.

2) Create a Twitter application

	If you don't already have a Twitter account, sign up for one:

		https://twitter.com/signup

	(Any tweets created by your BBS will be sent from this account.)

	Next, create a new application:

		https://apps.twitter.com/app/new

	Under the application's 'Keys and Access Tokens' tab, click on the 'Create
	my access token' button at the bottom.

3) Add your Twitter configuration to modopts.ini

	Add a new section to your 'ctrl/modopts.ini' file that looks like this:

		[twitter]
		consumer_key = xxx
		consumer_secret = xxx
		access_token = xxx
		access_token_secret = xxx

	Replace the 'xxx' values with the corresponding details from your Twitter
	application's 'Keys and Access Tokens' page.

4) Send a test tweet

	From a command prompt, change to the 'exec' directory of your Synchronet
	installation and try the following:

		Linux:

			./jsexec ../xtrn/twitter/tweet.js This is a test tweet

		Windows:

			jsexec ..\xtrn\twitter\tweet.js This is a test tweet

	On the web, check to see that your tweet was published.  If so, you're ready
	to proceed.

5) Do things

	You can send a tweet from within any Synchronet javascript module by adding
	a line similar to this:

		bbs.exec('?../xtrn/twitter/tweet.js This is a test tweet');

	For example, you could announce on Twitter whenever somebody logs on to your
	BBS by placing the following line at the bottom of your 'logon.js' file:

		bbs.exec('?../xtrn/twitter/tweet.js ' + user.alias + ' logged on.');

	Twitter may refuse to accept a tweet if it's identical to another one you've
	sent recently, so it may help to add some unique info to it:

		bbs.exec(
			'?../xtrn/twitter/tweet.js ' + user.alias + ' logged on.' + 
			' (Call #' + system.stats.total_logons + ')'
		);

	If modifying 'logon.js' or any other stock javascript module, remember to
	copy the original file to your 'mods' directory and make your changes to
	the copy.  If a script exists in 'mods', it will be used instead of the copy
	in the 'exec' directory.

6) Support

	DOVE-Net

		Post a message to 'echicken' in the 'Synchronet Sysops' sub-board.
		Unless I'm dead or on vacation, I'll probably get back to you within a
		day or so.

	electronic chicken bbs
	
		Post a message in the 'Support' sub-board of the 'Local' message group
		on my BBS, bbs.electronicchicken.com

	IRC : #synchronet on irc.synchro.net
	
		I'm not always in front of a computer, so you won't always receive an
		immediate response if you contact me on IRC.  That said, if you stay
		online and idle, I'll probably see your message and respond eventually.
