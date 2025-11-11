#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

#include "serialize.h"

#define pack_char(x) do {        \
	assert(remain);           \
	if (remain < sizeof(char)) \
		return SIZE_MAX;    \
	*(char *)(dst++) = x;        \
	remain--;                     \
} while(0)

#define pack_int8(x) do {        \
	assert(remain);           \
	if (remain < sizeof(char)) \
		return SIZE_MAX;    \
	*(int8_t *)(dst++) = x;      \
	remain--;                     \
} while(0)

#define pack_uint8_t(x) do {     \
	assert(remain);           \
	if (remain < sizeof(char)) \
		return SIZE_MAX;    \
	*(dst++) = x;                \
	remain--;                     \
} while(0)

#define pack_bool(x) do {        \
	assert(remain);           \
	if (remain < sizeof(char)) \
		return SIZE_MAX;    \
	*(dst++) = x;                \
	remain--;                     \
} while(0)

#define pack_int16_t(x) do {         \
	int16_t tmp = x;              \
	assert(remain >= sizeof(tmp)); \
	if (remain < sizeof(tmp))       \
		return SIZE_MAX;         \
	tmp = SWAP16S(tmp);               \
	memcpy(dst, &tmp, sizeof(tmp));    \
	dst += sizeof(tmp);                 \
	remain -= sizeof(tmp);               \
} while(0)

#define pack_uint16_t(x) do {        \
	uint16_t tmp = x;             \
	assert(remain >= sizeof(tmp)); \
	if (remain < sizeof(tmp))       \
		return SIZE_MAX;         \
	tmp = SWAP16(tmp);                \
	memcpy(dst, &tmp, sizeof(tmp));    \
	dst += sizeof(tmp);                 \
	remain -= sizeof(tmp);               \
} while(0)

#define pack_int32_t(x) do {         \
	int32_t tmp = x;              \
	assert(remain >= sizeof(tmp)); \
	if (remain < sizeof(tmp))       \
		return SIZE_MAX;         \
	tmp = SWAP32S(tmp);               \
	memcpy(dst, &tmp, sizeof(tmp));    \
	dst += sizeof(tmp);                 \
	remain -= sizeof(tmp);               \
} while(0)

#define pack_int16_tArr(x) do {                                    \
	for (size_t ctr = 0; ctr < sizeof(x) / sizeof(*(x)); ctr++) \
		pack_int16_t(x[ctr]);                                \
} while(0)

#define pack_uint16_tArr(x) do {                                   \
	for (size_t ctr = 0; ctr < sizeof(x) / sizeof(*(x)); ctr++) \
		pack_uint16_t(x[ctr]);                               \
} while(0)

#define pack_int16_tArr2(x, szo, szi) do {              \
	for (size_t ctro = 0; ctro < szo; ctro++)        \
		for (size_t ctri = 0; ctri < szi; ctri++) \
			pack_int16_t(x[ctro][ctri]);       \
} while(0)

#define pack_charSz(x) do {            \
	assert(remain >= sizeof(x));    \
	if (remain < sizeof(x))          \
		return SIZE_MAX;          \
	strncpy((char*)dst, x, sizeof(x)); \
	dst[sizeof(x) - 1] = (char)0;       \
	dst += sizeof(x);                    \
	remain -= sizeof(x);                  \
} while(0)

#define pack_int8Arr(x) do {        \
	assert(remain >= sizeof(x)); \
	if (remain < sizeof(x))       \
		return SIZE_MAX;       \
	memcpy(dst, x, sizeof(x));      \
	dst += sizeof(x);                \
	remain -= sizeof(x);              \
} while(0)

#define pack_uint8Arr(x) do {       \
	assert(remain >= sizeof(x)); \
	if (remain < sizeof(x))       \
		return SIZE_MAX;       \
	memcpy(dst, x, sizeof(x));      \
	dst += sizeof(x);                \
	remain -= sizeof(x);              \
} while(0)

#define pack_ptr(x) do { \
} while(0)

// Setting these to zero... hopefully they don't round-trip. :(
#define pack_ptrArr(x) do {                                         \
	for (size_t ctr = 0; ctr < sizeof(x) / sizeof(*(x)); ctr++); \
} while(0)

