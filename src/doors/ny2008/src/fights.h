#include <OpenDoor.h>

void	fight_ops(user_rec *cur_user);
//char	fight(INT16 expert);
void	attack_ops(user_rec *user_on);
void	p_attack_ops(user_rec *user_on, INT16 *nCurrentUserNumber);
void	p_fight_ops(user_rec *cur_user,INT16 *user_num);
//char	p_fight(INT16 expert);
void    online_fight_a(INT16 *user_num, user_rec *user_on, INT16 enm_num);
void    online_fight(INT16 *user_num, user_rec *user_on, INT16 enm_num);
void    fgc(INT16 *enm_num, INT16 *user_num);
INT16	event_gen(user_rec *user_on);
INT16	len_format(char str[]);
void	ny_send_file(const char filename[]);
char	ny_send_menu(menu_t menu,const char allowed[],INT16 onscreen=0);
void	ny_get_index(void);
void	ny_line(INT16 line,INT16 before,INT16 after);
void    any_attack_ops(user_rec *user_on, const char fight_name[], const char en_name[], INT32 en_hitpoints, INT32 en_strength, INT32 en_defense, weapon en_arm);



//char	attack();
//INT16     attack_points();
//INT16	monster_hit();
//INT16	random(); in ny2008.h and .cpp
//void	attack_sequence();
double  what_arm_force(weapon zbran);
double  what_drug_force_a(drug_type droga,INT16 high);
double  what_drug_force_d(drug_type droga,INT16 high);
void	copfight_ops(user_rec *cur_user);
//char	copfight(INT16 expert);
void	copattack_ops(user_rec *user_on);
//char    copattack();
char 	cop_list(void);
void	print_cop(INT16 num);
//char	evil(INT16 expert);
void	evil_ops(user_rec *cur_user);
