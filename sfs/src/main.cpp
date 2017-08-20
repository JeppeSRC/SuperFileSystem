#include <sfs.h>

#include <stdio.h>

char* RootDir;
char* DrivePath;
char* Label;

const char* CMD_FORMAT = "format";
const char* CMD_FORMAT_VOLUMELABEL = "l=";
const char* CMD_FORMAT_CLUSTERSIZE = "s=";
const char* CMD_FORMAT_RESERVEDSECTORS = "r=";

const char* CMD_WRITE = "write";
const char* CMD_WRITE_DEST = "d=";
const char* CMD_WRITE_SRC = "f=";
const char* CMD_WRITE_OVERWRITE = "-o";
const char* CMD_WRITE_BOOT = "-b";

const char* CMD_DELETE = "delete";
const char* CMD_DELETE_DEST = "d=";

#define DEFAULT_CLUSTERSIZE 4
#define DEFAULT_RESERVEDSECTORS 1

inline void* ReadFile(const char* filename, size_t* size) {
	FILE* file = fopen(filename, "rb");

	if (!file) {
		printf("Error: Failed to open file \"%s\"\n", filename);
		return nullptr;
	}

	fseek(file, 0, SEEK_END);
	*size = ftell(file);
	fseek(file, 0, SEEK_SET);

	byte* data = new byte[*size];

	fread(data, *size, 1, file);

	fclose(file);

	return data;
}

inline void FreePaths(char** paths, dword num) {
	for (dword i = 0; i < num; i++) {
		delete[] paths[i];
	}

	delete[] paths;
}

inline size_t CountSlash(const char* path) {
	size_t len = strlen(path);
	size_t res = 0;
	for (size_t i = 0; i < len; i++) {
		if (path[i] == '/' || path[i] == '\\') res++;
	}

	return res;
}

inline void SplitPath(const char* path, char*** paths, dword* num_paths) {
	word index = 0;

	*num_paths = CountSlash(path) + 1;

	*paths = new char*[*num_paths];


	char** dir = *paths;

	size_t len = strlen(path);
	size_t last = 0;

	for (size_t i = 1; i < len; i++) {
		const char c = path[i];
		if (c == '/' || c == '\\') {
			size_t strlen = i - last;
			dir[index] = new char[strlen + 1];
			memset(dir[index], 0, strlen + 1);
			memcpy(dir[index], path + last, strlen);
			last = i + 1;
			index++;
		}
	}

	size_t strlen = len - last;
	dir[index] = new char[strlen + 1];
	memset(dir[index], 0, strlen + 1);
	memcpy(dir[index], path + last, strlen);
}

bool StartsWith(const char* src, const char* src2) {
	size_t srclen = strlen(src);
	size_t src2len = strlen(src2);

	if (srclen < src2len) return false;

	return memcmp(src, src2, src2len) == 0;
}

bool Exists(const char* src, const char* src2) {
	size_t srclen = strlen(src);
	size_t src2len = strlen(src2);

	if (srclen < src2len) return false;

	for (size_t i = 0; i < srclen - src2len; i++) {
		bool match = true;
		for (size_t j = 0; j < src2len; j++) {
			match = src[i + j] == src2[j];
			if (!match) break;
		}

		if (match) return true;
	}

	return false;
}

size_t OptionGetInt(const char* cmd, const char* option) {
	size_t len = strlen(cmd);
	size_t olen = strlen(option);

	if (len < olen) return ~0;

	for (size_t i = 0; i < len - olen; i++) {
		bool match = true;
		for (size_t j = 0; j < olen; j++) {
			match = cmd[i + j] == option[j];
			if (!match) break;
		}

		if (match) {
			return (size_t)atoi(cmd + i+olen);
		}
	}

	return ~0;
}

