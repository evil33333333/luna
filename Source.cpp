#include <iostream>
#include <fstream>
#include "Extract.h"

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Bad arguments provided";
		return -1;
	}
	std::string filename = argv[1];
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cerr << "Could not open file";
		return -1;
	}
	file.close();
	Extract ex;
	ex.DecompilePythonExe(filename);
	std::cout << "written pyc file [beta]";
	
}