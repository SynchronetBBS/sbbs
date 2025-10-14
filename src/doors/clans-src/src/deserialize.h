#ifndef DESERIALIZE_H
#define DESERIALIZE_H

#include "interbbs.h"
#include "myopen.h"
#include "quests.h"
#include "structs.h"

size_t s_Alliance_d(const void *bufptr, size_t bufsz, struct Alliance *s);
size_t s_Army_d(const void *bufptr, size_t bufsz, struct Army *s);
size_t s_Strategy_d(const void *bufptr, size_t bufsz, struct Strategy *s);
size_t s_clan_d(const void *bufptr, size_t bufsz, struct clan *s);
size_t s_empire_d(const void *bufptr, size_t bufsz, struct empire *s);
size_t s_item_data_d(const void *bufptr, size_t bufsz, struct item_data *s);
size_t s_AttackPacket_d(const void *bufptr, size_t bufsz, struct AttackPacket *s);
size_t s_AttackResult_d(const void *bufptr, size_t bufsz, struct AttackResult *s);
size_t s_EventHeader_d(const void *bufptr, size_t bufsz, struct EventHeader *s);
size_t s_FileHeader_d(const void *bufptr, size_t bufsz, struct FileHeader *s);
size_t s_game_data_d(const void *bufptr, size_t bufsz, struct game_data *s);
size_t s_ibbs_node_attack_d(const void *bufptr, size_t bufsz, struct ibbs_node_attack *s);
size_t s_ibbs_node_reset_d(const void *bufptr, size_t bufsz, struct ibbs_node_reset *s);
size_t s_ibbs_node_recon_d(const void *bufptr, size_t bufsz, struct ibbs_node_recon *s);
size_t s_Language_d(const void *bufptr, size_t bufsz, struct Language *s);
size_t s_Topic_d(const void *bufptr, size_t bufsz, struct Topic *s);
size_t s_NPCInfo_d(const void *bufptr, size_t bufsz, struct NPCInfo *s);
size_t s_NPCNdx_d(const void *bufptr, size_t bufsz, struct NPCNdx *s);
size_t s_Packet_d(const void *bufptr, size_t bufsz, struct Packet *s);
size_t s_SpellsInEffect_d(const void *bufptr, size_t bufsz, struct SpellsInEffect *s);
size_t s_pc_d(const void *bufptr, size_t bufsz, struct pc *s);
size_t s_PClass_d(const void *bufptr, size_t bufsz, struct PClass *s);
size_t s_Spell_d(const void *bufptr, size_t bufsz, struct Spell *s);
size_t s_SpyAttemptPacket_d(const void *bufptr, size_t bufsz, struct SpyAttemptPacket *s);
size_t s_SpyResultPacket_d(const void *bufptr, size_t bufsz, struct SpyResultPacket *s);
size_t s_TradeData_d(const void *bufptr, size_t bufsz, struct TradeData *s);
size_t s_TradeList_d(const void *bufptr, size_t bufsz, struct TradeList *s);
size_t s_UserInfo_d(const void *bufptr, size_t bufsz, struct UserInfo *s);
size_t s_UserScore_d(const void *bufptr, size_t bufsz, struct UserScore *s);
size_t s_village_data_d(const void *bufptr, size_t bufsz, struct village_data *s);
size_t s_LeavingData_d(const void *bufptr, size_t bufsz, struct LeavingData *s);
size_t s_Msg_Txt_d(const void *bufptr, size_t bufsz, struct Msg_Txt *s);
size_t s_Message_d(const void *bufptr, size_t bufsz, struct Message *s);
size_t s_MessageHeader_d(const void *bufptr, size_t bufsz, struct MessageHeader *s);

#endif
