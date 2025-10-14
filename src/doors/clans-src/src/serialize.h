#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "interbbs.h"
#include "myopen.h"
#include "quests.h"
#include "structs.h"

size_t s_Alliance_s(struct Alliance *s, void *bufptr, size_t bufsz);
size_t s_Army_s(struct Army *s, void *bufptr, size_t bufsz);
size_t s_Strategy_s(struct Strategy *s, void *bufptr, size_t bufsz);
size_t s_clan_s(struct clan *s, void *bufptr, size_t bufsz);
size_t s_empire_s(struct empire *s, void *bufptr, size_t bufsz);
size_t s_item_data_s(struct item_data *s, void *bufptr, size_t bufsz);
size_t s_AttackPacket_s(struct AttackPacket *s, void *bufptr, size_t bufsz);
size_t s_AttackResult_s(struct AttackResult *s, void *bufptr, size_t bufsz);
size_t s_EventHeader_s(struct EventHeader *s, void *bufptr, size_t bufsz);
size_t s_FileHeader_s(struct FileHeader *s, void *bufptr, size_t bufsz);
size_t s_game_data_s(struct game_data *s, void *bufptr, size_t bufsz);
size_t s_ibbs_node_attack_s(struct ibbs_node_attack *s, void *bufptr, size_t bufsz);
size_t s_ibbs_node_reset_s(struct ibbs_node_reset *s, void *bufptr, size_t bufsz);
size_t s_ibbs_node_recon_s(struct ibbs_node_recon *s, void *bufptr, size_t bufsz);
size_t s_Language_s(struct Language *s, void *bufptr, size_t bufsz);
size_t s_Topic_s(struct Topic *s, void *bufptr, size_t bufsz);
size_t s_NPCInfo_s(struct NPCInfo *s, void *bufptr, size_t bufsz);
size_t s_NPCNdx_s(struct NPCNdx *s, void *bufptr, size_t bufsz);
size_t s_Packet_s(struct Packet *s, void *bufptr, size_t bufsz);
size_t s_SpellsInEffect_s(struct SpellsInEffect *s, void *bufptr, size_t bufsz);
size_t s_pc_s(struct pc *s, void *bufptr, size_t bufsz);
size_t s_PClass_s(struct PClass *s, void *bufptr, size_t bufsz);
size_t s_Spell_s(struct Spell *s, void *bufptr, size_t bufsz);
size_t s_SpyAttemptPacket_s(struct SpyAttemptPacket *s, void *bufptr, size_t bufsz);
size_t s_SpyResultPacket_s(struct SpyResultPacket *s, void *bufptr, size_t bufsz);
size_t s_TradeData_s(struct TradeData *s, void *bufptr, size_t bufsz);
size_t s_TradeList_s(struct TradeList *s, void *bufptr, size_t bufsz);
size_t s_UserInfo_s(struct UserInfo *s, void *bufptr, size_t bufsz);
size_t s_UserScore_s(struct UserScore *s, void *bufptr, size_t bufsz);
size_t s_village_data_s(struct village_data *s, void *bufptr, size_t bufsz);
size_t s_LeavingData_s(struct LeavingData *s, void *bufptr, size_t bufsz);
size_t s_Msg_Txt_s(struct Msg_Txt *s, void *bufptr, size_t bufsz);
size_t s_Message_s(struct Message *s, void *bufptr, size_t bufsz);
size_t s_MessageHeader_s(struct MessageHeader *s, void *bufptr, size_t bufsz);

#endif
