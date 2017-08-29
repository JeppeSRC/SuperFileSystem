#include "sfs.h"
#include <util.h>
#include <stdio.h>

SFS_VOLUME* LoadDisk(const char* path) {
	SFS_VOLUME* vol = new SFS_VOLUME;

	vol->handle = OpenDisk(path);

	if (vol->handle == INVALID_HANDLE_VALUE) {
		dword e = GetLastError();
		printf("Failed to open disk \"%s\"", path);
		delete vol;

		return nullptr;
	}

	byte tmpp[4096] = { 0 };

	DiskRead(vol->handle, 0, sizeof(tmpp), tmpp);

	memcpy(&vol->mbr, tmpp, sizeof(SFS_MBR));

	if (vol->IsFormatted()) {
		vol->InitTmpClusters();
	}

	return vol;
}

void CloseDisk(SFS_VOLUME* volume) {
	CloseDisk(volume->handle);

	delete volume;
}