#include "2dxhelper.h"
#include <fstream>

struct iidx_snd_header
{
	uint8_t comment[16];
	uint32_t pos_data;
	uint32_t count_file;
	uint32_t unk[12];
};

struct snd9_header
{
	char magic[4];
	uint32_t unk1;
	uint32_t size;
	uint32_t unk2;
	uint32_t unk3;
	uint32_t unk4;
};

int read_2dx(char *s3p_path, FARPROC iidxsnd_read_callback)
{
	std::fstream s3pfile;
	s3pfile.open(s3p_path, std::ios::in | std::ios::binary);
	if (!s3pfile)
		return -1;

	s3pfile.seekg(0, std::ios::end);
	size_t file_size = s3pfile.tellg();
	s3pfile.seekg(0, std::ios::beg);

	unsigned char *filebuf = new unsigned char[file_size];
	s3pfile.read((char*)filebuf, file_size);
	s3pfile.clear(); s3pfile.close();

	iidx_snd_header *p_header = (iidx_snd_header*)filebuf;
	uint32_t *p_index = (uint32_t*)(filebuf + sizeof(iidx_snd_header));
	snd9_header *psnd9 = NULL;

	int cnt = 0;
	//  std::fstream s3vfile, logfile;
	//  char output_path[260];

	//  logfile.open("s3p_ext_log.txt", std::ios::out | std::ios::trunc);
	//  char logline[260];
	//	uint32_t *tmp_ptr;

	printf("2DX: Total file count %d\n", p_header->count_file);

	if (iidxsnd_read_callback)
		((int(*)(int id, void *buf, int size))iidxsnd_read_callback)(p_header->count_file, NULL, 0);

	while (cnt < p_header->count_file)
	{
		//	sprintf_s(output_path, 260, "%s_%08d.%s", s3p_path, cnt, (flag == 1) ? "wma" : "s3v");
		//    s3vfile.open(output_path, std::ios::binary | std::ios::out | std::ios::trunc);
		//    printf("file %08d, size %d\n", cnt, p_index->size_file);
		//    if (flag)
		//      s3vfile.write((char*)(filebuf + p_index->pos_file + 0x20), p_index->size_file - 0x20); // wma
		//    else
		//      s3vfile.write((char*)(filebuf + p_index->pos_file), p_index->size_file);

		psnd9 = (snd9_header*)(filebuf + *p_index);

		if (iidxsnd_read_callback)
			((int(*)(int id, void *buf, int size))iidxsnd_read_callback)(cnt, (filebuf + *p_index + sizeof(snd9_header)), _byteswap_ulong(psnd9->size));

		//	tmp_ptr = (uint32_t*)(filebuf + p_index->pos_file + 0x10);
		//	sprintf_s(logline, 260, "%08d: %08x\t%08x\t%08x\t%08x\t%08x\n",
		//		cnt, *(tmp_ptr - 1), *tmp_ptr, *(tmp_ptr + 1), *(tmp_ptr + 2), *(tmp_ptr + 3));
		//	logfile << logline;

		//    s3vfile.close();
		p_index++; cnt++;
	}

	//  logfile.close();

	delete[]filebuf;
	return cnt;
}