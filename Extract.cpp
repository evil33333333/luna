#include "Extract.h"
#include <iostream>
#include <ios>
#include <vector>
#include <sstream>
#include <windows.h>

Extract::Extract() {}

void Extract::DecompilePythonExe(std::string filename) {
	this->python_exe->open(filename, std::ios::binary);
	this->GetPythonExeFileSize();
	this->SearchForExeVersion();
	this->GetPythonArchiveInfo();
	this->ParseTableOfContents();
	this->GetSourceFile();
	this->ExtractBaseLibrary();
	this->WriteCompiledPythonFile();
}

void Extract::GetPythonExeFileSize() {
	this->python_exe->seekg(0, std::ios::end);
	this->file_size = this->python_exe->tellg();
	this->python_exe->seekg(0, std::ios::beg);
}

void Extract::SearchForExeVersion() {
	int search_chunk_size = 8192;
	int end_position = this->file_size;
	this->cookie_position = -1;
	if (end_position < strlen(MAGIC)) {
		return;
	}
	while (true) {
		int start_position;
		if (end_position >= search_chunk_size) {
			start_position = end_position - search_chunk_size;
		}
		else {
			start_position = 0;
		}
		int chunk_size = end_position - start_position;
		if (chunk_size < strlen(MAGIC)) {
			break;
		}
		this->python_exe->seekg(start_position, std::ios_base::beg);
		std::vector<char> buffer;
		buffer.reserve(chunk_size);
		this->python_exe->read(&buffer[0], chunk_size);
		size_t offset;
		if (offset = std::string(buffer.data()).find(MAGIC) == std::string::npos) {
			this->cookie_position = start_position + offset;
			break;
		}
		end_position = start_position + strlen(MAGIC) - 1;
		if (start_position == 0) {
			break;
		}
	}
	if (this->cookie_position == -1) {
		std::cerr << "Cookie position was not found" << std::endl;
		ExitProcess(EXIT_FAILURE);
	}
	this->python_exe->seekg(static_cast<std::basic_istream<char, std::char_traits<char>>::off_type>(this->cookie_position) + PYINST20_COOKIE_SIZE, std::ios_base::beg);
	std::vector<char> buffer;
	buffer.reserve(64);
	this->python_exe->read(&buffer[0], 64);
	if (std::string(buffer.data()).find("python") != std::string::npos) {
		this->python_exe_version = 21;
	}
	else {
		this->python_exe_version = 20;
	}
}

void Extract::GetPythonArchiveInfo() {
	std::string magic_key, library_name;
	int length_of_pkg, toc, toc_length, python_version, tail_bytes;

	this->python_exe->seekg(this->cookie_position, std::ios::beg);
	std::stringstream stream{};
	stream << this->python_exe->rdbuf();
	std::string filebuf = stream.str().substr(0, this->python_exe_version == 20 ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE);
	magic_key = filebuf.substr(0, 7);
	length_of_pkg = std::stoi(filebuf.substr(8, 11));
	toc = std::stoi(filebuf.substr(12, 15));
	toc_length = std::stoi(filebuf.substr(16, 19));
	this->python_version = std::stoi(filebuf.substr(20, 23));
	library_name = this->python_exe_version == 20 ? std::string{} : filebuf.substr(24, 87);

	tail_bytes = this->file_size - this->cookie_position - (this->python_exe_version == 20 ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE);
	this->overlay_size = length_of_pkg + tail_bytes;
	this->overlay_position = this->file_size - this->overlay_size;
	this->table_of_contents_position = this->overlay_position + toc;
	this->table_of_contents_size - toc_length;

}

