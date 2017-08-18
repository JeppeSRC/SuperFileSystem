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

	DiskRead(vol->handle, 0, sizeof(SFS_MBR), &vol->mbr);

	if (vol->IsFormatted()) {
		vol->InitTmpClusters();
	}

	return vol;
}

void CloseDisk(SFS_VOLUME* volume) {
	CloseDisk(volume->handle);

	delete volume;
}