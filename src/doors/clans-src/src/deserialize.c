#include <assert.h>
#include <stdnoreturn.h>
#include <string.h>
#include "platform.h"

#include "defines.h"
#include "deserialize.h"
#include "myopen.h"
#include "quests.h"
#include "structs.h"

noreturn void System_Error(char *szErrorMsg);

#define unpack_char(x) do {      \
	assert(remain);           \
	if (remain < sizeof(char)) \
		return SIZE_MAX;    \
	x = (char)*(src++);                \
	remain--;                     \
} while(0)

#define unpack_int8(x) do {        \
	assert(remain);             \
	if (remain < sizeof(int8_t)) \
		return SIZE_MAX;      \
	x = (int8_t)*(src++);          \
	remain--;                       \
} while(0)

#define unpack_uint8_t(x) do {   \
	assert(remain);           \
	if (remain < sizeof(char)) \
		return SIZE_MAX;    \
	x = *(src++);      \
	remain--;                     \
} while(0)

#define unpack_bool(x) do {      \
	assert(remain);           \
	if (remain < sizeof(char)) \
		return SIZE_MAX;    \
	x = *(src++);                \
	remain--;                     \
} while(0)

#define unpack_int16_t(x) do {       \
	int16_t tmp;                  \
	assert(remain >= sizeof(tmp)); \
	if (remain < sizeof(tmp))       \
		return SIZE_MAX;         \
	memcpy(&tmp, src, sizeof(tmp));   \
	x = SWAP16S(tmp);                  \
	src += sizeof(tmp);                 \
	remain -= sizeof(tmp);               \
} while(0)

#define unpack_uint16_t(x) do {      \
	uint16_t tmp;                 \
	assert(remain >= sizeof(tmp)); \
	if (remain < sizeof(tmp))       \
		return SIZE_MAX;         \
	memcpy(&tmp, src, sizeof(tmp));   \
	x = SWAP16(tmp);                   \
	src += sizeof(tmp);                 \
	remain -= sizeof(tmp);               \
} while(0)

#define unpack_int32_t(x) do {       \
	int32_t tmp;                  \
	assert(remain >= sizeof(tmp)); \
	if (remain < sizeof(tmp))       \
		return SIZE_MAX;         \
	memcpy(&tmp, src, sizeof(tmp));   \
	x = SWAP32S(tmp);                  \
	src += sizeof(tmp);                 \
	remain -= sizeof(tmp);               \
} while(0)

#define unpack_int16_tArr(x) do {                                  \
	for (size_t ctr = 0; ctr < sizeof(x) / sizeof(*(x)); ctr++) \
		unpack_int16_t(x[ctr]);                              \
} while(0)

#define unpack_uint16_tArr(x) do {                                 \
	for (size_t ctr = 0; ctr < sizeof(x) / sizeof(*(x)); ctr++) \
		unpack_uint16_t(x[ctr]);                             \
} while(0)

#define unpack_int16_tArr2(x, szo, szi) do {            \
	for (size_t ctro = 0; ctro < szo; ctro++)        \
		for (size_t ctri = 0; ctri < szi; ctri++) \
			unpack_int16_t(x[ctro][ctri]);     \
} while(0)

#define unpack_charArr(x) do {      \
	assert(remain >= sizeof(x)); \
	if (remain < sizeof(x))       \
		return SIZE_MAX;       \
	memcpy(x, src, sizeof(x));      \
	src += sizeof(x);                \
	remain -= sizeof(x);              \
} while(0)

#define unpack_ptr(x) do {    \
	assert(remain >= 4);   \
	if (remain < 4)         \
		return SIZE_MAX; \
	x = NULL;                 \
} while(0)

// Setting these to zero... hopefully they don't round-trip. :(
#define unpack_ptrArr(x) do {                                        \
	for (size_t ctr = 0; ctr < sizeof(x) / sizeof(*(x)); ctr++) { \
		x[ctr] = NULL;                                         \
	}                                                               \
} while(0)

#define unpack_struct(x, s) do {                   \
	size_t res = s_ ## s ## _d(src, remain, x); \
	if (res == SIZE_MAX)                         \
		return SIZE_MAX;                      \
	src += res;                                    \
	remain -= res;                                  \
} while(0)

#define unpack_structArr(x, s) do {                                \
	for (size_t ctr = 0; ctr < sizeof(x) / sizeof(*(x)); ctr++) \
		unpack_struct(&x[ctr], s);                           \
} while(0)