void Extract::ParseTableOfContents() {
	this->python_exe->seekg(this->table_of_contents_position, std::ios::beg);
	int parsed_code_count = 0;
	while (parsed_code_count < this->table_of_contents_size) {
		int entry_size = 0;
		int entry_position{}, compressed_size{}, uncompressed_size{};
		unsigned char compress_flag{};
		char type_of_compress_flag{};
		this->python_exe->read((char*)entry_size, 4);
		// struct.calcsize(!iiiiBc) 4*4 + 1 + 1
		// 4 ints and 2 chars [signed and unsigned] makes 18 bytes total [assuming we are on x64]
		int namelen = 18;
		std::string name;

		std::vector<char> filebuf;
		filebuf.reserve(entry_size - 4);

		// i need to clean this bit up
		this->python_exe->read(filebuf.data(), entry_size - 4);

		// memcpy bytes into variables 
		for (auto it = filebuf.begin(); it != filebuf.begin()+3; ++it) {
			memcpy((void*)entry_position, (const void*)*it, sizeof(char));
		}

		for (auto it = filebuf.begin() + 4; it != filebuf.begin() + 7; ++it) {
			memcpy((void*)compressed_size, (const void*)*it, sizeof(char));
		}

		for (auto it = filebuf.begin() + 8; it != filebuf.begin() + 11; ++it) {
			memcpy((void*)uncompressed_size, (const void*)*it, sizeof(char));
		}

		for (auto it = filebuf.begin() + 12; it != filebuf.begin() + 13; ++it) {
			memcpy((void*)compress_flag, (const void*)*it, sizeof(char));
		}

		for (auto it = filebuf.begin() + 14; it != filebuf.begin() + 15; ++it) {
			memcpy((void*)type_of_compress_flag, (const void*)*it, sizeof(char));
		}

		// get the name
		for (auto it = filebuf.begin() + 16; it != filebuf.begin() + namelen; ++it) {
			name += *it;
		}
		this->toc_list->push_back(TOC_ENTRY{this->overlay_position + entry_position, compressed_size, uncompressed_size, compress_flag, type_of_compress_flag, name});
		parsed_code_count += entry_size;
	}
}

void Extract::GetSourceFile() {
	for (auto it = this->toc_list->begin(); it != this->toc_list->end(); ++it) {
		std::vector<char> data_buf;
		data_buf.reserve(it->compressed_data_size);
		this->python_exe->seekg(it->position, std::ios::beg);
		this->python_exe->read(data_buf.data(), it->compressed_data_size);
		if (it->type_of_compressed_data == 0x73) {
			if (it->name.substr(0, 2).find("py") == std::string::npos && it->name.find("struct") == std::string::npos) {
				for (auto it = this->magic_number.begin(); it != this->magic_number.end(); ++it) {
					this->source_file->push_back(*it);
				}
				for (int i = 0; i < 12; i++) {
					this->source_file->push_back(0x00);
				}
				for (auto it = data_buf.begin(); it != data_buf.end(); ++it) {
					this->source_file->push_back(*it);
				}
				std::cout << "[!] Extracted Main Source [Python File: " << it->name <<  "]" << std::endl;
				break;
			}
		}
	}
}

void Extract::ExtractBaseLibrary() {
	for (auto it = this->toc_list->begin(); it != this->toc_list->end(); ++it) {
		// if the data isnt a library or a source file
		if (it->type_of_compressed_data != 0x73 && it->type_of_compressed_data != 0x6D && it->type_of_compressed_data != 0x4D) {
			// check the first 12 bytes
			std::vector<char> data_buf;
			data_buf.reserve(it->compressed_data_size);
			this->python_exe->seekg(it->position, std::ios::beg);
			this->python_exe->read(data_buf.data(), it->compressed_data_size);
			// check if data is a zip file
			if (std::string(data_buf.data()).substr(0, 12).find("\x50\x4B\x03\x04\x14\x00\x00\x00\x00\x00\x00\x00") != std::string::npos) {
				for (auto it = data_buf.begin(); it != data_buf.end(); ++it) {
					this->base_library->push_back(*it);
				}
				std::cout << "[!] Found extracted Base Library!" << std::endl;
				break;
			}
		}
	}
}

void Extract::WriteCompiledPythonFile() {
	std::ofstream file("compiled.pyc");
	file.write(this->source_file->data(), this->source_file->size());
	file.close();
}