#include "msadpcm.h"
#include <iostream>
#include <stdint.h>
#include <fstream>

#define _USE_MSACM32

#ifdef _USE_MSACM32
#include <Windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "msacm32.lib")
#endif

#include "outdef.h"

struct T_fmt_coeff
{
	int16_t sample_per_blk;
	int16_t cnt_coeff;
	int16_t coeff[255][2];
};

#ifdef _USE_MSACM32
static DWORD decode_msadpcm(const uint8_t* wavedata, std::size_t totalsize, uint8_t *in, size_t size_in, int16_t* outSamples)
{
	/* REF: http://d.hatena.ne.jp/radioactive_radiance/20101027/1288134918 */
	DWORD ret = 0;
	size_t data_size = 0;

	ADPCMWAVEFORMAT *fmt = (ADPCMWAVEFORMAT *)wavedata;

	PCMWAVEFORMAT decode_fmt;

	decode_fmt.wf.wFormatTag = WAVE_FORMAT_PCM;
	decode_fmt.wf.nChannels = __OUTPUT_WAVEFORM_CHANNELS;
	decode_fmt.wf.nSamplesPerSec = __OUTPUT_WAVEFORM_SAMPLE_PER_SECOND;
	decode_fmt.wf.nAvgBytesPerSec = decode_fmt.wf.nSamplesPerSec * decode_fmt.wf.nChannels * sizeof(__OUTPUT_WAVEFORM_TYPE);
	decode_fmt.wf.nBlockAlign = decode_fmt.wf.nChannels * sizeof(__OUTPUT_WAVEFORM_TYPE);
	decode_fmt.wBitsPerSample = sizeof(__OUTPUT_WAVEFORM_TYPE)* 8;

	HRESULT hr;

	HACMSTREAM hACM;
	ACMSTREAMHEADER acmStreamHeader;

	hr = acmFormatSuggest(0, (LPWAVEFORMATEX)wavedata, (LPWAVEFORMATEX)&decode_fmt, sizeof(ADPCMWAVEFORMAT), ACM_FORMATSUGGESTF_WFORMATTAG);
	hr = acmStreamOpen(&hACM, 0, (LPWAVEFORMATEX)wavedata, (LPWAVEFORMATEX)&decode_fmt, NULL, NULL, NULL, ACM_STREAMOPENF_NONREALTIME);
	hr = acmStreamSize(hACM, size_in, &ret, ACM_STREAMSIZEF_SOURCE);

	ZeroMemory(&acmStreamHeader, sizeof(ACMSTREAMHEADER));
	acmStreamHeader.cbStruct = sizeof(ACMSTREAMHEADER);
	acmStreamHeader.cbSrcLength = size_in;
	acmStreamHeader.pbSrc = in;
	acmStreamHeader.cbDstLength = ret;
	acmStreamHeader.pbDst = (uint8_t*)outSamples;

	hr = acmStreamPrepareHeader(hACM, &acmStreamHeader, 0);

	hr = acmStreamConvert(hACM, &acmStreamHeader, ACM_STREAMCONVERTF_BLOCKALIGN);
	hr = acmStreamUnprepareHeader(hACM, &acmStreamHeader, 0);
	hr = acmStreamClose(hACM, 0);

	//	printf("Total Decoded %d bytes.\n", curr_out);

	return ret;
}
#else
static const int AdaptationTable[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	768, 614, 512, 409, 307, 230, 230, 230
};
// These are the 'built in' set of 7 predictor value pairs; additional values can be added to this table by including them as metadata chunks in the WAVE header
static const int AdaptCoeff1[] = { 256, 512, 0, 192, 240, 460, 392 };
static const int AdaptCoeff2[] = { 0, -256, 0, 64, 0, -208, -232 };

#pragma pack(push, 1)
struct ADPCMBlockHeader_Mono
{
	uint8_t predictor;
	int16_t intialDelta;
	int16_t sample1;
	int16_t sample2;
};

struct ADPCMBlockHeader_Stereo
{
	uint8_t predictorL;
	uint8_t predictorR;
	int16_t intialDeltaL;
	int16_t intialDeltaR;
	int16_t sample1L;
	int16_t sample1R;
	int16_t sample2L;
	int16_t sample2R;
};
#pragma pack(pop)

