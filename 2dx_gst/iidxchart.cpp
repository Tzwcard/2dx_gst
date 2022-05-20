#include "iidxchart.h"
#include "waveoutput.h"
#include "soundhelper.h"
#include "sha1.h"
#include <fstream>

#include <Windows.h>

#include "misc.h"

#define SIZE_OUTPUT_BUF 150 * 1024 * 1024 // 100MB

struct iidx_chart_index
{
	uint32_t pos_chart;
	uint32_t size_chart;
};

struct iidx_chart_header
{
	iidx_chart_index chart_index[12];
};

struct iidx_chart_event
{
	int32_t timecode; // milliseconds?
	int8_t command;
	int8_t val1;
	int16_t val2;
};

enum E_IIDX_CHART_CMD
{
	E_1P_PLAY = 0,
	E_2P_PLAY = 1,
	E_1P_CHANGE_KEY = 2,
	E_2P_CHANGE_KEY = 3,
	E_KEYPLAY = 7
};

enum E_IIDX_CHART_TYPE_ID
{
	E_CHART_SPN = 1,
	E_CHART_SPH = 0,
	E_CHART_SPA = 2,
	E_CHART_SPB = 3,
	E_CHART_DPH = 6,
	E_CHART_DPN = 7,
	E_CHART_DPA = 8
};

static char iidx_chart_name[][8] = 
{
	"SPH", "SPN", "SPA", "SPB", "UNK04", "UNK05", "DPH", "DPN", "DPA", "UNK09", "UNK10", "UNK11"
};

// command 00: note [val1], if [val2] != 0 it's a CN/BSS, notice BSS has a keysound at the end
// command 02: change key [val1] sound to [val2]
// command 07: auto lane with sound [val2]

static int16_t keysounds[8 * 2]; // 8 lanes each side
static int32_t bss_end_pos[2];

