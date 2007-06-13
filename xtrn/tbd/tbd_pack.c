#include "tbd_pack.h"

int pack_user_t_struct(char *buf, user_t *data)
{
	char	*p;
	int		i;

	p = buf;
	*(short *)p = LE_SHORT(data->status);
	p += sizeof(short);
	for(i = 0; i < 26; i++) {
		*p = data->handle[i];
		p++;
	}
	*(short *)p = LE_SHORT(data->mapx);
	p += sizeof(short);
	*(short *)p = LE_SHORT(data->mapy);
	p += sizeof(short);
	*(short *)p = LE_SHORT(data->mapz);
	p += sizeof(short);
	*(short *)p = LE_SHORT(data->roomx);
	p += sizeof(short);
	*(short *)p = LE_SHORT(data->roomy);
	p += sizeof(short);
	*(short *)p = LE_SHORT(data->health);
	p += sizeof(short);
	*(short *)p = LE_SHORT(data->max_health);
	p += sizeof(short);
	*(int *)p = LE_LONG(data->exp);
	p += sizeof(int);
	*(uchar *)p = data->level;
	p++;
	*(unsigned short *)p = LE_SHORT(data->weapon);
	p += sizeof(unsigned short);
	*(unsigned short *)p = LE_SHORT(data->armor);
	p += sizeof(unsigned short);
	*(unsigned short *)p = LE_SHORT(data->gold);
	p += sizeof(unsigned short);
	for(i = 0; i < 6; i++) {
		*(unsigned short *)p = LE_SHORT(data->item[i]);
		p += sizeof(unsigned short);
	}
	*(int *)p = LE_LONG(data->left_game);
	p += sizeof(int);
	*(uchar *)p = data->ring;
	p++;
	*(uchar *)p = data->key;
	p++;
	*(uchar *)p = data->compass;
	p++;
	for(i = 0; i < 28; i++) {
		*p = data->space[i];
		p++;
	}

	return((int)(p-buf));
}

int unpack_user_t_struct(user_t *data, char *buf)
{
	char	*p;
	int		i;

	p = buf;
	data->status = LE_SHORT(*(short *)p);
	p += sizeof(short);
	for(i = 0; i < 26; i++) {
		data->handle[i] = *p;
		p++;
	}
	data->mapx = LE_SHORT(*(short *)p);
	p += sizeof(short);
	data->mapy = LE_SHORT(*(short *)p);
	p += sizeof(short);
	data->mapz = LE_SHORT(*(short *)p);
	p += sizeof(short);
	data->roomx = LE_SHORT(*(short *)p);
	p += sizeof(short);
	data->roomy = LE_SHORT(*(short *)p);
	p += sizeof(short);
	data->health = LE_SHORT(*(short *)p);
	p += sizeof(short);
	data->max_health = LE_SHORT(*(short *)p);
	p += sizeof(short);
	data->exp = LE_LONG(*(int *)p);
	p += sizeof(int);
	data->level = *(uchar *)p;
	p++;
	data->weapon = LE_SHORT(*(unsigned short *)p);
	p += sizeof(unsigned short);
	data->armor = LE_SHORT(*(unsigned short *)p);
	p += sizeof(unsigned short);
	data->gold = LE_SHORT(*(unsigned short *)p);
	p += sizeof(unsigned short);
	for(i = 0; i < 6; i++) {
		data->item[i] = LE_SHORT(*(unsigned short *)p);
		p += sizeof(unsigned short);
	}
	data->left_game = LE_LONG(*(int *)p);
	p += sizeof(int);
	data->ring = *(uchar *)p;
	p++;
	data->key = *(uchar *)p;
	p++;
	data->compass = *(uchar *)p;
	p++;
	for(i = 0; i < 28; i++) {
		data->space[i] = *p;
		p++;
	}

	return((int)(p-buf));
}
