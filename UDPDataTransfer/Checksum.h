#pragma once
#include "common.h"

class Checksum
{
private:
	uint32_t *crc_table;
public:
	Checksum();
	DWORD CRC32(unsigned char *buf, size_t len);
	~Checksum();
};

