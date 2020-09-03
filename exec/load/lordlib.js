/************ Synchronet LORD game data library *************

		by mcmlxxix - March 5, 2015 
		www.thebrokenbubble.com
		
		$Id: lordlib.js,v 1.1 2015/03/05 20:03:44 mcmlxxix Exp $
		$Revision: 1.1 $
*/

/********************** API usage example**************************

// load this library
load("lordlib.js");

// create lord object instance
var LORD = new Lord("/sbbs/xtrn/lord/");

// print all player names and their level
for(var p=0;p<LORD.players.length;p++) {
	var player = LORD.players[p];
	console.writeln("Name: " + player.name + " Level: " + player.level);
}
	
*/

load("cnflib.js");

/* LORD player record structure */
struct.lord_player={
	name:		{bytes:20,			type:"str", 	length:UCHAR},
	realname:	{bytes:50,			type:"str", 	length:UCHAR},
	hitpoints:	{bytes:UINT16_T,	type:"int"},
	bad:		{bytes:UINT16_T,	type:"int"},
	rate:		{bytes:UINT16_T,	type:"int"},
	hit_max:	{bytes:UINT16_T,	type:"int"},
	weapon_num:	{bytes:UINT16_T,	type:"int"},
	weapon:		{bytes:20,			type:"str", 	length:UCHAR},
	seen_master:{bytes:UINT16_T,	type:"int"},
	fights_left:{bytes:UINT16_T,	type:"int"},
	human_left:	{bytes:UINT16_T,	type:"int"},
	gold:		{bytes:UINT32_T,	type:"int"},
	bank:		{bytes:UINT32_T,	type:"int"},
	def:		{bytes:UINT16_T,	type:"int"},
	strength:	{bytes:UINT16_T,	type:"int"},
	charm:		{bytes:UINT16_T,	type:"int"},
	seen_dragon:{bytes:UINT16_T,	type:"int"},
	seen_violet:{bytes:UINT16_T,	type:"int"},
	level:		{bytes:UINT16_T,	type:"int"},
	time:		{bytes:UINT16_T,	type:"int"},
	arm:		{bytes:20,			type:"str", 	length:UCHAR},
	arm_num:	{bytes:UINT16_T,	type:"int"},
	dead:		{bytes:UCHAR,		type:"int"},
	inn:		{bytes:UCHAR,		type:"int"},
	gem:		{bytes:UINT16_T,	type:"int"},
	exp:		{bytes:UINT32_T,	type:"int"},
	sex:		{bytes:UCHAR,		type:"int"},
	seen_bard:	{bytes:UCHAR,		type:"int"},
	last_alive:	{bytes:UINT16_T,	type:"int"},
	lays:		{bytes:UINT16_T,	type:"int"},
	why:		{bytes:UINT16_T,	type:"int"},
	on_now:		{bytes:UCHAR,		type:"int"},
	m_time:		{bytes:UINT16_T,	type:"int"},
	time_on:	{bytes:5,			type:"str", 	length:UCHAR},
	pclass:		{bytes:UCHAR,		type:"int"},
	horse:		{bytes:UINT16_T,	type:"int"},
	love:		{bytes:25,			type:"str", 	length:UCHAR},
	married:	{bytes:UINT16_T,	type:"int"},
	kids:		{bytes:UINT16_T,	type:"int"},
	king:		{bytes:UINT16_T,	type:"int"},
	skillw:		{bytes:UCHAR,		type:"int"},
	skillm:		{bytes:UCHAR,		type:"int"},
	skillt:		{bytes:UCHAR,		type:"int"},
	levelw:		{bytes:UCHAR,		type:"int"},
	levelm:		{bytes:UCHAR,		type:"int"},
	levelt:		{bytes:UCHAR,		type:"int"},
	inn_random:	{bytes:UCHAR,		type:"int"},
	married_to:	{bytes:UINT16_T,	type:"int"},
	v1:			{bytes:UINT32_T,	type:"int"},
	pkills:		{bytes:UINT16_T,	type:"int"},
	forest_event:{bytes:UINT16_T,	type:"int"},
	special:	{bytes:UCHAR,		type:"int"},
	flirted:	{bytes:UCHAR,		type:"int"},
	new_stat1:	{bytes:UCHAR,		type:"int"},
	new_stat2:	{bytes:UCHAR,		type:"int"},
	new_stat3:	{bytes:UCHAR,		type:"int"}
}