size_t
s_ibbs_node_attack_d(const void *bufptr, size_t bufsz, struct ibbs_node_attack *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_t(s->ReceiveIndex);
	unpack_int16_t(s->SendIndex);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_MessageHeader_d(const void *bufptr, size_t bufsz, struct MessageHeader *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->szFromUserName);
	unpack_charArr(s->szToUserName);
	unpack_charArr(s->szSubject);
	unpack_charArr(s->szDateTime);
	unpack_uint16_t(s->wTimesRead);
	unpack_uint16_t(s->wDestNode);
	unpack_uint16_t(s->wOrigNode);
	unpack_uint16_t(s->wCost);
	unpack_uint16_t(s->wOrigNet);
	unpack_uint16_t(s->wDestNet);
	unpack_uint16_t(s->wDestZone);
	unpack_uint16_t(s->wOrigZone);
	unpack_uint16_t(s->wDestPoint);
	unpack_uint16_t(s->wOrigPoint);
	unpack_uint16_t(s->wReplyTo);
	unpack_uint16_t(s->wAttribute);
	unpack_uint16_t(s->wNextReply);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_ibbs_node_reset_d(const void *bufptr, size_t bufsz, struct ibbs_node_reset *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_t(s->Received);
	unpack_int32_t(s->LastSent);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_ibbs_node_recon_d(const void *bufptr, size_t bufsz, struct ibbs_node_recon *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int32_t(s->LastReceived);
	unpack_int32_t(s->LastSent);
	unpack_char(s->PacketIndex);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Msg_Txt_d(const void *bufptr, size_t bufsz, struct Msg_Txt *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_tArr(s->Offsets);
	unpack_int16_t(s->Length);
	unpack_int16_t(s->NumLines);
	unpack_ptr(s->MsgTxt);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Message_d(const void *bufptr, size_t bufsz, struct Message *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_tArr(s->ToClanID);
	unpack_int16_tArr(s->FromClanID);
	unpack_charArr(s->szFromName);
	unpack_charArr(s->szDate);
	unpack_charArr(s->szAllyName);
	unpack_charArr(s->szFromVillageName);
	unpack_int16_t(s->MessageType);
	unpack_int16_t(s->AllianceID);
	unpack_int16_t(s->Flags);
	unpack_int16_t(s->BBSIDFrom);
	unpack_int16_t(s->BBSIDTo);
	unpack_uint16_t(s->PublicMsgIndex);
	unpack_struct(&s->Data, Msg_Txt);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Topic_d(const void *bufptr, size_t bufsz, struct Topic *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_bool(s->Known);
	unpack_bool(s->Active);
	unpack_bool(s->ClanInfo);
	unpack_charArr(s->szName);
	unpack_charArr(s->szFileName);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_LeavingData_d(const void *bufptr, size_t bufsz, struct LeavingData *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_bool(s->Active);
	unpack_int16_t(s->DestID);
	unpack_int16_tArr(s->ClanID);
	unpack_int32_t(s->Followers);
	unpack_int32_t(s->Footmen);
	unpack_int32_t(s->Axemen);
	unpack_int32_t(s->Knights);
	unpack_int32_t(s->Catapults);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_PClass_d(const void *bufptr, size_t bufsz, struct PClass *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->szName);
	unpack_charArr(s->Attributes);
	unpack_int16_t(s->MaxHP);
	unpack_int16_t(s->Gold);
	unpack_int16_t(s->MaxSP);
	unpack_charArr(s->SpellsKnown);
	unpack_int16_t(s->VillageType);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_SpellsInEffect_d(const void *bufptr, size_t bufsz, struct SpellsInEffect *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_t(s->SpellNum);
	unpack_int16_t(s->Energy);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_SpyAttemptPacket_d(const void *bufptr, size_t bufsz, struct SpyAttemptPacket *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->szSpierName);
	unpack_int16_t(s->IntelligenceLevel);
	unpack_int16_t(s->TargetType);
	unpack_int16_tArr(s->ClanID);
	unpack_int16_tArr(s->MasterID);
	unpack_int16_t(s->BBSFromID);
	unpack_int16_t(s->BBSToID);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_SpyResultPacket_d(const void *bufptr, size_t bufsz, struct SpyResultPacket *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_t(s->BBSFromID);
	unpack_int16_t(s->BBSToID);
	unpack_int16_tArr(s->MasterID);
	unpack_charArr(s->szTargetName);
	unpack_bool(s->Success);
	unpack_struct(&s->Empire, empire);
	unpack_charArr(s->szTheDate);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_TradeData_d(const void *bufptr, size_t bufsz, struct TradeData *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_struct(&s->Giving, TradeList);
	unpack_struct(&s->Asking, TradeList);
	unpack_bool(s->Active);
	unpack_int16_tArr(s->FromClanID);
	unpack_int16_tArr(s->ToClanID);
	unpack_charArr(s->szFromClan);
	unpack_int32_t(s->Code);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_TradeList_d(const void *bufptr, size_t bufsz, struct TradeList *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int32_t(s->Gold);
	unpack_int32_t(s->Followers);
	unpack_int32_t(s->Footmen);
	unpack_int32_t(s->Axemen);
	unpack_int32_t(s->Knights);
	unpack_int32_t(s->Catapults);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_UserInfo_d(const void *bufptr, size_t bufsz, struct UserInfo *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_tArr(s->ClanID);
	unpack_bool(s->Deleted);
	unpack_charArr(s->szMasterName);
	unpack_charArr(s->szName);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_UserScore_d(const void *bufptr, size_t bufsz, struct UserScore *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_tArr(s->ClanID);
	unpack_charArr(s->Symbol);
	unpack_charArr(s->szName);
	unpack_int32_t(s->Points);
	unpack_int16_t(s->BBSID);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Spell_d(const void *bufptr, size_t bufsz, struct Spell *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;
	uint8_t tmp;

	unpack_charArr(s->szName);
	unpack_int16_t(s->TypeFlag);
	unpack_bool(s->Friendly);
	unpack_bool(s->Target);
	unpack_charArr(s->Attributes);
	unpack_int8(s->Value);
	unpack_int16_t(s->Energy);
	unpack_int8(s->Level);
	unpack_ptr(s->pszDamageStr);
	unpack_ptr(s->pszHealStr);
	unpack_ptr(s->pszModifyStr);
	unpack_ptr(s->pszWearoffStr);
	unpack_ptr(s->pszStatusStr);
	unpack_ptr(s->pszOtherStr);
	unpack_ptr(s->pszUndeadName);
	unpack_int16_t(s->SP);
	unpack_bool(s->StrengthCanReduce);
	unpack_bool(s->WisdomCanReduce);
	unpack_uint8_t(tmp);
	s->MultiAffect = tmp & 0x80 ? 1U : 0U >> 7;

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_pc_d(const void *bufptr, size_t bufsz, struct pc *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;
	uint8_t tmp;

	unpack_charArr(s->szName);
	unpack_int16_t(s->HP);
	unpack_int16_t(s->MaxHP);
	unpack_int16_t(s->SP);
	unpack_int16_t(s->MaxSP);
	unpack_charArr(s->Attributes);
	unpack_int8(s->Status);
	unpack_int16_t(s->Weapon);
	unpack_int16_t(s->Shield);
	unpack_int16_t(s->Armor);
	unpack_int16_t(s->WhichRace);
	unpack_int16_t(s->WhichClass);
	unpack_int32_t(s->Experience);
	unpack_int16_t(s->Level);
	unpack_int16_t(s->TrainingPoints);
	unpack_ptr(s->MyClan);
	unpack_charArr(s->SpellsKnown);
	unpack_structArr(s->SpellsInEffect, SpellsInEffect);
	unpack_int16_t(s->Difficulty);
	unpack_uint8_t(tmp);
	s->Undead = tmp & 0x80 ? 1U : 0U;
	s->DefaultAction = tmp & 0x7FU;
	unpack_int32_t(s->CRC);
	if (CRCValue(bufptr, (src - (uint8_t*)bufptr) - 4) == s->CRC)
		s->CRC = 1;
	else
		System_Error("CRC Value is WRONG!");

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Packet_d(const void *bufptr, size_t bufsz, struct Packet *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_bool(s->Active);
	unpack_charArr(s->GameID);
	unpack_charArr(s->szDate);
	unpack_int16_t(s->BBSIDFrom);
	unpack_int16_t(s->BBSIDTo);
	unpack_int16_t(s->PacketType);
	unpack_int32_t(s->PacketLength);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_NPCInfo_d(const void *bufptr, size_t bufsz, struct NPCInfo *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->szName);
	unpack_structArr(s->Topics, Topic);
	unpack_struct(&s->IntroTopic, Topic);
	unpack_int8(s->Loyalty);
	unpack_int16_t(s->WhereWander);
	unpack_bool(s->Roamer);
	unpack_int16_t(s->NPCPCIndex);
	unpack_int16_t(s->KnownTopics);
	unpack_int16_t(s->MaxTopics);
	unpack_int16_t(s->OddsOfSeeing);
	unpack_charArr(s->szHereNews);
	unpack_charArr(s->szQuoteFile);
	unpack_charArr(s->szMonFile);
	unpack_charArr(s->szIndex);
	unpack_int16_t(s->VillageType);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_NPCNdx_d(const void *bufptr, size_t bufsz, struct NPCNdx *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->szIndex);
	unpack_bool(s->InClan);
	unpack_int16_tArr(s->ClanID);
	unpack_int16_t(s->WhereWander);
	unpack_int16_t(s->Status);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Language_d(const void *bufptr, size_t bufsz, struct Language *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->Signature);
	unpack_uint16_tArr(s->StrOffsets);
	unpack_uint16_t(s->NumBytes);
	unpack_ptr(s->BigString);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Alliance_d(const void *bufptr, size_t bufsz, struct Alliance *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_t(s->ID);
	unpack_charArr(s->szName);
	unpack_int16_tArr(s->CreatorID);
	unpack_int16_tArr(s->OriginalCreatorID);
	unpack_int16_tArr2(s->Member, MAX_ALLIANCEMEMBERS, 2);
	unpack_struct(&s->Empire, empire);
	unpack_structArr(s->Items, item_data);
	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_game_data_d(const void *bufptr, size_t bufsz, struct game_data *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_t(s->GameState);
	unpack_bool(s->InterBBS);
	unpack_charArr(s->szTodaysDate);
	unpack_charArr(s->szDateGameStart);
	unpack_charArr(s->szLastJoinDate);
	unpack_int16_t(s->NextClanID);
	unpack_int16_t(s->NextAllianceID);
	unpack_charArr(s->szWorldName);
	unpack_charArr(s->LeagueID);
	unpack_charArr(s->GameID);
	unpack_bool(s->ClanTravel);
	unpack_int16_t(s->LostDays);
	unpack_int16_t(s->MaxPermanentMembers);
	unpack_bool(s->ClanEmpires);
	unpack_int16_t(s->MineFights);
	unpack_int16_t(s->ClanFights);
	unpack_int16_t(s->DaysOfProtection);
	unpack_int32_t(s->CRC);
	if (CRCValue(bufptr, (src - (uint8_t*)bufptr) - 4) == s->CRC)
		s->CRC = 1;
	else
		System_Error("CRC Value is WRONG!");

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_village_data_d(const void *bufptr, size_t bufsz, struct village_data *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->ColorScheme);
	unpack_charArr(s->szName);
	unpack_int16_t(s->TownType);
	unpack_int16_t(s->TaxRate);
	unpack_int16_t(s->InterestRate);
	unpack_int16_t(s->GST);	
	unpack_int16_t(s->ConscriptionRate);
	unpack_int16_tArr(s->RulingClanId);
	unpack_charArr(s->szRulingClan);
	unpack_int16_t(s->GovtSystem);
	unpack_int16_t(s->RulingDays);
	unpack_uint16_t(s->PublicMsgIndex);
	unpack_int16_t(s->MarketLevel);
	unpack_int16_t(s->TrainingHallLevel);
	unpack_int16_t(s->ChurchLevel);
	unpack_int16_t(s->PawnLevel);
	unpack_int16_t(s->WizardLevel);
	assert(remain);
	if (!remain)
		return SIZE_MAX;
	s->SetTaxToday = (*src & 0x80) ? 1U : 0U;
	s->SetInterestToday = (*src & 0x40) ? 1U : 0U;
	s->SetGSTToday = (*src & 0x20) ? 1U : 0U;
	s->UpMarketToday = (*src & 0x10) ? 1U : 0U;
	s->UpTHallToday = (*src & 0x08) ? 1U : 0U;
	s->UpChurchToday = (*src & 0x04) ? 1U : 0U;
	s->UpPawnToday = (*src & 0x02) ? 1U : 0U;
	s->UpWizToday = (*(src++) & 0x01) ? 1U : 0U;
	remain--;
	assert(remain);
	if (!remain)
		return SIZE_MAX;
	s->SetConToday = (*src & 0x80) ? 1U : 0U;
	s->ShowEmpireStats = (*(src++) & 0x40) ? 1U : 0U;
	remain--;

	unpack_charArr(s->HFlags);
	unpack_charArr(s->GFlags);
	unpack_int16_t(s->VillageType);
	unpack_int16_t(s->CostFluctuation);
	unpack_int16_t(s->MarketQuality);
	unpack_struct(&s->Empire, empire);
	unpack_int32_t(s->CRC);
	if (CRCValue(bufptr, (src - (uint8_t*)bufptr) - 4) == s->CRC)
		s->CRC = 1;
	else
		System_Error("CRC Value is WRONG!");

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_EventHeader_d(const void *bufptr, size_t bufsz, struct EventHeader *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->szName);
	unpack_int32_t(s->EventSize);
	unpack_bool(s->Event);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_FileHeader_d(const void *bufptr, size_t bufsz, struct FileHeader *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_ptr(s->fp);
	unpack_charArr(s->szFileName);
	unpack_int32_t(s->lStart);
	unpack_int32_t(s->lEnd);
	unpack_int32_t(s->lFileSize);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Army_d(const void *bufptr, size_t bufsz, struct Army *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int32_t(s->Footmen);
	unpack_int32_t(s->Axemen);
	unpack_int32_t(s->Knights);
	unpack_int32_t(s->Followers);
	unpack_int8(s->Rating);
	unpack_int8(s->Level);
	unpack_struct(&s->Strategy, Strategy);
	unpack_int32_t(s->CRC);
	if (CRCValue(bufptr, (src - (uint8_t*)bufptr) - 4) == s->CRC)
		s->CRC = 1;
	else
		System_Error("CRC Value is WRONG!");

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_Strategy_d(const void *bufptr, size_t bufsz, struct Strategy *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int8(s->AttackLength);
	unpack_int8(s->AttackIntensity);
	unpack_int8(s->LootLevel);
	unpack_int8(s->DefendLength);
	unpack_int8(s->DefendIntensity);

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_AttackPacket_d(const void *bufptr, size_t bufsz, struct AttackPacket *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_t(s->BBSFromID);
	unpack_int16_t(s->BBSToID);
	unpack_struct(&s->AttackingEmpire, empire);
	unpack_struct(&s->AttackingArmy, Army);
	unpack_int16_t(s->Goal);
	unpack_int16_t(s->ExtentOfAttack);
	unpack_int16_t(s->TargetType);
	unpack_int16_tArr(s->ClanID);
	unpack_int16_tArr(s->AttackOriginatorID);
	unpack_int16_t(s->AttackIndex);
	unpack_int32_t(s->CRC);
	if (CRCValue(bufptr, (src - (uint8_t*)bufptr) - 4) == s->CRC)
		s->CRC = 1;
	else
		System_Error("CRC Value is WRONG!");

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_AttackResult_d(const void *bufptr, size_t bufsz, struct AttackResult *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_bool(s->Success);
	unpack_bool(s->NoTarget);
	unpack_bool(s->InterBBS);
	unpack_struct(&s->OrigAttackArmy, Army);
	unpack_int16_t(s->AttackerType);
	unpack_int16_tArr(s->AttackerID);
	unpack_int16_t(s->AllianceID);
	unpack_charArr(s->szAttackerName);
	unpack_int16_t(s->DefenderType);
	unpack_int16_tArr(s->DefenderID);
	unpack_charArr(s->szDefenderName);
	unpack_int16_t(s->BBSIDFrom);
	unpack_int16_t(s->BBSIDTo);
	unpack_int16_t(s->PercentDamage);
	unpack_int16_t(s->Goal);
	unpack_int16_t(s->ExtentOfAttack);
	unpack_struct(&s->AttackCasualties, Army);
	unpack_struct(&s->DefendCasualties, Army);
	unpack_int16_tArr(s->BuildingsDestroyed);
	unpack_int32_t(s->GoldStolen);
	unpack_int16_t(s->LandStolen);
	unpack_struct(&s->ReturningArmy, Army);
	unpack_int16_t(s->ResultIndex);
	unpack_int16_t(s->AttackIndex);
	unpack_int32_t(s->CRC);
	if (CRCValue(bufptr, (src - (uint8_t*)bufptr) - 4) == s->CRC)
		s->CRC = 1;
	else
		System_Error("CRC Value is WRONG!");

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_item_data_d(const void *bufptr, size_t bufsz, struct item_data *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_bool(s->Available);
	unpack_int16_t(s->UsedBy);
	unpack_charArr(s->szName);
	unpack_int8(s->cType);
	unpack_bool(s->Special);
	unpack_int16_t(s->SpellNum);
	unpack_charArr(s->Attributes);
	unpack_charArr(s->ReqAttributes);
	unpack_int32_t(s->lCost);
	unpack_bool(s->DiffMaterials);
	unpack_int16_t(s->Energy);
	unpack_int16_t(s->MarketLevel);
	unpack_int16_t(s->VillageType);
	unpack_int32_t(s->ItemDate);
	unpack_int8(s->RandLevel);
	unpack_int8(s->HPAdd);
	unpack_int8(s->SPAdd);
	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_empire_d(const void *bufptr, size_t bufsz, struct empire *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_charArr(s->szName);
	unpack_int16_t(s->OwnerType);
	unpack_int32_t(s->VaultGold);
	unpack_int16_t(s->Land);
	unpack_int16_tArr(s->Buildings);
	unpack_int16_t(s->AllianceID);
	unpack_int8(s->WorkerEnergy);
	unpack_int16_t(s->LandDevelopedToday);
	unpack_int16_t(s->SpiesToday);
	unpack_int16_t(s->AttacksToday);
	unpack_int32_t(s->Points);
	unpack_struct(&s->Army, Army);
	unpack_int32_t(s->CRC);
	if (CRCValue(bufptr, (src - (uint8_t*)bufptr) - 4) == s->CRC)
		s->CRC = 1;
	else
		System_Error("CRC Value is WRONG!");

	return (size_t)(src - (uint8_t *)bufptr);
}

size_t
s_clan_d(const void *bufptr, size_t bufsz, struct clan *s)
{
	size_t remain = bufsz;
	const uint8_t *src = bufptr;

	unpack_int16_tArr(s->ClanID);
	unpack_charArr(s->szUserName);
	unpack_charArr(s->szName);
	unpack_charArr(s->Symbol);
	unpack_charArr(s->QuestsDone);
	unpack_charArr(s->QuestsKnown);
	unpack_charArr(s->PFlags);
	unpack_charArr(s->DFlags);
	unpack_uint8_t(s->ChatsToday);
	unpack_uint8_t(s->TradesToday);
	unpack_int16_tArr(s->ClanRulerVote);
	unpack_int16_tArr(s->Alliances);
	unpack_int32_t(s->Points);
	unpack_charArr(s->szDateOfLastGame);
	unpack_int16_t(s->FightsLeft);
	unpack_int16_t(s->ClanFights);
	unpack_int16_t(s->MineLevel);
	unpack_int16_t(s->WorldStatus);
	unpack_int16_t(s->DestinationBBS);
	unpack_int8(s->VaultWithdrawals);
	unpack_uint16_t(s->PublicMsgIndex);
	unpack_int16_tArr2(s->ClanCombatToday, MAX_CLANCOMBAT, 2);
	unpack_int16_t(s->ClanWars);
	unpack_ptrArr(s->Member);
	unpack_structArr(s->Items, item_data);
	unpack_int8(s->ResUncToday);
	unpack_int8(s->ResDeadToday);
	unpack_struct(&s->Empire, empire);

	// Bitfields...
	assert(remain);
	if (!remain)
		return SIZE_MAX;
	s->DefActionHelp = (*src & 0x80 ? 1U : 0U);
	s->CommHelp = (*src & 0x40 ? 1U : 0U);
	s->MineHelp = (*src & 0x20 ? 1U : 0U);
	s->MineLevelHelp = (*src & 0x10 ? 1U : 0U);
	s->CombatHelp = (*src & 0x08 ? 1U : 0U);
	s->TrainHelp = (*src & 0x04 ? 1U : 0U);
	s->MarketHelp = (*src & 0x02 ? 1U : 0U);
	s->PawnHelp = (*(src++) & 0x01 ? 1U : 0U);
	remain--;

	assert(remain);
	if (!remain)
		return SIZE_MAX;
	s->WizardHelp = (*src & 0x80 ? 1U : 0U);
	s->EmpireHelp = (*src & 0x40 ? 1U : 0U);
	s->DevelopHelp = (*src & 0x20 ? 1U : 0U);
	s->TownHallHelp = (*src & 0x10 ? 1U : 0U);
	s->DestroyHelp = (*src & 0x08 ? 1U : 0U);
	s->ChurchHelp = (*src & 0x04 ? 1U : 0U);
	s->THallHelp = (*src & 0x02 ? 1U : 0U);
	s->SpyHelp = (*(src++) & 0x01 ? 1U : 0U);
	remain--;

	assert(remain >= 2);
	if (remain < 2)
		return SIZE_MAX;
	s->AllyHelp = (*src & 0x80 ? 1U : 0U);
	s->WarHelp = (*src & 0x40 ? 1U : 0U);
	s->VoteHelp = (*src & 0x20 ? 1U : 0U);
	s->TravelHelp = (*src & 0x10 ? 1U : 0U);
	s->WasRulerToday = (*src & 0x08 ? 1U : 0U);
	s->MadeAlliance = (*src & 0x04 ? 1U : 0U);
	s->Protection = (src[0] & 0x03U << 2) | (src[1] & 0xC0U >> 6);
	src++;
	remain--;

	assert(remain);
	if (!remain)
		return SIZE_MAX;
	s->FirstDay = (*src & 0x20 ? 1U : 0U);
	s->Eliminated = (*src & 0x10 ? 1U : 0U);
	s->QuestToday = (*src & 0x08 ? 1U : 0U);
	s->AttendedMass = (*src & 0x04 ? 1U : 0U);
	s->GotBlessing = (*src & 0x02 ? 1U : 0U);
	s->Prayed = (*(src++) & 0x01 ? 1U : 0U);
	remain--;

	unpack_int32_t(s->CRC);
	if (CRCValue(bufptr, (src - (uint8_t*)bufptr) - 4) == s->CRC)
		s->CRC = 1;
	else
		System_Error("CRC Value is WRONG!");
	return (size_t)(src - (uint8_t *)bufptr);
}

#if 0
#include <stddef.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	char buf[4036];
	struct clan c = {0};
	struct empire e = {0};
	struct item_data i = {0};
	struct Alliance a = {0};
	struct Army ar = {0};
	struct Strategy s = {0};
	struct AttackPacket ap = {0};
	struct AttackResult at = {0};
	struct EventHeader eh = {0};
	struct FileHeader fh = {0};
	struct game_data gd = {0};
	struct ibbs_node_attack ina = {0};
	struct ibbs_node_reset inr = {0};
	struct ibbs_node_recon inre = {0};
	struct Language l = {0};
	struct Topic t = {0};
	struct NPCInfo npci = {0};
	struct NPCNdx npcn = {0};
	struct Packet p = {0};
	struct SpellsInEffect sie = {0};
	struct pc pc = {0};
	struct PClass pcl = {0};
	struct Spell sp = {0};
	struct SpyAttemptPacket sap = {0};
	struct SpyResultPacket srp = {0};
	struct TradeData td = {0};
	struct TradeList tl = {0};
	struct UserInfo ui = {0};
	struct UserScore us = {0};
	struct village_data vd = {0};
	struct LeavingData ld = {0};
	struct Msg_Txt mt = {0};
	struct Message m = {0};
	struct MessageHeader mh = {0};

	printf("#define BUF_SIZE_clan %zu\n#define STRUCT_SIZE_clan %zu\n\n", s_clan_d(buf, sizeof(buf), &c), sizeof(c));
	printf("#define BUF_SIZE_empire %zu\n#define STRUCT_SIZE_empire %zu\n\n", s_empire_d(buf, sizeof(buf), &e), sizeof(e));
	printf("#define BUF_SIZE_item_data %zu\n#define STRUCT_SIZE_item_data %zu\n\n", s_item_data_d(buf, sizeof(buf), &i), sizeof(i));
	printf("#define BUF_SIZE_Alliance %zu\n#define STRUCT_SIZE_Alliance %zu\n\n", s_Alliance_d(buf, sizeof(buf), &a), sizeof(a));
	printf("#define BUF_SIZE_Army %zu\n#define STRUCT_SIZE_Army %zu\n\n", s_Army_d(buf, sizeof(buf), &ar), sizeof(ar));
	printf("#define BUF_SIZE_Strategy %zu\n#define STRUCT_SIZE_Strategy %zu\n\n", s_Strategy_d(buf, sizeof(buf), &s), sizeof(s));
	printf("#define BUF_SIZE_AttackPacket %zu\n#define STRUCT_SIZE_AttackPacket %zu\n\n", s_AttackPacket_d(buf, sizeof(buf), &ap), sizeof(ap));
	printf("#define BUF_SIZE_AttackResult %zu\n#define STRUCT_SIZE_AttackResult %zu\n\n", s_AttackResult_d(buf, sizeof(buf), &at), sizeof(at));
	printf("#define BUF_SIZE_EventHeader %zu\n#define STRUCT_SIZE_EventHeader %zu\n\n", s_EventHeader_d(buf, sizeof(buf), &eh), sizeof(eh));
	printf("#define BUF_SIZE_FileHeader %zu\n#define STRUCT_SIZE_FileHeader %zu\n\n", s_FileHeader_d(buf, sizeof(buf), &fh), sizeof(fh));
	printf("#define BUF_SIZE_game_data %zu\n#define STRUCT_SIZE_game_data %zu\n\n", s_game_data_d(buf, sizeof(buf), &gd), sizeof(gd));
	printf("#define BUF_SIZE_ibbs_node_attack %zu\n#define STRUCT_SIZE_ibbs_node_attack %zu\n\n", s_ibbs_node_attack_d(buf, sizeof(buf), &ina), sizeof(ina));
	printf("#define BUF_SIZE_ibbs_node_reset %zu\n#define STRUCT_SIZE_ibbs_node_reset %zu\n\n", s_ibbs_node_reset_d(buf, sizeof(buf), &inr), sizeof(inr));
	printf("#define BUF_SIZE_ibbs_node_recon %zu\n#define STRUCT_SIZE_ibbs_node_recon %zu\n\n", s_ibbs_node_recon_d(buf, sizeof(buf), &inre), sizeof(inre));
	printf("#define BUF_SIZE_Language %zu\n#define STRUCT_SIZE_Language %zu\n\n", s_Language_d(buf, sizeof(buf), &l), sizeof(l));
	printf("#define BUF_SIZE_Topic %zu\n#define STRUCT_SIZE_Topic %zu\n\n", s_Topic_d(buf, sizeof(buf), &t), sizeof(t));
	printf("#define BUF_SIZE_NPCInfo %zu\n#define STRUCT_SIZE_NPCInfo %zu\n\n", s_NPCInfo_d(buf, sizeof(buf), &npci), sizeof(npci));
	printf("#define BUF_SIZE_NPCNdx %zu\n#define STRUCT_SIZE_NPCNdx %zu\n\n", s_NPCNdx_d(buf, sizeof(buf), &npcn), sizeof(npcn));
	printf("#define BUF_SIZE_Packet %zu\n#define STRUCT_SIZE_Packet %zu\n\n", s_Packet_d(buf, sizeof(buf), &p), sizeof(p));
	printf("#define BUF_SIZE_SpellsInEffect %zu\n#define STRUCT_SIZE_SpellsInEffect %zu\n\n", s_SpellsInEffect_d(buf, sizeof(buf), &sie), sizeof(sie));
	printf("#define BUF_SIZE_pc %zu\n#define STRUCT_SIZE_pc %zu\n\n", s_pc_d(buf, sizeof(buf), &pc), sizeof(pc));
	printf("#define BUF_SIZE_PClass %zu\n#define STRUCT_SIZE_PClass %zu\n\n", s_PClass_d(buf, sizeof(buf), &pcl), sizeof(pcl));
	printf("#define BUF_SIZE_Spell %zu\n#define STRUCT_SIZE_Spell %zu\n\n", s_Spell_d(buf, sizeof(buf), &sp), sizeof(sp));
	printf("#define BUF_SIZE_SpyAttemptPacket %zu\n#define STRUCT_SIZE_SpyAttemptPacket %zu\n\n", s_SpyAttemptPacket_d(buf, sizeof(buf), &sap), sizeof(sap));
	printf("#define BUF_SIZE_SpyResultPacket %zu\n#define STRUCT_SIZE_SpyResultPacket %zu\n\n", s_SpyResultPacket_d(buf, sizeof(buf), &srp), sizeof(srp));
	printf("#define BUF_SIZE_TradeData %zu\n#define STRUCT_SIZE_TradeData %zu\n\n", s_TradeData_d(buf, sizeof(buf), &td), sizeof(td));
	printf("#define BUF_SIZE_TradeList %zu\n#define STRUCT_SIZE_TradeList %zu\n\n", s_TradeList_d(buf, sizeof(buf), &tl), sizeof(tl));
	printf("#define BUF_SIZE_UserInfo %zu\n#define STRUCT_SIZE_UserInfo %zu\n\n", s_UserInfo_d(buf, sizeof(buf), &ui), sizeof(ui));
	printf("#define BUF_SIZE_UserScore %zu\n#define STRUCT_SIZE_UserScore %zu\n\n", s_UserScore_d(buf, sizeof(buf), &us), sizeof(us));
	printf("#define BUF_SIZE_village_data %zu\n#define STRUCT_SIZE_village_data %zu\n\n", s_village_data_d(buf, sizeof(buf), &vd), sizeof(vd));
	printf("#define BUF_SIZE_LeavingData %zu\n#define STRUCT_SIZE_LeavingData %zu\n\n", s_LeavingData_d(buf, sizeof(buf), &ld), sizeof(ld));
	printf("#define BUF_SIZE_Msg_Txt %zu\n#define STRUCT_SIZE_Msg_Txt %zu\n\n", s_Msg_Txt_d(buf, sizeof(buf), &mt), sizeof(mt));
	printf("#define BUF_SIZE_Message %zu\n#define STRUCT_SIZE_Message %zu\n\n", s_Message_d(buf, sizeof(buf), &m), sizeof(m));
	printf("#define BUF_SIZE_MessageHeader %zu\n#define STRUCT_SIZE_MessageHeader %zu\n\n", s_MessageHeader_d(buf, sizeof(buf), &mh), sizeof(mh));

	return 0;
}
#endif
