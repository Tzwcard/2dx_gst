#include <iostream>
#include "defs.h"
#include "iidxchart.h"

int main(int argc, char *argv[])
{
	printf("2dx_gst build at " __BUILD_DATE_TIME__ " (ver " __VERSION__2DX__GST__ ")\n");

	if (argc < 2)
	{
		char *exe = NULL;
		exe = strrchr(argv[0], '\\');
		if (!exe)
			exe = argv[0];
		else
			exe++;

		printf("Usage: %s [music_id] <args...>\n\n", exe);
		printf("       Please place it in the same folder\n");
		printf("       as .1 and sound file.\n\n");
		printf("Args:\n");
		printf("       --snd-string: define sound surfix\n");
		printf("                     ex: 0ha0ha00\n\n");
		printf("       [2DX] Supported.\n");
		printf("       [S3P] Supported, but will generate temp file.\n");
		return -1;
	}

	char snd_string[9] = "00000000";
	if (argc > 2)
	{
		int pos = 2;
		while (pos < argc)
		{
			if (strcmp(argv[pos], "--snd-string") == 0
				&& pos + 1 < argc)
			{
				strcpy_s(snd_string, 9, argv[pos + 1]);
				pos += 2;
			}

			pos++;
		}
	}
	return process_chart_file(atoi(argv[1]), snd_string, 100);//atoi(argv[3]));
}