#define pack_struct(x, s) do {                     \
	size_t res = s_ ## s ## _s(x, dst, remain); \
	if (res == SIZE_MAX)                         \
		return SIZE_MAX;                      \
	dst += res;                                    \
	remain -= res;                                  \
} while(0)

#define pack_structArr(x, s) do {                                  \
	for (size_t ctr = 0; ctr < sizeof(x) / sizeof(*(x)); ctr++) \
		pack_struct(&x[ctr], s);                             \
} while(0)

size_t
s_ibbs_node_attack_s(const struct ibbs_node_attack *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_t(s->ReceiveIndex);
	pack_int16_t(s->SendIndex);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_MessageHeader_s(const struct MessageHeader *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_charSz(s->szFromUserName);
	pack_charSz(s->szToUserName);
	pack_charSz(s->szSubject);
	pack_charSz(s->szDateTime);
	pack_uint16_t(s->wTimesRead);
	pack_int16_t(s->wDestNode);
	pack_int16_t(s->wOrigNode);
	pack_uint16_t(s->wCost);
	pack_int16_t(s->wOrigNet);
	pack_int16_t(s->wDestNet);
	pack_int16_t(s->wDestZone);
	pack_int16_t(s->wOrigZone);
	pack_int16_t(s->wDestPoint);
	pack_int16_t(s->wOrigPoint);
	pack_uint16_t(s->wReplyTo);
	pack_uint16_t(s->wAttribute);
	pack_uint16_t(s->wNextReply);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_ibbs_node_reset_s(const struct ibbs_node_reset *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_t(s->Received);
	pack_int32_t(s->LastSent);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_ibbs_node_recon_s(const struct ibbs_node_recon *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int32_t(s->LastReceived);
	pack_int32_t(s->LastSent);
	pack_char(s->PacketIndex);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Msg_Txt_s(const struct Msg_Txt *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_tArr(s->Offsets);
	pack_int16_t(s->Length);
	pack_int16_t(s->NumLines);
	pack_ptr(s->MsgTxt);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Message_s(const struct Message *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_tArr(s->ToClanID);
	pack_int16_tArr(s->FromClanID);
	pack_charSz(s->szFromName);
	pack_charSz(s->szDate);
	pack_charSz(s->szAllyName);
	pack_charSz(s->szFromVillageName);
	pack_int16_t(s->MessageType);
	pack_int16_t(s->AllianceID);
	pack_int16_t(s->Flags);
	pack_int16_t(s->BBSIDFrom);
	pack_int16_t(s->BBSIDTo);
	pack_uint16_t(s->PublicMsgIndex);
	pack_struct(&s->Data, Msg_Txt);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Topic_s(const struct Topic *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_bool(s->Known);
	pack_bool(s->Active);
	pack_bool(s->ClanInfo);
	pack_charSz(s->szName);
	pack_charSz(s->szFileName);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_LeavingData_s(const struct LeavingData *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_bool(s->Active);
	pack_int16_t(s->DestID);
	pack_int16_tArr(s->ClanID);
	pack_int32_t(s->Followers);
	pack_int32_t(s->Footmen);
	pack_int32_t(s->Axemen);
	pack_int32_t(s->Knights);
	pack_int32_t(s->Catapults);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_PClass_s(const struct PClass *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_charSz(s->szName);
	pack_int8Arr(s->Attributes);
	pack_int16_t(s->MaxHP);
	pack_int16_t(s->Gold);
	pack_int16_t(s->MaxSP);
	pack_int8Arr(s->SpellsKnown);
	pack_int16_t(s->VillageType);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_SpellsInEffect_s(const struct SpellsInEffect *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_t(s->SpellNum);
	pack_int16_t(s->Energy);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_SpyAttemptPacket_s(const struct SpyAttemptPacket *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_charSz(s->szSpierName);
	pack_int16_t(s->IntelligenceLevel);
	pack_int16_t(s->TargetType);
	pack_int16_tArr(s->ClanID);
	pack_int16_tArr(s->MasterID);
	pack_int16_t(s->BBSFromID);
	pack_int16_t(s->BBSToID);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_SpyResultPacket_s(const struct SpyResultPacket *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_t(s->BBSFromID);
	pack_int16_t(s->BBSToID);
	pack_int16_tArr(s->MasterID);
	pack_charSz(s->szTargetName);
	pack_bool(s->Success);
	pack_struct(&s->Empire, empire);
	pack_charSz(s->szTheDate);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_TradeData_s(const struct TradeData *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_struct(&s->Giving, TradeList);
	pack_struct(&s->Asking, TradeList);
	pack_bool(s->Active);
	pack_int16_tArr(s->FromClanID);
	pack_int16_tArr(s->ToClanID);
	pack_charSz(s->szFromClan);
	pack_int32_t(s->Code);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_TradeList_s(const struct TradeList *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int32_t(s->Gold);
	pack_int32_t(s->Followers);
	pack_int32_t(s->Footmen);
	pack_int32_t(s->Axemen);
	pack_int32_t(s->Knights);
	pack_int32_t(s->Catapults);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_UserInfo_s(const struct UserInfo *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_tArr(s->ClanID);
	pack_bool(s->Deleted);
	pack_charSz(s->szMasterName);
	pack_charSz(s->szName);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_UserScore_s(const struct UserScore *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_tArr(s->ClanID);
	pack_charSz(s->Symbol);
	pack_charSz(s->szName);
	pack_int32_t(s->Points);
	pack_int16_t(s->BBSID);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Spell_s(const struct Spell *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_charSz(s->szName);
	pack_int16_t(s->TypeFlag);
	pack_bool(s->Friendly);
	pack_bool(s->Target);
	pack_int8Arr(s->Attributes);
	pack_int8(s->Value);
	pack_int16_t(s->Energy);
	pack_int8(s->Level);
	pack_ptr(s->pszDamageStr);
	pack_ptr(s->pszHealStr);
	pack_ptr(s->pszModifyStr);
	pack_ptr(s->pszWearoffStr);
	pack_ptr(s->pszStatusStr);
	pack_ptr(s->pszOtherStr);
	pack_ptr(s->pszUndeadName);
	pack_int16_t(s->SP);
	pack_bool(s->StrengthCanReduce);
	pack_bool(s->WisdomCanReduce);
	pack_uint8_t((uint8_t)(s->MultiAffect << 7));

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_pc_s(const struct pc *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;
	int32_t crc;

	pack_charSz(s->szName);
	pack_int16_t(s->HP);
	pack_int16_t(s->MaxHP);
	pack_int16_t(s->SP);
	pack_int16_t(s->MaxSP);
	pack_int8Arr(s->Attributes);
	pack_int8(s->Status);
	pack_int16_t(s->Weapon);
	pack_int16_t(s->Shield);
	pack_int16_t(s->Armor);
	pack_int16_t(s->WhichRace);
	pack_int16_t(s->WhichClass);
	pack_int32_t(s->Experience);
	pack_int16_t(s->Level);
	pack_int16_t(s->TrainingPoints);
	pack_ptr(s->MyClan);
	pack_int8Arr(s->SpellsKnown);
	pack_structArr(s->SpellsInEffect, SpellsInEffect);
	pack_int16_t(s->Difficulty);
	pack_uint8_t((uint8_t)((s->Undead << 7) | s->DefaultAction));
	crc = CRCValue(bufptr, dst - (uint8_t*)bufptr);
	pack_int32_t(crc);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Packet_s(const struct Packet *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_bool(s->Active);
	pack_charSz(s->GameID);
	pack_charSz(s->szDate);
	pack_int16_t(s->BBSIDFrom);
	pack_int16_t(s->BBSIDTo);
	pack_int16_t(s->PacketType);
	pack_int32_t(s->PacketLength);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_NPCInfo_s(const struct NPCInfo *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_charSz(s->szName);
	pack_structArr(s->Topics, Topic);
	pack_struct(&s->IntroTopic, Topic);
	pack_int8(s->Loyalty);
	pack_int16_t(s->WhereWander);
	pack_bool(s->Roamer);
	pack_int16_t(s->NPCPCIndex);
	pack_int16_t(s->KnownTopics);
	pack_int16_t(s->MaxTopics);
	pack_int16_t(s->OddsOfSeeing);
	pack_charSz(s->szHereNews);
	pack_charSz(s->szQuoteFile);
	pack_charSz(s->szMonFile);
	pack_charSz(s->szIndex);
	pack_int16_t(s->VillageType);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_NPCNdx_s(const struct NPCNdx *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_charSz(s->szIndex);
	pack_bool(s->InClan);
	pack_int16_tArr(s->ClanID);
	pack_int16_t(s->WhereWander);
	pack_int16_t(s->Status);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Language_s(const struct Language *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_charSz(s->Signature);
	pack_uint16_tArr(s->StrOffsets);
	pack_uint16_t(s->NumBytes);
	pack_ptr(s->BigString);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Alliance_s(const struct Alliance *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int16_t(s->ID);
	pack_charSz(s->szName);
	pack_int16_tArr(s->CreatorID);
	pack_int16_tArr(s->OriginalCreatorID);
	pack_int16_tArr2(s->Member, MAX_ALLIANCEMEMBERS, 2);
	pack_struct(&s->Empire, empire);
	pack_structArr(s->Items, item_data);
	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_game_data_s(const struct game_data *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;
	int32_t crc;

	pack_int16_t(s->GameState);
	pack_bool(s->InterBBS);
	pack_charSz(s->szTodaysDate);
	pack_charSz(s->szDateGameStart);
	pack_charSz(s->szLastJoinDate);
	pack_int16_t(s->NextClanID);
	pack_int16_t(s->NextAllianceID);
	pack_charSz(s->szWorldName);
	pack_charSz(s->LeagueID);
	pack_charSz(s->GameID);
	pack_bool(s->ClanTravel);
	pack_int16_t(s->LostDays);
	pack_int16_t(s->MaxPermanentMembers);
	pack_bool(s->ClanEmpires);
	pack_int16_t(s->MineFights);
	pack_int16_t(s->ClanFights);
	pack_int16_t(s->DaysOfProtection);
	crc = CRCValue(bufptr, dst - (uint8_t*)bufptr);
	pack_int32_t(crc);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_village_data_s(const struct village_data *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;
	int32_t crc;

	pack_int8Arr(s->ColorScheme);
	pack_charSz(s->szName);
	pack_int16_t(s->TownType);
	pack_int16_t(s->TaxRate);
	pack_int16_t(s->InterestRate);
	pack_int16_t(s->GST);
	pack_int16_t(s->ConscriptionRate);
	pack_int16_tArr(s->RulingClanId);
	pack_charSz(s->szRulingClan);
	pack_int16_t(s->GovtSystem);
	pack_int16_t(s->RulingDays);
	pack_uint16_t(s->PublicMsgIndex);
	pack_int16_t(s->MarketLevel);
	pack_int16_t(s->TrainingHallLevel);
	pack_int16_t(s->ChurchLevel);
	pack_int16_t(s->PawnLevel);
	pack_int16_t(s->WizardLevel);

	assert(remain);
	if (!remain)
		return SIZE_MAX;
	*dst = 0;
	*dst |= (s->SetTaxToday << 7);
	*dst |= (s->SetInterestToday << 6);
	*dst |= (s->SetGSTToday << 5);
	*dst |= (s->UpMarketToday << 4);
	*dst |= (s->UpTHallToday << 3);
	*dst |= (s->UpChurchToday << 2);
	*dst |= (s->UpPawnToday << 1);
	*(dst++) |= (s->UpWizToday << 0);

	remain--;
	assert(remain);
	if (!remain)
		return SIZE_MAX;
	*dst = 0;
	*dst |= (s->SetConToday << 7);
	*(dst++) |= (s->ShowEmpireStats << 6);
	remain--;

	pack_uint8Arr(s->HFlags);
	pack_uint8Arr(s->GFlags);
	pack_int16_t(s->VillageType);
	pack_int16_t(s->CostFluctuation);
	pack_int16_t(s->MarketQuality);
	pack_struct(&s->Empire, empire);
	crc = CRCValue(bufptr, dst - (uint8_t*)bufptr);
	pack_int32_t(crc);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_EventHeader_s(const struct EventHeader *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_charSz(s->szName);
	pack_int32_t(s->EventSize);
	pack_bool(s->Event);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_FileHeader_s(const struct FileHeader *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_ptr(s->fp);
	pack_charSz(s->szFileName);
	pack_int32_t(s->lStart);
	pack_int32_t(s->lEnd);
	pack_int32_t(s->lFileSize);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Army_s(const struct Army *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;
	int32_t crc;

	pack_int32_t(s->Footmen);
	pack_int32_t(s->Axemen);
	pack_int32_t(s->Knights);
	pack_int32_t(s->Followers);
	pack_int8(s->Rating);
	pack_int8(s->Level);
	pack_struct(&s->Strategy, Strategy);
	crc = CRCValue(bufptr, dst - (uint8_t*)bufptr);
	pack_int32_t(crc);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_Strategy_s(const struct Strategy *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_int8(s->AttackLength);
	pack_int8(s->AttackIntensity);
	pack_int8(s->LootLevel);
	pack_int8(s->DefendLength);
	pack_int8(s->DefendIntensity);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_AttackPacket_s(const struct AttackPacket *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;
	int32_t crc;

	pack_int16_t(s->BBSFromID);
	pack_int16_t(s->BBSToID);
	pack_struct(&s->AttackingEmpire, empire);
	pack_struct(&s->AttackingArmy, Army);
	pack_int16_t(s->Goal);
	pack_int16_t(s->ExtentOfAttack);
	pack_int16_t(s->TargetType);
	pack_int16_tArr(s->ClanID);
	pack_int16_tArr(s->AttackOriginatorID);
	pack_int16_t(s->AttackIndex);
	crc = CRCValue(bufptr, dst - (uint8_t*)bufptr);
	pack_int32_t(crc);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_AttackResult_s(const struct AttackResult *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;
	int32_t crc;

	pack_bool(s->Success);
	pack_bool(s->NoTarget);
	pack_bool(s->InterBBS);
	pack_struct(&s->OrigAttackArmy, Army);
	pack_int16_t(s->AttackerType);
	pack_int16_tArr(s->AttackerID);
	pack_int16_t(s->AllianceID);
	pack_charSz(s->szAttackerName);
	pack_int16_t(s->DefenderType);
	pack_int16_tArr(s->DefenderID);
	pack_charSz(s->szDefenderName);
	pack_int16_t(s->BBSIDFrom);
	pack_int16_t(s->BBSIDTo);
	pack_int16_t(s->PercentDamage);
	pack_int16_t(s->Goal);
	pack_int16_t(s->ExtentOfAttack);
	pack_struct(&s->AttackCasualties, Army);
	pack_struct(&s->DefendCasualties, Army);
	pack_int16_tArr(s->BuildingsDestroyed);
	pack_int32_t(s->GoldStolen);
	pack_int16_t(s->LandStolen);
	pack_struct(&s->ReturningArmy, Army);
	pack_int16_t(s->ResultIndex);
	pack_int16_t(s->AttackIndex);
	crc = CRCValue(bufptr, dst - (uint8_t*)bufptr);
	pack_int32_t(crc);

	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_item_data_s(const struct item_data *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;

	pack_bool(s->Available);
	pack_int16_t(s->UsedBy);
	pack_charSz(s->szName);
	pack_int8(s->cType);
	pack_bool(s->Special);
	pack_int16_t(s->SpellNum);
	pack_int8Arr(s->Attributes);
	pack_int8Arr(s->ReqAttributes);
	pack_int32_t(s->lCost);
	pack_bool(s->DiffMaterials);
	pack_int16_t(s->Energy);
	pack_int16_t(s->MarketLevel);
	pack_int16_t(s->VillageType);
	pack_int32_t(s->ItemDate);
	pack_int8(s->RandLevel);
	pack_int8(s->HPAdd);
	pack_int8(s->SPAdd);
	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_empire_s(const struct empire *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;
	int32_t crc;

	pack_charSz(s->szName);
	pack_int16_t(s->OwnerType);
	pack_int32_t(s->VaultGold);
	pack_int16_t(s->Land);
	pack_int16_tArr(s->Buildings);
	pack_int16_t(s->AllianceID);
	pack_int8(s->WorkerEnergy);
	pack_int16_t(s->LandDevelopedToday);
	pack_int16_t(s->SpiesToday);
	pack_int16_t(s->AttacksToday);
	pack_int32_t(s->Points);
	pack_struct(&s->Army, Army);
	crc = CRCValue(bufptr, dst - (uint8_t*)bufptr);
	pack_int32_t(crc);
	return (size_t)(dst - (uint8_t *)bufptr);
}

size_t
s_clan_s(const struct clan *s, void *bufptr, size_t bufsz)
{
	size_t remain = bufsz;
	uint8_t *dst = bufptr;
	int32_t crc;

	pack_int16_tArr(s->ClanID);
	pack_charSz(s->szUserName);
	pack_charSz(s->szName);
	pack_charSz(s->Symbol);
	pack_uint8Arr(s->QuestsDone);
	pack_uint8Arr(s->QuestsKnown);
	pack_uint8Arr(s->PFlags);
	pack_uint8Arr(s->DFlags);
	pack_uint8_t(s->ChatsToday);
	pack_uint8_t(s->TradesToday);
	pack_int16_tArr(s->ClanRulerVote);
	pack_int16_tArr(s->Alliances);
	pack_int32_t(s->Points);
	pack_charSz(s->szDateOfLastGame);
	pack_int16_t(s->FightsLeft);
	pack_int16_t(s->ClanFights);
	pack_int16_t(s->MineLevel);
	pack_int16_t(s->WorldStatus);
	pack_int16_t(s->DestinationBBS);
	pack_int8(s->VaultWithdrawals);
	pack_uint16_t(s->PublicMsgIndex);
	pack_int16_tArr2(s->ClanCombatToday, MAX_CLANCOMBAT, 2);
	pack_int16_t(s->ClanWars);
	pack_ptrArr(s->Member);
	pack_structArr(s->Items, item_data);
	pack_int8(s->ResUncToday);
	pack_int8(s->ResDeadToday);
	pack_struct(&s->Empire, empire);

	// Bitfields...
	assert(remain);
	if (!remain)
		return SIZE_MAX;
	*dst = 0;
	*dst |= (s->DefActionHelp ? 0x80U : 0U);
	*dst |= (s->CommHelp ? 0x40U : 0U);
	*dst |= (s->MineHelp ? 0x20U : 0U);
	*dst |= (s->MineLevelHelp ? 0x10U : 0U);
	*dst |= (s->CombatHelp ? 0x08U : 0U);
	*dst |= (s->TrainHelp ? 0x04U : 0U);
	*dst |= (s->MarketHelp ? 0x02U : 0U);
	*(dst++) |= (s->PawnHelp ? 0x01U : 0U);
	remain--;

	assert(remain);
	if (!remain)
		return SIZE_MAX;
	*dst = 0;
	*dst |= (s->WizardHelp ? 0x80U : 0U);
	*dst |= (s->EmpireHelp ? 0x40U : 0U);
	*dst |= (s->DevelopHelp ? 0x20U : 0U);
	*dst |= (s->TownHallHelp ? 0x10U : 0U);
	*dst |= (s->DestroyHelp ? 0x08U : 0U);
	*dst |= (s->ChurchHelp ? 0x04U : 0U);
	*dst |= (s->THallHelp ? 0x02U : 0U);
	*(dst++) |= (s->SpyHelp ? 0x01U : 0U);
	remain--;

	assert(remain);
	if (!remain)
		return SIZE_MAX;
	*dst = 0;
	*dst |= (s->AllyHelp ? 0x80U : 0U);
	*dst |= (s->WarHelp ? 0x40U : 0U);
	*dst |= (s->VoteHelp ? 0x20U : 0U);
	*dst |= (s->TravelHelp ? 0x10U : 0U);
	*dst |= (s->WasRulerToday ? 0x08U : 0U);
	*dst |= (s->MadeAlliance ? 0x04U : 0U);
	*(dst++) |= ((s->Protection & 0x0c) >> 2);
	remain--;

	assert(remain);
	if (!remain)
		return SIZE_MAX;
	*dst = 0;
	*dst |= ((s->Protection & 0x03U) << 6);
	*dst |= (s->FirstDay ? 0x20U : 0U);
	*dst |= (s->Eliminated ? 0x10U : 0U);
	*dst |= (s->QuestToday ? 0x08U : 0U);
	*dst |= (s->AttendedMass ? 0x04U : 0U);
	*dst |= (s->GotBlessing ? 0x02U : 0U);
	*(dst++) |= (uint8_t)(s->Prayed ? 0x01U : 0U);
	remain--;

	crc = CRCValue(bufptr, dst - (uint8_t*)bufptr);
	pack_int32_t(crc);
	return (size_t)(dst - (uint8_t *)bufptr);
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

	printf("#define BUF_SIZE_clan %zu\n#define STRUCT_SIZE_clan %zu\n\n", s_clan_s(&c, buf, sizeof(buf)), sizeof(c));
	printf("#define BUF_SIZE_empire %zu\n#define STRUCT_SIZE_empire %zu\n\n", s_empire_s(&e, buf, sizeof(buf)), sizeof(e));
	printf("#define BUF_SIZE_item_data %zu\n#define STRUCT_SIZE_item_data %zu\n\n", s_item_data_s(&i, buf, sizeof(buf)), sizeof(i));
	printf("#define BUF_SIZE_Alliance %zu\n#define STRUCT_SIZE_Alliance %zu\n\n", s_Alliance_s(&a, buf, sizeof(buf)), sizeof(a));
	printf("#define BUF_SIZE_Army %zu\n#define STRUCT_SIZE_Army %zu\n\n", s_Army_s(&ar, buf, sizeof(buf)), sizeof(ar));
	printf("#define BUF_SIZE_Strategy %zu\n#define STRUCT_SIZE_Strategy %zu\n\n", s_Strategy_s(&s, buf, sizeof(buf)), sizeof(s));
	printf("#define BUF_SIZE_AttackPacket %zu\n#define STRUCT_SIZE_AttackPacket %zu\n\n", s_AttackPacket_s(&ap, buf, sizeof(buf)), sizeof(ap));
	printf("#define BUF_SIZE_AttackResult %zu\n#define STRUCT_SIZE_AttackResult %zu\n\n", s_AttackResult_s(&at, buf, sizeof(buf)), sizeof(at));
	printf("#define BUF_SIZE_EventHeader %zu\n#define STRUCT_SIZE_EventHeader %zu\n\n", s_EventHeader_s(&eh, buf, sizeof(buf)), sizeof(eh));
	printf("#define BUF_SIZE_FileHeader %zu\n#define STRUCT_SIZE_FileHeader %zu\n\n", s_FileHeader_s(&fh, buf, sizeof(buf)), sizeof(fh));
	printf("#define BUF_SIZE_game_data %zu\n#define STRUCT_SIZE_game_data %zu\n\n", s_game_data_s(&gd, buf, sizeof(buf)), sizeof(gd));
	printf("#define BUF_SIZE_ibbs_node_attack %zu\n#define STRUCT_SIZE_ibbs_node_attack %zu\n\n", s_ibbs_node_attack_s(&ina, buf, sizeof(buf)), sizeof(ina));
	printf("#define BUF_SIZE_ibbs_node_reset %zu\n#define STRUCT_SIZE_ibbs_node_reset %zu\n\n", s_ibbs_node_reset_s(&inr, buf, sizeof(buf)), sizeof(inr));
	printf("#define BUF_SIZE_ibbs_node_recon %zu\n#define STRUCT_SIZE_ibbs_node_recon %zu\n\n", s_ibbs_node_recon_s(&inre, buf, sizeof(buf)), sizeof(inre));
	printf("#define BUF_SIZE_Language %zu\n#define STRUCT_SIZE_Language %zu\n\n", s_Language_s(&l, buf, sizeof(buf)), sizeof(l));
	printf("#define BUF_SIZE_Topic %zu\n#define STRUCT_SIZE_Topic %zu\n\n", s_Topic_s(&t, buf, sizeof(buf)), sizeof(t));
	printf("#define BUF_SIZE_NPCInfo %zu\n#define STRUCT_SIZE_NPCInfo %zu\n\n", s_NPCInfo_s(&npci, buf, sizeof(buf)), sizeof(npci));
	printf("#define BUF_SIZE_NPCNdx %zu\n#define STRUCT_SIZE_NPCNdx %zu\n\n", s_NPCNdx_s(&npcn, buf, sizeof(buf)), sizeof(npcn));
	printf("#define BUF_SIZE_Packet %zu\n#define STRUCT_SIZE_Packet %zu\n\n", s_Packet_s(&p, buf, sizeof(buf)), sizeof(p));
	printf("#define BUF_SIZE_SpellsInEffect %zu\n#define STRUCT_SIZE_SpellsInEffect %zu\n\n", s_SpellsInEffect_s(&sie, buf, sizeof(buf)), sizeof(sie));
	printf("#define BUF_SIZE_pc %zu\n#define STRUCT_SIZE_pc %zu\n\n", s_pc_s(&pc, buf, sizeof(buf)), sizeof(pc));
	printf("#define BUF_SIZE_PClass %zu\n#define STRUCT_SIZE_PClass %zu\n\n", s_PClass_s(&pcl, buf, sizeof(buf)), sizeof(pcl));
	printf("#define BUF_SIZE_Spell %zu\n#define STRUCT_SIZE_Spell %zu\n\n", s_Spell_s(&sp, buf, sizeof(buf)), sizeof(sp));
	printf("#define BUF_SIZE_SpyAttemptPacket %zu\n#define STRUCT_SIZE_SpyAttemptPacket %zu\n\n", s_SpyAttemptPacket_s(&sap, buf, sizeof(buf)), sizeof(sap));
	printf("#define BUF_SIZE_SpyResultPacket %zu\n#define STRUCT_SIZE_SpyResultPacket %zu\n\n", s_SpyResultPacket_s(&srp, buf, sizeof(buf)), sizeof(srp));
	printf("#define BUF_SIZE_TradeData %zu\n#define STRUCT_SIZE_TradeData %zu\n\n", s_TradeData_s(&td, buf, sizeof(buf)), sizeof(td));
	printf("#define BUF_SIZE_TradeList %zu\n#define STRUCT_SIZE_TradeList %zu\n\n", s_TradeList_s(&tl, buf, sizeof(buf)), sizeof(tl));
	printf("#define BUF_SIZE_UserInfo %zu\n#define STRUCT_SIZE_UserInfo %zu\n\n", s_UserInfo_s(&ui, buf, sizeof(buf)), sizeof(ui));
	printf("#define BUF_SIZE_UserScore %zu\n#define STRUCT_SIZE_UserScore %zu\n\n", s_UserScore_s(&us, buf, sizeof(buf)), sizeof(us));
	printf("#define BUF_SIZE_village_data %zu\n#define STRUCT_SIZE_village_data %zu\n\n", s_village_data_s(&vd, buf, sizeof(buf)), sizeof(vd));
	printf("#define BUF_SIZE_LeavingData %zu\n#define STRUCT_SIZE_LeavingData %zu\n\n", s_LeavingData_s(&ld, buf, sizeof(buf)), sizeof(ld));
	printf("#define BUF_SIZE_Msg_Txt %zu\n#define STRUCT_SIZE_Msg_Txt %zu\n\n", s_Msg_Txt_s(&mt, buf, sizeof(buf)), sizeof(mt));
	printf("#define BUF_SIZE_Message %zu\n#define STRUCT_SIZE_Message %zu\n\n", s_Message_s(&m, buf, sizeof(buf)), sizeof(m));
	printf("#define BUF_SIZE_MessageHeader %zu\n#define STRUCT_SIZE_MessageHeader %zu\n\n", s_MessageHeader_s(&mh, buf, sizeof(buf)), sizeof(mh));

	return 0;
}
#endif
