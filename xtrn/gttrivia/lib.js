// Gets the JSON service port number from services.ini (if possible)
function getJSONSvcPortFromServicesIni()
{
	var portNum = 0;
	var servicesIniFile = new File(system.ctrl_dir + "services.ini");
	if (servicesIniFile.open("r"))
	{
		var sectionNamesToTry = [ "JSON_DB", "JSON-DB", "JSON" ];
		for (var i = 0; i < sectionNamesToTry.length; ++i)
		{
			var jsonServerCfg = servicesIniFile.iniGetObject(sectionNamesToTry[i]);
			if (jsonServerCfg != null && typeof(jsonServerCfg) === "object")
			{
				portNum = jsonServerCfg.Port;
				break;
			}
		}
		servicesIniFile.close();
	}
	return portNum;
}