const char* OptionGetString(const char* cmd, const char* option, size_t* length, bool create) {
	size_t len = strlen(cmd);
	size_t olen = strlen(option);

	if (len < olen) return nullptr;

	for (size_t i = 0; i < len - olen; i++) {
		bool match = true;
		for (size_t j = 0; j < olen; j++) {
			match = cmd[i + j] == option[j];
			if (!match) break;
		}

		if (match) {
			char first = cmd[i + olen];
			bool ignoreSpaces = first == '\"';

			size_t beginning = i + olen + (ignoreSpaces ? 1 : 0);
			size_t offset = beginning;

			char endChar = ignoreSpaces ? '\"' : ' ';

			while (cmd[offset++] != endChar && cmd[offset-1] != '\n');

			*length = offset - beginning - 1;

			if (!create) return cmd + beginning;

			char* newString = new char[*length + 1];
			newString[*length] = 0;

			memcpy(newString, cmd + beginning, *length);

			return newString;
		}
	}

	*length = 0;

	return nullptr;
}

void ParseCMD(const char* cmd, SFS_VOLUME* vol) {
	size_t len = 0;
	if (StartsWith(cmd, CMD_FORMAT)) {
		size_t clusterSize = OptionGetInt(cmd, CMD_FORMAT_CLUSTERSIZE);
		size_t reservedSectors = OptionGetInt(cmd, CMD_FORMAT_RESERVEDSECTORS);


		char*  volLabel = (char*)OptionGetString(cmd, CMD_FORMAT_VOLUMELABEL, &len, false);

		if (clusterSize == ~0) clusterSize = DEFAULT_CLUSTERSIZE;
		if (reservedSectors == ~0) reservedSectors = DEFAULT_RESERVEDSECTORS;
		if (volLabel == nullptr) volLabel = "DEFAULT_LABEL";
		else volLabel[len] = 0;

		printf("Formatting: <Label: %s> <ClusterSize: %u> <ReservedSectors: %u>\n", volLabel, clusterSize, reservedSectors);
		vol->Format(volLabel, clusterSize, reservedSectors);
		Label = (char*)vol->mbr.VolumeLable;
	} else if (StartsWith(cmd, CMD_WRITE)) {
		bool overwrite = Exists(cmd, CMD_WRITE_OVERWRITE);
		bool boot = Exists(cmd, CMD_WRITE_BOOT);

		const char* dest = OptionGetString(cmd, CMD_WRITE_DEST, &len, true);
		const char* src = OptionGetString(cmd, CMD_WRITE_SRC, &len, true);

		size_t size = 0;

		void* data = ReadFile(src, &size);

		if (!data) {
			printf("Failed to read source file!\n");
			return;
		}
		
		dword res = SFS_ERROR_NONE;

		if (boot) res = vol->WriteBootCode((byte*)data, size);
		else res = vol->WriteFile(dest, size, data, overwrite ? SFS_ATTR_OVERWRITE : 0);

		if (res != SFS_ERROR_NONE) {
			if (res == SFS_ERROR_EXIST) {
				printf("File already exist! Use -o to overwrite\n");
			}
			else {
				if (boot) printf("Failed to write boot code: too large buffer!\n");
				else printf("Failed to write file!\n");
			}

			delete[] dest, src;
			return;
		}

		printf("Writing: %s -> %s\n", src, dest);

		delete[] dest, src;
	} else if (StartsWith(cmd, CMD_DELETE)) {
		const char* dest = OptionGetString(cmd, CMD_DELETE_DEST, &len, true);

		dword res = vol->DeleteFile(dest);

		if (res == SFS_ERROR_INVALID_PATH) {
			printf("File doesn't exist!\n");
		}

		delete[] dest;
	} else if (StartsWith(cmd, "exit")) {
		CloseDisk(vol);
		exit(0);
	} else {

		printf("Unknown command: %s\n", cmd);
	}



}

int main(int argc, char** argv) {

	RootDir = argv[0];

	if (argc < 2) {
		printf("Usage: sfs <drive/file>\n");
		return 1;
	}

	DrivePath = argv[1];
	
	SFS_VOLUME* vol = LoadDisk(DrivePath);
	
	if (vol->IsFormatted()) {
		Label = (char*)vol->mbr.VolumeLable;
	} else {
		Label = DrivePath;
	}

	char cmd[512];

	while (true) {
		memset(cmd, 0, sizeof(cmd));
		printf("%s: ", Label);
		fgets(cmd, sizeof(cmd), stdin);

		ParseCMD(cmd, vol);

	}


	CloseDisk(vol);
}

