#pragma once
#include <iostream>
typedef struct {
	int position;
	int compressed_data_size;
	int uncompressed_data_size;
	unsigned char compressed_flag;
	char type_of_compressed_data;
	std::string name;
} TOC_ENTRY;