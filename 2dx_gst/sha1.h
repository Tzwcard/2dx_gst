#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct sha1_object
{
	uint8_t digest[20];
	uint32_t sz_total;
	uint32_t sz_buf;
	uint8_t buffer[128];
};

int do_sha1_init(sha1_object *digest);
int do_sha1_proc(sha1_object *digest, uint8_t *input, size_t size_input);
int do_sha1_final(sha1_object *digest, uint8_t *output, size_t size_output);
