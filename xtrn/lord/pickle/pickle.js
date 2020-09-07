function menu() {
        sclrscr();
		sln('');
		sln('');
		lln('  `5Welcome Other Places Pickles\'s!');
		lln('  `2Home of the Pickle Goddess!');
		sln('');
        display_file(js.exec_dir+'pickle.ans');
		more_nomail();
        var runonce = false;
        do {
            sclrscr();
            display_file(js.exec_dir+'garden.ans');
            lln('  `5The Pickle Goddess has decided to allow you to');
            lln('  `5pick a Pickle from her garden! But choose wisely,');
            lln('  `5Not all are great choices!  The higher you go,');
            lln('  `5The more potent the pickle!');
            if (runonce) { lln('\n`2Must be a choice from 1-9!'); }
            lln('\n`2Which row would you like to pick from?');
            sw('  ');
            tempnum = parseInt(getstr(0, 0, 10, 0, 7, '', {integer:true}), 10);
            runonce = true;
        } while (isNaN(tempnum) || tempnum < 1 || tempnum > 10);
        sclrscr();
		sln('');
		sln('');
        var randNum = random(100) + 1; 
        var strType;
        var strWhat;
        var truncated;
        var pctGainLost;
    
        pctGainLost = '.0' + tempnum;
     
        if (randNum >= 49) // Vegas slot machine odds, tilting to the house
         strType='Bad'
        else   
         strType='Good';
        var randNum = random(10) + 1;
	    if (randNum == 1) {
		 strWhat='hit points';
         truncated=parseInt(player.hp * parseFloat(pctGainLost));
            if (strType=='Bad') 
                player.hp=player.hp-truncated;
            else
                player.hp=player.hp+truncated;
        }
		else if (randNum == 2) {
	 	 strWhat='max hitpoints';
         truncated=parseInt(player.hp_max * parseFloat(pctGainLost));
            if (strType=='Bad') 
                player.hp_max=player.hp_max-truncated;
            else
                player.hp_max=player.hp_max+truncated;  
        }
		else if (randNum == 3) {
	 	 strWhat='forest fights';
         truncated=parseInt(player.forest_fights * parseFloat(pctGainLost));
            if (strType=='Bad') 
                player.forest_fights=player.forest_fights-truncated;
            else
                player.forest_fights=player.forest_fights+truncated;              
        }
		else if (randNum == 4) {
	 	 strWhat='gold';
         truncated=parseInt(player.gold * parseFloat(pctGainLost));
            if (strType=='Bad') 
                player.gold=player.gold-truncated;
            else
                player.gold=player.gold+truncated;              
        }
		else if (randNum == 5) {
	 	 strWhat='bank gold';
         truncated=parseInt(player.bank * parseFloat(pctGainLost));
             if (strType=='Bad') 
                player.bank=player.bank-truncated;
            else
                player.bank=player.bank+truncated;              
        }
		else if (randNum == 6) {
	 	 strWhat='defense';
         truncated=parseInt(player.def * parseFloat(pctGainLost));
             if (strType=='Bad') 
                player.def=player.def-truncated;
            else
                player.def=player.def+truncated;              
        }
		else if (randNum == 7) {
	 	 strWhat='strength';
         truncated=parseInt(player.str * parseFloat(pctGainLost));
             if (strType=='Bad') 
                player.str=player.str-truncated;
            else
                player.str=player.str+truncated;                 
        }
		else if (randNum == 8) {
	 	 strWhat='charm';
         truncated=parseInt(player.cha * parseFloat(pctGainLost));
             if (strType=='Bad') 
                player.cha=player.cha-truncated;
            else
                player.cha=player.cha+truncated;              
        }
		else if (randNum == 9) {
	 	 strWhat='gems';
         truncated=parseInt(player.gem * parseFloat(pctGainLost));
             if (strType=='Bad') 
                player.gem=player.gem-truncated;
            else
                player.gem=player.gem+truncated;              
        }
		else{
	 	 strWhat='des1';
         truncated=1;
        }
        if (parseFloat(truncated) != 0) {
            if (strType=='Good' && strWhat!='des1') {
            lln('  `5This pickle is divine! Your '+strWhat+' increased by ' +truncated+'!');
            truncated=parseInt(player.hp * parseFloat(pctGainLost));
            }
            else if (strType=='Bad' && strWhat!='des1'){		
            lln('  `5This pickle taste like poo! Your '+strWhat+' decreased by ' +truncated+'!');
            truncated=parseInt(player.hp * parseFloat(pctGainLost));
            }
            else {
            lln('  `5You have been marked by tattoo from the Pickle Goddess!'); 
            player.des1='I have been tagged pwned by the Pickle Goddess!';
            }
        }
        else
            {
            lln('  `5You were about to... ');
                if (strType=='Good')
                    lw('  `5gain ');
             else
                 lw('  `5lose ');
             lln('something, but you do not have enough ' + strWhat + '!'); 
             lln('  `5Come back later when you level up or have more stuff... ');
            }
        more_nomail();
		exit(0);
}

if (argc == 1 && argv[0] == 'INSTALL') {
        var install = {
                desc:'`0T`2ravel `0T`2o Other Places Pickles',
        }
        exit(0);
}
// possible record changes: hp, hp_max, forest_fights, gold, bank, def, str, cha, gem, des1

menu();