/* LORD player file structure */
struct.lord_players = {
	players:	{
		bytes:struct.lord_player,	
		type:"lst", 
		length:file_size(lordPlayerFile)/CNF.getBytes(struct.lord_player)
	}
}

/* LORD game object */
function Lord(path) {
	
	/* module settings */
	this.gamePath = path;
	this.playerFile = "player.dat";
	this.heroFile = "lasthero.dat";
	this.mailPrefix = "mail";
	
	/* game properties */
	this.players = [];
	this.mail = {};
	this.lastHero;
	
	/* constructor */
	this.loadPlayers();
	this.loadHero();
	this.loadMail();
	
}

/* LORD game object methods */
Lord.prototype.loadPlayers = function() {
	this.players = CNF.read(this.gamePath + this.playerFile,struct.lord_players).players;
}
Lord.prototype.loadHero = function() {
	var f = new File(this.gamePath + this.heroFile);
	f.open('r',true);
	this.lastHero = f.readln();
	f.close();
}
Lord.prototype.loadMail = function() {
	var mail = directory(this.gamePath + this.mailPrefix + "*.dat");
	while(mail.length > 0) {
		var m = mail.shift();
		var p = file_getname(m).substring(4,file_getname(m).indexOf('.'));
		var f = new File(m);
		f.open('r',true);
		this.mail[p] = f.readAll();
		f.close();
	}
}

/* LORD Player record structure (from DDIGM v1.01)

PlayerRec = record
   names:         string[20];      {player handle in the game}
   real_names:    string[50]       {real name/or handle from BBS} ;
   hit_points:    integer;         {player hit points}
   bad:           integer;         {don't know - might not be used at all}
   rate:          integer;         {couldn't find this one in the source}
   hit_max:       integer;         {hit_point max}
   weapon_num:    integer;         {weapon number}
   weapon:        string[20];      {name of weapon}
   seen_master:   integer;         {equals 5 if seen master, else 0}
   fights_left:   integer;         {forest fights left}
   human_left:    integer;         {human fights left}
   gold:          longint;         {gold in hand}
   bank:          longint;         {gold in bank}
   def:           integer;         {total defense points }
   strength:      integer;         {total strength}
   charm:         integer;         {good looking meter}
   seen_dragon:   integer;         {seen dragon?  5 if yes else 0}
   seen_violet:   integer;         {seen violet?  5 if yes else 0}
   level:         integer;         {level of player}
   time:          word;            {day # that player last played on}
   arm:           string[20];      {armour name}
   arm_num:       integer;         {armour number}
   dead:          shortint;        {player dead?  5 if yes else 0}
   inn:           shortint;        {player sleeping at inn?  5 if yes else 0}
   gem:           integer;         {# of gems on hand}
   exp:           longint;         {experience}
   sex:           shortint;        {gender, 5 if female else 0}
   seen_bard:     shortint;        {seen bard?  5 if yes else 0}
   last_alive_time: integer;       {day # player was last reincarnated on}
   Lays:          integer;         {players lays stat}
   Why:           integer;         {not used yet}
   on_now:        boolean;         {is player on?}
   m_time:        integer;         {day on_now stat was last used}
   time_on:       string[5];       {time player logged on in Hour:Minutes format}
   class:         shortint;        {class, should be 1, 2 or 3}
   horse:         integer;         {*NEW*  If 1, player has a horse}
   love:          string[25];      {not used - may be used for inter-player marrages later}
   married:       integer;         {who player is married to, should be -1 if not married}
   kids:          integer;         {# of kids}
   king:          integer;         {# of times player has won game}
   skillw:        shortint;        {number of Death Knight skill points}
   skillm:        shortint;        {number of Mystical Skills points}
   skillt:        shortint;        {number of Thieving Skills points}
  levelw: shortint; {number of Death Knight skill uses left today}
  levelm: shortint; {number of Mystical skill uses left today}
  levelt: shortint; {number of Thieving skill uses left today}

  inn_random: boolean; {not used yet}
  married_to: integer; {same as Married, I think - don't know why it's here}
  v1: longint;
  v2: integer; {# of player kills}
  v3: integer; {if 5, 'wierd' event in forest will happen}
  v4: boolean; {has player done 'special' for that day?}
  v5: shortint; {has player flirted with another player that day?  if so, 5}
  new_stat1: shortint;
  new_stat2: shortint;  {these 3 are unused right now}
  new_stat3: shortint;  {Warning: Joseph's NPCLORD screws with all three}
end;

*/