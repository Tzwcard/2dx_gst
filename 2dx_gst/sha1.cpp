#include "sha1.h"

static void proc_message_sha1(uint32_t *h, uint32_t *w)
{
	int i = 0;
	uint32_t a, b, c, d, e, f, k, temp;

	for (i = 16; i <= 79; i++)
	{
		w[i] = (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
		w[i] = (w[i] << 1) | (w[i] >> 31);
	}

	a = h[0];
	b = h[1];
	c = h[2];
	d = h[3];
	e = h[4];

	for (i = 0; i <= 79; i++)
	{
		if (0 <= i && i <= 19)
		{
			f = (b & c) | ((~b) & d);
			k = 0x5A827999;
		}
		else if (20 <= i && i <= 39)
		{
			f = b ^ c ^ d;
			k = 0x6ED9EBA1;
		}
		else if (40 <= i && i <= 59)
		{
			f = (b & c) | (b & d) | (c & d);
			k = 0x8F1BBCDC;
		}
		else if (60 <= i && i <= 79)
		{
			f = b ^ c ^ d;
			k = 0xCA62C1D6;
		}

		temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
		e = d;
		d = c;
		c = (b << 30) | (b >> 2);
		b = a;
		a = temp;
	}

	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
	h[4] += e;

	memset(w, 0, 80 * sizeof(uint32_t));
}

int do_sha1_init(sha1_object *digest)
{
	uint32_t *h = (uint32_t*)(&digest->digest[0]);

	h[0] = 0x67452301; h[1] = 0xEFCDAB89;
	h[2] = 0x98BADCFE; h[3] = 0x10325476;
	h[4] = 0xC3D2E1F0;
	digest->sz_buf = 0; digest->sz_total = 0;

	return 0;
}

int do_sha1_proc(sha1_object *digest, uint8_t *input, size_t size_input)
{
	uint32_t *h = (uint32_t*)(&digest->digest[0]);

	uint8_t buffer[80 * sizeof(uint32_t)], *in_ptr = input;
	uint32_t *w = (uint32_t*)buffer;
	int64_t ml = size_input * 8;
	int i = 0, ctr = 0;

	if (digest->sz_buf == 0)
	{
		while (size_input >= 64)
		{
			for (i = 0; i < 16; i++)
				w[i] = _byteswap_ulong(*(uint32_t*)(in_ptr + i * 4));

			size_input -= 64; digest->sz_total += 64; in_ptr += 64;

			proc_message_sha1(h, w);
		}
	}
	else
	{
		if (digest->sz_buf >= 64)
		{
			for (i = 0; i < 16; i++)
				w[i] = _byteswap_ulong(*(uint32_t*)(digest->buffer + i * 4));

			digest->sz_total += 64;
			digest->sz_buf -= 64;
			memcpy(digest->buffer, digest->buffer + 64, digest->sz_buf);

			proc_message_sha1(h, w);
		}

		if (digest->sz_buf + size_input >= 64)
		{
			if (digest->sz_buf != 0)
			{
				memcpy(digest->buffer + digest->sz_buf, in_ptr, 64 - digest->sz_buf);
				for (i = 0; i < 16; i++)
					w[i] = _byteswap_ulong(*(uint32_t*)(digest->buffer + i * 4));
				size_input -= (64 - digest->sz_buf);
				in_ptr += (64 - digest->sz_buf);
				digest->sz_total += 64;
				digest->sz_buf = 0;

				proc_message_sha1(h, w);
			}

			while (size_input >= 64)
			{
				for (i = 0; i < 16; i++)
					w[i] = _byteswap_ulong(*(uint32_t*)(in_ptr + i * 4));

				size_input -= 64; digest->sz_total += 64; in_ptr += 64;

				proc_message_sha1(h, w);
			}
		}
	}
	if (size_input)
	{
		memcpy(digest->buffer + digest->sz_buf, in_ptr, size_input);
		digest->sz_buf += size_input;
	}

	return 0;
}

int do_sha1_final(sha1_object *digest, uint8_t *output, size_t size_output)
{
	uint32_t *h = (uint32_t*)(&digest->digest[0]);
	uint8_t buffer[80 * sizeof(uint32_t)];
	uint32_t *w = (uint32_t*)buffer;
	int64_t ml = (digest->sz_total + digest->sz_buf) * 8;

	int i = 0;
	int iq = 0;

	while (digest->sz_buf >= 64)
	{
		for (i = 0; i < 16; i++)
			w[i] = _byteswap_ulong(*(uint32_t*)(digest->buffer + i * 4));

		memcpy(digest->buffer, digest->buffer + 64, digest->sz_buf - 64);
		digest->sz_buf -= 64; digest->sz_total += 64;

		proc_message_sha1(h, w);
	}

	if (digest->sz_buf <= 55)
	{
		memset(buffer, 0, 80 * 4);
		memcpy(buffer, digest->buffer, digest->sz_buf);
		buffer[digest->sz_buf] = 0x80;
		for (i = 0; i < 14; i++)
			w[i] = _byteswap_ulong(w[i]);
		w[14] = (ml >> 32) & 0xffffffff;
		w[15] = (ml & 0xffffffff);

		proc_message_sha1(h, w);
	}
	else
	{
		// block 1
		memset(buffer, 0, 80 * 4);
		memcpy(buffer, digest->buffer, digest->sz_buf);
		buffer[digest->sz_buf] = 0x80;
		for (i = 0; i < 16; i++)
			w[i] = _byteswap_ulong(w[i]);
		proc_message_sha1(h, w);

		// block 2
		memset(buffer, 0, 80 * 4);
		w[14] = (ml >> 32) & 0xffffffff;
		w[15] = ml & 0xffffffff;

		proc_message_sha1(h, w);
	}

	for (i = 0; i < 5; i++)
		h[i] = _byteswap_ulong(h[i]);
	memcpy(output, h, 20);

	return 0;
}