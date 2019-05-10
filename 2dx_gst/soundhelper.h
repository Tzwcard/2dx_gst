#pragma once
#include <stdint.h>

struct wave_info
{
	uint32_t size;
	void *addr;
};

int clear_sound_buffer(void);
int init_sound_buffer(int music_id, char *suffix = 0);
int query_sound_from_buffer(int id, wave_info *info);
int snd_buf_init(void);
int snd_buf_revoke(void);