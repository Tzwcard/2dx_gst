#include "waveoutput.h"
#include "soundhelper.h"
#include <fstream>

static int32_t mixing_function(int32_t buf, int16_t input)
{
#if 1
	return buf + input;
#else
	// definitly not working with multi keysounds mixing
	return buf + input - (buf * input / 32768);
#endif
}

// mix input with buffer, treat buffer as 32bit signed samples for further operatings
static int32_t add_wave_to_buffer(uint8_t *output_buffer, uint8_t *wave_buffer, int32_t size_input, int32_t size_buffer, int32_t mix_offset, uint8_t ch)
{
	// treat as 44100Hz, 32bit, stereo, sample offset = (mix_offset * 441 / 10) * sizeof(int32_t) * 2
	int32_t pos_mix = (mix_offset * 441 / 10) * sizeof(int32_t)* 2, pos = 0;
	int32_t *sample_mix = (int32_t*)(output_buffer + pos_mix);
	int16_t *sample_in = (int16_t*)wave_buffer;
	while (pos < size_input)
	{
		// mix both channels
		*sample_mix = mixing_function(*sample_mix, *sample_in);
		*(sample_mix + 1) = mixing_function(*(sample_mix + 1), *(sample_in + (ch == 1 ? 0 : 1)));
		sample_mix += 2; sample_in += (ch == 1 ? 1 : 2);
		pos += 4;
	}
	return (pos_mix + size_input * 2 > size_buffer) ? pos_mix + size_input * 2 : size_buffer;
}

// 32bit signed back to 16bit signed
int32_t normalize_bgm(uint8_t *output_buf, int32_t size_output_buf, int volume_level)
{
	int32_t *sample_mix = (int32_t*)output_buf;
	int16_t *write_sample = (int16_t*)output_buf;

	// get the max sample value
	uint32_t max_sample_val = 0;
	while ((int)sample_mix - (int)output_buf < size_output_buf)
	{
		if (max_sample_val < abs(*sample_mix))
			max_sample_val = abs(*sample_mix);
		sample_mix++;
	}

	// scale all sample based on max value
	sample_mix = (int32_t*)output_buf;
	double ratio = (32768 / (max_sample_val / 1.0));
	if (ratio > 1.0) ratio = 1.0;
	printf("\t\tmixing level %f\n", ratio);
	while ((int)sample_mix - (int)output_buf < size_output_buf)
	{
		*sample_mix = (int32_t)(*sample_mix * ratio);
		if (*sample_mix > 32767)
			*write_sample = 32767;
		else if (*sample_mix < -32768)
			*write_sample = -32768;
		else
			*write_sample = *sample_mix;
		sample_mix++;
		write_sample++;
	}
	return size_output_buf / 2;
}

// only for input as 16bit 44100Hz stereo PCM
#if 0
int32_t mix_bgm(uint8_t *output_buf, int32_t size_output_buf, char *keysound_prefix, int16_t keysound_id, int32_t time)
{
	//	printf("\tadding keysound %08d at %08d\n", keysound_id, time);
	//	printf("\t%08d\t%08d\n", time, keysound_id);
	if (keysound_id == 0)
		return size_output_buf;
	char path_keysound[260];
	sprintf_s(path_keysound, 260, "%s_%08d.wav", keysound_prefix, keysound_id - 1);
	std::fstream keysound;
	keysound.open(path_keysound, std::ios::in | std::ios::binary);
	if (!keysound)
		return size_output_buf;

	keysound.seekg(0, std::ios::end);
	size_t size_keysound = keysound.tellg();
	keysound.seekg(0, std::ios::beg);

	unsigned char *key_buf = new unsigned char[size_keysound];
	keysound.read((char*)key_buf, size_keysound);
	keysound.close();

	uint32_t *pseek = (uint32_t*)key_buf;
	while (*pseek != 0x61746164 // just seek data trunk, i don't want to deal with RIFF header shits
		&& (int)pseek - (int)key_buf < size_keysound - 4)
		pseek = (uint32_t*)((char*)pseek + 1);

	if ((int)pseek - (int)key_buf >= size_keysound - 4)
	{
		delete[]key_buf;
		return size_output_buf;
	}

	pseek++;

	int32_t ret = add_wave_to_buffer(output_buf, (uint8_t*)(pseek + 1), *pseek, size_output_buf, time);
	delete[]key_buf;
	return ret;
}
#else
int32_t mix_bgm(uint8_t *output_buf, int32_t size_output_buf, char *keysound_prefix, int16_t keysound_id, int32_t time)
{
	//	printf("\tadding keysound %08d at %08d\n", keysound_id, time);
	//	printf("\t%08d\t%08d\n", time, keysound_id);
	if (keysound_id == 0)
		return size_output_buf;

	wave_info info;
	query_sound_from_buffer(keysound_id - 1, &info);
	uint8_t ch = query_channels_from_buffer(keysound_id - 1);

	int32_t ret = add_wave_to_buffer(output_buf, (uint8_t*)info.addr, info.size, size_output_buf, time, ch);
	return ret;
}
#endif

int init_riff_header(dummy_wave_header *hdr)
{
	memcpy(hdr->riff, "RIFF", 4);
	memcpy(hdr->wave, "WAVE", 4);
	memcpy(hdr->fmt, "fmt ", 4);
	memcpy(hdr->data, "data", 4);
	hdr->size_fmt_chunk = 16;
	hdr->audio_format = 1;
	hdr->channels = 2;
	hdr->samplerate = 44100;
	hdr->byterate = hdr->samplerate * hdr->channels * sizeof(int16_t);// 176400
	hdr->blockalign = hdr->channels * sizeof(int16_t);// 4
	hdr->bitpersample = 8 * sizeof(int16_t);// 16

	return 0;
}

int set_riff_wave_size(dummy_wave_header *hdr, uint32_t size_data)
{
	hdr->size_data = size_data;
	hdr->size_wave = size_data + 36;
	return 0;
}