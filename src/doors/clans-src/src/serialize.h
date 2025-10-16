#ifndef THE_CLANS__SERIALIZE___H
#define THE_CLANS__SERIALIZE___H

#include "interbbs.h"
#include "myopen.h"
#include "quests.h"
#include "structs.h"

size_t s_Alliance_s(const struct Alliance *s, void *bufptr, size_t bufsz);
size_t s_Army_s(const struct Army *s, void *bufptr, size_t bufsz);
size_t s_Strategy_s(const struct Strategy *s, void *bufptr, size_t bufsz);
size_t s_clan_s(const struct clan *s, void *bufptr, size_t bufsz);
size_t s_empire_s(const struct empire *s, void *bufptr, size_t bufsz);
size_t s_item_data_s(const struct item_data *s, void *bufptr, size_t bufsz);
size_t s_AttackPacket_s(const struct AttackPacket *s, void *bufptr, size_t bufsz);
size_t s_AttackResult_s(const struct AttackResult *s, void *bufptr, size_t bufsz);
size_t s_EventHeader_s(const struct EventHeader *s, void *bufptr, size_t bufsz);
size_t s_FileHeader_s(const struct FileHeader *s, void *bufptr, size_t bufsz);
size_t s_game_data_s(const struct game_data *s, void *bufptr, size_t bufsz);
size_t s_ibbs_node_attack_s(const struct ibbs_node_attack *s, void *bufptr, size_t bufsz);
size_t s_ibbs_node_reset_s(const struct ibbs_node_reset *s, void *bufptr, size_t bufsz);
size_t s_ibbs_node_recon_s(const struct ibbs_node_recon *s, void *bufptr, size_t bufsz);
size_t s_Language_s(const struct Language *s, void *bufptr, size_t bufsz);
size_t s_Topic_s(const struct Topic *s, void *bufptr, size_t bufsz);
size_t s_NPCInfo_s(const struct NPCInfo *s, void *bufptr, size_t bufsz);
size_t s_NPCNdx_s(const struct NPCNdx *s, void *bufptr, size_t bufsz);
size_t s_Packet_s(const struct Packet *s, void *bufptr, size_t bufsz);
size_t s_SpellsInEffect_s(const struct SpellsInEffect *s, void *bufptr, size_t bufsz);
size_t s_pc_s(const struct pc *s, void *bufptr, size_t bufsz);
size_t s_PClass_s(const struct PClass *s, void *bufptr, size_t bufsz);
size_t s_Spell_s(const struct Spell *s, void *bufptr, size_t bufsz);
size_t s_SpyAttemptPacket_s(const struct SpyAttemptPacket *s, void *bufptr, size_t bufsz);
size_t s_SpyResultPacket_s(const struct SpyResultPacket *s, void *bufptr, size_t bufsz);
size_t s_TradeData_s(const struct TradeData *s, void *bufptr, size_t bufsz);
size_t s_TradeList_s(const struct TradeList *s, void *bufptr, size_t bufsz);
size_t s_UserInfo_s(const struct UserInfo *s, void *bufptr, size_t bufsz);
size_t s_UserScore_s(const struct UserScore *s, void *bufptr, size_t bufsz);
size_t s_village_data_s(const struct village_data *s, void *bufptr, size_t bufsz);
size_t s_LeavingData_s(const struct LeavingData *s, void *bufptr, size_t bufsz);
size_t s_Msg_Txt_s(const struct Msg_Txt *s, void *bufptr, size_t bufsz);
size_t s_Message_s(const struct Message *s, void *bufptr, size_t bufsz);
size_t s_MessageHeader_s(const struct MessageHeader *s, void *bufptr, size_t bufsz);

#endif
