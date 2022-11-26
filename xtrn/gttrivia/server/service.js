// For Good Time Trivia JSON database.
// This is a long-running background script.


// Values for JSON DB reading and writing
var JSON_DB_LOCK_READ = 1;
var JSON_DB_LOCK_WRITE = 2;
var JSON_DB_LOCK_UNLOCK = -1;

var NUM_SECONDS_PER_DAY = 86400;

// gRootTriviaScriptDir is the root directory where service.js is located
var gRootTriviaScriptDir = argv[0];


var requireFnExists = (typeof(require) === "function");
if (requireFnExists)
{
	require("json-client.js", "JSONClient");
	require(gRootTriviaScriptDir + "../lib.js", "getJSONSvcPortFromServicesIni");
}
else
{
	load("json-client.js");
	load(gRootTriviaScriptDir + "../lib.js");
}


var gSettings, gJSONClient;

function processUpdate(update)
{
	log(LOG_INFO, "Good Time Trivia: Update");

	if (gSettings.server.deleteScoresOlderThanDays > 0)
	{
		// Look through the server data for old scores that we might want to delete
		try
		{
			var data = gJSONClient.read(gSettings.remoteServer.gtTriviaScope, "SCORES", JSON_DB_LOCK_READ);
			// Example of scores from the server (as of data version 1.01):
			/*
			SCORES:
			 systems:
			  DIGDIST:
			   bbs_name: Digital Distortion
			   user_scores:
				Nightfox:
				 category_stats:
				  0:
				   category_name: General
				   last_score: 20
				   last_time: 2022-11-24
				 total_score: 60
				 last_score: 20
				 last_trivia_category: General
				 last_time: 2022-11-24
			*/
			/*
			if (typeof(data) === "string")
				data = JSON.parse(data);
			*/
			// Sanity checking: Make sure the data is an object and has a "systems" property
			if (typeof(data) !== "object")
				return;
			if (!data.hasOwnProperty("systems"))
				return;

			for (var BBS_ID in data.systems)
			{
				var now = time();
				if (!data.systems[BBS_ID].hasOwnProperty("user_scores"))
					continue;
				for (var playerName in data.systems[BBS_ID].user_scores)
				{
					if (data.systems[BBS_ID].user_scores[playerName].last_time < now - (NUM_SECONDS_PER_DAY * gSettings.server.deleteScoresOlderThanDays))
					{
						// Delete this user's entry
						var JSONLocation = format("SCORES.systems.%s.user_scores.%s", BBS_ID, playerName);
						gJSONClient.remove(gtTriviaScope, JSONLocation, LOCK_WRITE);
					}
				}
			}
		}
		catch (err)
		{
			log(LOG_ERR, "Good Time Trivia: Line " + err.lineNumber + ": " + err);
		}
	}
}

function init()
{
	// Assuming this script is in a 'server' subdirectory, gttrivia.ini should be one directory up
	gSettings = readSettings(gRootTriviaScriptDir + "../");
	try
	{
		gJSONClient = new JSONClient("127.0.0.1", getJSONSvcPortFromServicesIni());
		gJSONClient.subscribe(gSettings.remoteServer.gtTriviaScope, "SCORES");
		gJSONClient.callback = processUpdate;
		processUpdate();
		log(LOG_INFO, "Good Time Trivia JSON DB service task initialized");
	}
	catch (error)
	{
		log(LOG_ERROR, error);
	}
}
var readSettings = function(path)
{
	var settings = {
		readSuccessful: false
	};
	var iniFile = new File(path + "gttrivia.ini");
	if (iniFile.open("r"))
	{
		settings.remoteServer = iniFile.iniGetObject("REMOTE_SERVER");
		settings.server = iniFile.iniGetObject("SERVER");
		iniFile.close();
		settings.readSuccessful = true;

		if (typeof(settings.remoteServer) !== "object")
			settings.remoteServer = {};
		if (!/^[0-9]+$/.test(settings.remoteServer.port))
			settings.remoteServer.port = 0;
		if (typeof(settings.server.deleteScoresOlderThanDays) !== "number")
			settings.server.deleteScoresOlderThanDays = 0;
		else if (settings.server.deleteScoresOlderThanDays < 0)
			settings.server.deleteScoresOlderThanDays = 0;

		// Other settings - Not read from the configuration file, but things we want to use in multiple places
		// in this script
		// JSON scope
		settings.remoteServer.gtTriviaScope = "GTTRIVIA";

		if (settings.server.deleteScoresOlderThanDays > 0)
			log(LOG_INFO, "Good Time Trivia service: Will delete user scores older than " + settings.server.deleteScoresOlderThanDays + " days");
	}
	return settings;
}

function main()
{
	while (!js.terminated)
	{
		mswait(5);
		gJSONClient.cycle();
	}
}


function cleanUp()
{
	gJSONClient.disconnect();
}


try
{
	init();
	main();
	cleanUp();
} catch (err) { }

exit();


