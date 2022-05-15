#pragma once
#include <fstream>
#include <vector>
#include <array>
#include "Tentry.h"
#define MAGIC "MEI\014\013\012\013\016"
#define PYINST20_COOKIE_SIZE 24        
#define PYINST21_COOKIE_SIZE 88
class Extract {
private:
	static int python_exe_version;
	std::ifstream* python_exe;
	std::vector<TOC_ENTRY>* toc_list;
	std::vector<char>* source_file;
	std::array<char, 4> magic_number = {0x55, 0xd, 0xd, 0xa};
	std::vector<char>* base_library;
	size_t file_size;
	int cookie_position;
	int overlay_size;
	int overlay_position;
	int table_of_contents_position;
	int table_of_contents_size;
	int python_version;

	void SearchForExeVersion();
	void GetPythonExeFileSize();
	void GetPythonArchiveInfo();
	void ParseTableOfContents();
	void GetSourceFile();
	void ExtractBaseLibrary();
	void WriteCompiledPythonFile();
public:
	
	Extract();

	void DecompilePythonExe(std::string file);

};