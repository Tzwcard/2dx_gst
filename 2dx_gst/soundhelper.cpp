#include "soundhelper.h"
#include "s3phelper.h"
#include "2dxhelper.h"
#include "wma_helper.h"
#include "msadpcm.h"

#include <iostream>
#include <fstream>
#include <Shlwapi.h>

#include "misc.h"

#define MAX_WAV_BUF_SIZE 250 * 1024 * 1024 // 250MB of wave size
#define MAX_WAV_COUNT 4096

static uint32_t addr_wavs[MAX_WAV_COUNT] = { 0 };
static int32_t size_wavs[MAX_WAV_COUNT] = { 0 };
static uint8_t channels_wavs[MAX_WAV_COUNT] = { 0 };
static unsigned char *buf_wavs = NULL;
static int total_wavs = 0, curr_pos = 0;

static int wma_to_wav_buf(void *buffer_waves, int current_pos, void *buffer_wma, int size_wma, int max_buf_wav_size, uint8_t *channels)
{
	int size = 0;
	size = wma_to_waveform((unsigned char*)buffer_wma, size_wma, (unsigned char*)buffer_waves + current_pos, channels);
	return size;
}

static int msadpcm_to_wav_buf(void *buffer_waves, int current_pos, void *buffer_wma, int size_wma, int max_buf_wav_size, uint8_t* channels)
{
	int size = 0;
	size = msadpcm_to_waveform((unsigned char*)buffer_wma, size_wma, (unsigned char*)buffer_waves + current_pos, channels);
	return size;
}

enum E_READ_TYPE
{
	ENUM_TYPE_MSADPCM = 1,
	ENUM_TYPE_WMA = 2,
};

int snd_buf_init(void)
{
	if (!buf_wavs)
	{
		buf_wavs = new unsigned char[MAX_WAV_BUF_SIZE];
		if (!buf_wavs)
		{
			printf("Failed to allocate wave buffer, exiting...\n");
			exit(-1);
		}

		// initialize MediaFoundation here
		wma_helper_initialize();
	}
	memset(buf_wavs, 0, MAX_WAV_BUF_SIZE);

	return 0;
}

int snd_buf_revoke(void)
{
	if (!buf_wavs) return 0;

	// uninitialized MediaFoundation here
	wma_helper_uninitialize();
	delete[]buf_wavs;
	buf_wavs = NULL;

	return 1;
}

static int snd_read(int id, void *buffer, int size_buffer, E_READ_TYPE type)
{
	if (buffer == NULL)
	{
		total_wavs = id;
		curr_pos = 0;
		snd_buf_init();
		return 1;
	}

	SetTitle("%s: Loading %d / %d (%.2f%%), memory %dKB / %dKB (%.2f%%)", "snd",
		id, total_wavs, (double)(id * 100.0 / (total_wavs * 1.0)),
		curr_pos / 1024,
		MAX_WAV_BUF_SIZE / 1024,
		(double)((curr_pos * 100.0) / (MAX_WAV_BUF_SIZE * 1.0))
		);

	addr_wavs[id] = (uint32_t)(buf_wavs + curr_pos);
	switch (type)
	{
	case ENUM_TYPE_WMA:
		size_wavs[id] = wma_to_wav_buf(buf_wavs, curr_pos, buffer, size_buffer, MAX_WAV_BUF_SIZE, &channels_wavs[id]);
		break;
	case ENUM_TYPE_MSADPCM:
		size_wavs[id] = msadpcm_to_wav_buf(buf_wavs, curr_pos, buffer, size_buffer, MAX_WAV_BUF_SIZE, &channels_wavs[id]);
		break;
	default:
		break;
	}

	if (size_wavs[id] > 0)
		curr_pos += size_wavs[id];
	//	printf("add: key[%04d]: start %p, size %d\n", id, addr_wavs[id], size_wavs[id]);
	return size_wavs[id];
}

static int wma_read_callback(int id, void *buffer, int size_buffer)
{
	return snd_read(id, buffer, size_buffer, ENUM_TYPE_WMA);
}

static int msadpcm_read_callback(int id, void *buffer, int size_buffer)
{
	return snd_read(id, buffer, size_buffer, ENUM_TYPE_MSADPCM);
}

int query_sound_from_buffer(int id, wave_info *info)
{
	if (id > total_wavs) return -1;
	info->addr = (void*)addr_wavs[id];
	info->size = size_wavs[id];
	//	printf("get: key[%04d]: start %p, size %d\n", id, addr_wavs[id], size_wavs[id]);
	return 1;
}

unsigned char query_channels_from_buffer(int id) {
	return channels_wavs[id];
}

int clear_sound_buffer(void)
{
	total_wavs = 0;
	curr_pos = 0;
	memset(addr_wavs, 0, MAX_WAV_COUNT * sizeof(uint32_t));
	memset(size_wavs, -1, MAX_WAV_COUNT * sizeof(int32_t));

	return 1;
}

int init_sound_buffer(int music_id, char *suffix)
{
	int ret = 0;

	clear_sound_buffer();

	char filename_path[260];

	if (suffix && *suffix != '0')
		sprintf_s(filename_path, "%05d%c.%s", music_id, *suffix, "s3p");
	else
		sprintf_s(filename_path, "%05d.%s", music_id, "s3p");

	if (PathFileExistsA(filename_path))
	{
		printf("Opening file %s...\n", filename_path);
		ret = read_s3p(filename_path, (FARPROC)wma_read_callback);
	}
	else
	{
		if (suffix && *suffix != '0')
			sprintf_s(filename_path, "%05d%c.%s", music_id, *suffix, "2dx");
		else
			sprintf_s(filename_path, "%05d.%s", music_id, "2dx");

		if (PathFileExistsA(filename_path))
		{
			printf("Opening file %s...\n", filename_path);
			ret = read_2dx(filename_path, (FARPROC)msadpcm_read_callback);
		}
	}

	printf("Total sound %d, memory used %d / %d (%.2f%%)\n",
		total_wavs,
		curr_pos,
		MAX_WAV_BUF_SIZE,
		(double)((curr_pos * 100.0) / (MAX_WAV_BUF_SIZE * 1.0))
		);

	return ret;
}