int process_chart_file(int music_id, char *sndstr, int volume_level)
{
	char path[260];
	sprintf_s(path, 260, "%05d.1", music_id);
	char *keysound_prefix = NULL;

	SetTitle("2dx_gst");

	printf("Opening chart file %s...\n", path);

	std::fstream iidxchart;
	iidxchart.open(path, std::ios::in | std::ios::binary);
	if (!iidxchart) return -1;
	iidxchart.seekg(0, std::ios::end);
	size_t size_chart = iidxchart.tellg();
	iidxchart.seekg(0, std::ios::beg);

	unsigned char *chartdata = new unsigned char[size_chart];
	iidxchart.read((char*)chartdata, size_chart);
	iidxchart.close();

	iidx_chart_header *p_header = (iidx_chart_header*)chartdata;

	int i = 0;
	iidx_chart_event *p_chart = NULL;

	struct chart_snd_info
	{
		char chart_id;
		char snd_suffix;
	};

	chart_snd_info mixed_chart_snd[12];
	for (i = 0; i < 12; i++)
	{
		mixed_chart_snd[i].chart_id = i;
		mixed_chart_snd[i].snd_suffix = '0';
	}
	mixed_chart_snd[E_CHART_SPN].snd_suffix = sndstr[0];
	mixed_chart_snd[E_CHART_SPH].snd_suffix = sndstr[1];
	mixed_chart_snd[E_CHART_SPA].snd_suffix = sndstr[2];
	mixed_chart_snd[E_CHART_DPN].snd_suffix = sndstr[3];
	mixed_chart_snd[E_CHART_DPH].snd_suffix = sndstr[4];
	mixed_chart_snd[E_CHART_DPA].snd_suffix = sndstr[5];


	std::qsort(mixed_chart_snd, 12, sizeof (chart_snd_info), [](const void* a, const void* b)
	{
		char arg1 = ((chart_snd_info*)a)->snd_suffix;
		char arg2 = ((chart_snd_info*)b)->snd_suffix;

		if (arg1 < arg2) return -1;
		if (arg1 > arg2) return 1;
		return 0;
	});

	printf("Create sound buffer pool...\n");
	snd_buf_init();

	unsigned char *output_buf = new unsigned char[SIZE_OUTPUT_BUF];
	if (!output_buf)
	{
		printf("Failed to allocate output buffer, exiting...\n");
		exit(-1);
	}
	unsigned char sha1_buf[12][20] = { 0 };
	unsigned char is_checked[12] = { 0 };
	sha1_object sha1obj;
	int j = 0, k = 0, total_keysound = 0;
	char last_suffix = 0;
	dummy_wave_header dummyheader;

	init_riff_header(&dummyheader);
	for (k = 0; k < 12; k++)
	{
		i = mixed_chart_snd[k].chart_id;

		if (p_header->chart_index[i].size_chart != 0)
		{
			if (last_suffix != mixed_chart_snd[k].snd_suffix)
			{
				printf("Initialize sound buffer pool for %d:%c...\n", music_id, mixed_chart_snd[k].snd_suffix);
				if (init_sound_buffer(music_id, &mixed_chart_snd[k].snd_suffix) == 0)
				{
					printf("Failed to init sound buffer pool, exiting...\n");
					delete[]chartdata;
					clear_sound_buffer();
					snd_buf_revoke();
					delete[]output_buf;
					return -1;
				}
				printf("Sound buffer pool initialized.\n");
				last_suffix = mixed_chart_snd[k].snd_suffix;
			}

			SetTitle("%s: Processing %s", "chart", iidx_chart_name[i]);

			p_chart = (iidx_chart_event*)(chartdata + p_header->chart_index[i].pos_chart);
			/* create a new output sound buffer */
			int32_t size_output_buf = 0;
			memset(output_buf, 0, SIZE_OUTPUT_BUF);

			memset(keysounds, 0, sizeof(keysounds));
			memset(bss_end_pos, -1, sizeof(bss_end_pos));
			total_keysound = 0;
			while ((int)p_chart - (int)chartdata < p_header->chart_index[i].size_chart + p_header->chart_index[i].pos_chart)
			{
				if (p_chart->timecode == 0x7fffffff) break;
				//				printf("%8d: %02X, %02X, %04X\n", p_chart->timecode, p_chart->command, p_chart->val1, p_chart->val2);

				/*
					check BSS end point
					i don't know if the game will use the same sample for both BSS start and end
					usually it will changed it's keysound during BSS
					but i put it here just in case it's not changed
				*/
				for (j = 0; j < 2; j++)
				{
					if (bss_end_pos[j] != -1 && p_chart->timecode > bss_end_pos[j])
					{
						total_keysound++;
						size_output_buf = mix_bgm(
							output_buf,
							size_output_buf,
							keysound_prefix,
							keysounds[7 + j * 8],
							bss_end_pos[j]
							);
						bss_end_pos[j] = -1;
					}
				}

				switch (p_chart->command)
				{
				case E_1P_PLAY:
				case E_2P_PLAY:
					/* mix keysound of id keysounds[p_chart->val1] */
					size_output_buf = mix_bgm(
						output_buf,
						size_output_buf,
						keysound_prefix,
						keysounds[(p_chart->val1 % 100) + 8 * (p_chart->command & 1)],
						p_chart->timecode
						);
					total_keysound++;
					if (
						// BSS (0x07) or MSS (107 or 0x6B)
						(p_chart->val1 % 100) == 0x07
						&& p_chart->val2 != 0)
					{
						// lane 0x07 CN is BSS
						bss_end_pos[0 + (p_chart->command & 1)] = p_chart->val2 + p_chart->timecode;
					}
					break;
				case E_1P_CHANGE_KEY:
				case E_2P_CHANGE_KEY:
					keysounds[p_chart->val1 + 8 * (p_chart->command & 1)] = p_chart->val2;

					/*
						handle BSS
						check if the new keysound is changed after BSS start and before BSS end
						it might have error if the keysound changes not once while BSS
						(but it shouldn't changed more than once i think)
					*/
					if ((p_chart->val1 % 100) == 0x07
						&& bss_end_pos[0 + (p_chart->command & 1)] != -1
						&& p_chart->timecode <= bss_end_pos[0 + (p_chart->command & 1)]
						)
					{
						total_keysound++;
						size_output_buf = mix_bgm(
							output_buf,
							size_output_buf,
							keysound_prefix,
							keysounds[7 + (p_chart->command & 1) * 8],
							bss_end_pos[0 + (p_chart->command & 1)]
							);
						bss_end_pos[0 + (p_chart->command & 1)] = -1;
					}
					break;
				case E_KEYPLAY:
					/* mix keysound of id p_chart->val2 */
					size_output_buf = mix_bgm(
						output_buf,
						size_output_buf,
						keysound_prefix,
						p_chart->val2,
						p_chart->timecode
						);
					total_keysound++;
					break;
				default:
					break;
				}

				p_chart++;
			}

			size_output_buf = normalize_bgm(output_buf, size_output_buf, volume_level);

			do_sha1_init(&sha1obj);
			do_sha1_proc(&sha1obj, output_buf, size_output_buf);
			do_sha1_final(&sha1obj, sha1_buf[i], 20);

			int is_spawn_file = 1;

			printf("\t%s_%s: %d\n", path, iidx_chart_name[i], total_keysound);

			for (int j = 0; j < 12; j++)
			{
				if (i == j) continue;
				if (is_checked[j] && memcmp(sha1_buf[i], sha1_buf[j], 20) == 0)
				{
					is_spawn_file = 0;
					printf("\tSame as file %s_%s\n", path, iidx_chart_name[j]);
					break;
				}
			}

			set_riff_wave_size(&dummyheader, size_output_buf);

			if (is_spawn_file)
			{
				// write sound buffer to file
				std::fstream output;
				char output_path[260];
				sprintf_s(output_path, 260, "%d_%s.wav", music_id, iidx_chart_name[i]);
				output.open(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
				if (output)
				{
					output.write((char*)&dummyheader, sizeof(dummyheader));
					output.write((char*)output_buf, size_output_buf);
					output.close();
				}
			}
			else
			{
				printf("same output, skipping...\n");
			}
			is_checked[i] = 1;
		}
	}
	delete[]output_buf;

	delete[]chartdata;

	clear_sound_buffer();
	snd_buf_revoke();
	return 0;
}