static const int8_t signed_code_table[] = { 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1 };
static int decodeBlock(const uint8_t* blockData, std::size_t blockSize, int16_t* outSamples, int channels = 2, T_fmt_coeff *coeff_data = NULL)
{
	std::size_t offset = 0;
	std::size_t sample_cnt = 0;

	enum E_CHANNEL
	{
		E_CHANNEL_MONO = 0,
		E_CHANNEL_LEFT = 0,
		E_CHANNEL_RIGHT = 1
	};

	ADPCMBlockHeader_Mono *block_mono = (ADPCMBlockHeader_Mono*)blockData;
	ADPCMBlockHeader_Stereo *block_stereo = (ADPCMBlockHeader_Stereo*)blockData;

	int16_t delta[2] = { 0 };
	int16_t sample1[2] = { 0 }, sample2[2] = { 0 };
	int32_t coeff1[2] = { 0 }, coeff2[2] = { 0 }, i = 0;

	switch (channels)
	{
	case 1:
	{
			  delta[E_CHANNEL_MONO] = block_mono->intialDelta;

			  sample1[E_CHANNEL_MONO] = block_mono->sample1;
			  sample2[E_CHANNEL_MONO] = block_mono->sample2;

			  if (coeff_data)
			  {
				  coeff1[E_CHANNEL_MONO] = coeff_data->coeff[block_mono->predictor][0];
				  coeff2[E_CHANNEL_MONO] = coeff_data->coeff[block_mono->predictor][1];
			  }
			  else
			  {
				  coeff1[E_CHANNEL_MONO] = AdaptCoeff1[block_mono->predictor];
				  coeff2[E_CHANNEL_MONO] = AdaptCoeff2[block_mono->predictor];
			  }

			  offset += sizeof(ADPCMBlockHeader_Mono);

			  outSamples[sample_cnt++] = sample2[E_CHANNEL_MONO];
			  outSamples[sample_cnt++] = sample1[E_CHANNEL_MONO];
			  break;
	}
	case 2:
	{
			  delta[E_CHANNEL_LEFT] = block_stereo->intialDeltaL; delta[E_CHANNEL_RIGHT] = block_stereo->intialDeltaR;

			  sample1[E_CHANNEL_LEFT] = block_stereo->sample1L; sample1[E_CHANNEL_RIGHT] = block_stereo->sample1R;
			  sample2[E_CHANNEL_LEFT] = block_stereo->sample2L; sample2[E_CHANNEL_RIGHT] = block_stereo->sample2R;

			  if (coeff_data)
			  {
				  coeff1[E_CHANNEL_LEFT] = coeff_data->coeff[block_stereo->predictorL][0]; coeff1[E_CHANNEL_RIGHT] = coeff_data->coeff[block_stereo->predictorR][0];
				  coeff2[E_CHANNEL_LEFT] = coeff_data->coeff[block_stereo->predictorL][1]; coeff2[E_CHANNEL_RIGHT] = coeff_data->coeff[block_stereo->predictorR][1];
			  }
			  else
			  {
				  coeff1[E_CHANNEL_LEFT] = AdaptCoeff1[block_stereo->predictorL]; coeff1[E_CHANNEL_RIGHT] = AdaptCoeff1[block_stereo->predictorR];
				  coeff2[E_CHANNEL_LEFT] = AdaptCoeff2[block_stereo->predictorL]; coeff2[E_CHANNEL_RIGHT] = AdaptCoeff2[block_stereo->predictorR];
			  }
			  offset += sizeof(ADPCMBlockHeader_Stereo);

			  outSamples[sample_cnt++] = sample2[E_CHANNEL_LEFT]; outSamples[sample_cnt++] = sample2[E_CHANNEL_RIGHT];
			  outSamples[sample_cnt++] = sample1[E_CHANNEL_LEFT]; outSamples[sample_cnt++] = sample1[E_CHANNEL_RIGHT];
			  break;
	}
	default:
		return 0;
	}

//	printf("coeffL %d : %d, coeffR %d : %d\n", coeff1[0], coeff2[0], coeff1[1], coeff2[1]);

	blockSize = coeff_data->sample_per_blk - 2 + offset;

	int8_t nibble_val[2];
	while (offset < blockSize)
	{
		nibble_val[0] = (blockData[offset] >> 4) & 0x0F;
		nibble_val[1] = blockData[offset] & 0x0F;

		for (i = 0; i < 2; i++)
		{
			int8_t channel = (channels > 1) ? i % channels : 0;
			int8_t signed_nibble = signed_code_table[nibble_val[i] & 0xf];

			int32_t predictor = ((sample1[channel] * coeff1[channel]) + (sample2[channel] * coeff2[channel])) >> 8,
				current = predictor + (signed_nibble * delta[channel]);
			
			if (current > INT16_MAX) current = INT16_MAX;
			else if (current < INT16_MIN) current = INT16_MIN;

			outSamples[sample_cnt++] = current;
			if (channels == 1)
				outSamples[sample_cnt++] = current; // copy to right channel if mono

			delta[channel] = (AdaptationTable[nibble_val[i]] * (int32_t)delta[channel]) >> 8;
			if (delta[channel] < 16) delta[channel] = 16;
//			delta[channel] = delta[channel] < 16 ? 16 : delta[channel];

			sample2[channel] = sample1[channel];
			sample1[channel] = current;
//			printf("delta[%d] %d, nibble %d(%d), sample %d:%d\n", channel, delta[channel], signed_nibble, nibble_val[channel], sample1[channel], sample2[channel]);
		}

		offset ++;
	}

	return sample_cnt * sizeof(int16_t);
}

