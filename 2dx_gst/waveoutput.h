#pragma once

#include <stdint.h>

struct dummy_wave_header
{
	char riff[4]; // 'RIFF'
	uint32_t size_wave;
	char wave[4]; // 'WAVE'
	char fmt[4]; // 'fmt '
	uint32_t size_fmt_chunk; // 0x10
	uint16_t audio_format; // 1 = PCM
	uint16_t channels; // 2 = stereo
	uint32_t samplerate; // 44100;
	uint32_t byterate; // 176400
	uint16_t blockalign; // 4
	uint16_t bitpersample; // 0x10(16)
	char data[4]; // 'data'
	uint32_t size_data;
};

int32_t normalize_bgm(uint8_t *output_buf, int32_t size_output_buf, int volume_level);
int32_t mix_bgm(uint8_t *output_buf, int32_t size_output_buf, char *keysound_prefix, int16_t keysound_id, int32_t time);
int init_riff_header(dummy_wave_header *hdr);
int set_riff_wave_size(dummy_wave_header *hdr, uint32_t size_data);