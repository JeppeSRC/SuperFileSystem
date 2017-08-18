#pragma once

#include <sfs.h>

void SetOffset(HANDLE disk, qword offset);

HANDLE OpenDisk(const char* path);
void   CloseDisk(HANDLE handle);

void DiskRead(HANDLE disk, qword offset, qword size, void* data);
//void DiskReadSector(SFS_VOLUME* volume, qword offset, qword size, void* data);
//void DiskReadCluster(SFS_VOLUME* volume, qword offset, qword size, void* data);
void DiskWrite(HANDLE disk, qword offset, qword size, const void* data);
//void DiskWriteSector(SFS_VOLUME* volume, qword offset, qword size, const void* data);
//void DiskWriteCluster(SFS_VOLUME* volume, qword offset, qword size, const void* data);

qword GetDiskSize(HANDLE disk);
void  GetDiskSectorSize(HANDLE disk, qword* sectors, word* bytes_per_sector);

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

	*num_paths = CountSlash(path)+1;

	*paths = new char*[*num_paths];


	char** dir = *paths;

	size_t len = strlen(path);
	size_t last = 0;

	for (size_t i = 1; i < len; i++) {
		const char c = path[i];
		if (c == '/' || c == '\\') {
			size_t strlen = i - last;
			dir[index] = new char[strlen+1];
			memset(dir[index], 0, strlen+1);
			memcpy(dir[index], path + last, strlen);
			last = i + 1;
			index++;
		}
	}

	size_t strlen = len - last;
	dir[index] = new char[strlen+1];
	memset(dir[index], 0, strlen + 1);
	memcpy(dir[index], path + last, strlen);
}

inline void GetFileNameAndPath(const char* pathToFile, char** path, char** filename) {
	size_t len = strlen(pathToFile);

	if (CountSlash(pathToFile) == 0) {
		*path = nullptr;
		*filename = new char[len + 1];
		memset(*filename, 0, len + 1);
		memcpy(*filename, pathToFile, len);
		return;
	}

	size_t slashIndex = len-1;
	
	for (; slashIndex >= 0; slashIndex--) {
		const char c = pathToFile[slashIndex];
		if (c == '/' || c == '\\') break;
	}

	size_t pathLen = slashIndex;
	size_t nameLen = len - pathLen;

	*path = new char[pathLen +1 ];
	*filename = new char[nameLen + 1];

	memset(*path, 0, pathLen + 1);
	memset(*filename, 0, nameLen + 1);

	memcpy(*path, pathToFile, pathLen);
	memcpy(*filename, pathToFile + pathLen+1, nameLen);
}