static int decode_msadpcm(const uint8_t* blockData, std::size_t totalsize, ::size_t block_align, int16_t* outSamples, int channels = 2, T_fmt_coeff *coeff_data = NULL)
{
	// decodeBlock
	int curr = 0, curr_out = 0;

	while (curr < totalsize)
	{
		curr_out += decodeBlock(blockData + curr, block_align, (int16_t*)((char*)outSamples + curr_out), channels, coeff_data);
		curr += block_align;
	}

//	printf("Total Decoded %d bytes.\n", curr_out);

	return 1;
}
#endif

static const char _data_string[] = "data", 
_fmt_string[] = "fmt ",
_fact_string[] = "fact";
int msadpcm_to_waveform(unsigned char *adpcm_data, int size_adpcm_data, unsigned char *wave_data)
{
	int wave_size = 0, pos = 0, fmt_pos = 0;

	// fmt chunk
	while (pos < size_adpcm_data)
	{
		if (memcmp(_fmt_string, adpcm_data + pos, 4) == 0)
			break;
		pos++;
	}
	if (pos == size_adpcm_data) return 0;
	fmt_pos = pos + 4 + 4;

	int16_t format = *(int16_t*)(adpcm_data + pos + 4 + 4),
		channels = *(int16_t*)(adpcm_data + pos + 4 + 4 + 2),
		block_align = *(int16_t*)(adpcm_data + pos + 4 + 4 + 0xc);
	T_fmt_coeff *coeff_data = NULL;
	if (*(int16_t*)(adpcm_data + pos + 4 + 4 + 16) >= 0x20)
		coeff_data = (T_fmt_coeff*)(adpcm_data + pos + 4 + 4 + 16 + 2);

//	printf("format %04X, channels %04X\n", format, channels);

	if (format != 0x0002) // MS-ADPCM
		return 0;

	// fact trunk
	pos = 0;
	uint32_t wave_size_fact = 0;
	while (pos < size_adpcm_data)
	{
		if (memcmp(_fact_string, adpcm_data + pos, 4) == 0)
			break;
		pos++;
	}
	if (pos != size_adpcm_data)
		wave_size_fact = *(uint32_t*)(adpcm_data + pos + 4 + 4) * sizeof(int16_t)* 2; // * channels;
	else
		return 0; // NON-LINEAR-PCM MUST HAVE FACT

//	printf("wave_size_fact %d\n", wave_size_fact);

	// data trunk
	pos = 0;
	while (pos < size_adpcm_data)
	{
		if (memcmp(_data_string, adpcm_data + pos, 4) == 0)
			break;
		pos++;
	}
	if (pos == size_adpcm_data) return 0;

	uint32_t size = *(uint32_t*)(adpcm_data + pos + 4);
	unsigned char *adpcm_rawdata = adpcm_data + pos + 4 + sizeof(uint32_t);

//	printf("msadpcm_to_waveform: data chunk at %p, size %d\n", adpcm_rawdata, size);

//	wave_size = decodeBlock(adpcm_rawdata, size, block_align, (int16_t*)wave_data, channels, coeff_data);
#ifdef _USE_MSACM32
	wave_size = decode_msadpcm(adpcm_data + fmt_pos,
		size_adpcm_data - fmt_pos,
		adpcm_rawdata,
		size,
		(int16_t*)wave_data
		);
	wave_size = wave_size_fact ? wave_size_fact : wave_size;
#else
	wave_size = decode_msadpcm(adpcm_rawdata, size, block_align, (int16_t*)wave_data, channels, coeff_data);
//	printf("Write wave_size_fact %d\n", wave_size_fact);
	wave_size = wave_size_fact ? wave_size_fact : wave_size;

//	printf("msadpcm_to_waveform: sz_data %d, expect %d\n", wave_size, calculateNumSamplesInBlock(size) * sizeof(int16_t));
#endif
	return wave_size;
}