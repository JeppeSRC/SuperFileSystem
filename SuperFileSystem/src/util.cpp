#include "util.h"
#include <stdio.h>

void SetOffset(HANDLE disk, qword offset) {
	dword high = (offset >> 32) & 0xFFFFFFFF;
	SetFilePointer(disk, offset & 0xFFFFFFFF, (long*)&high, FILE_BEGIN);
}

HANDLE OpenDisk(const char* path) {
	
	HANDLE handle = CreateFileA(path, GENERIC_ALL, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

	return handle;
}

void CloseDisk(HANDLE disk) {
	CloseHandle(disk);
}

void DiskRead(HANDLE disk, qword offset, qword size, void* data) {
	SetOffset(disk, offset);
	ReadFile(disk, data, size, 0, 0);
}

/*
void DiskReadSector(SFS_VOLUME* volume, qword offset, qword size, void* data) {
	DiskRead(volume->handle, offset * volume->mbr.SectorSize, size * volume->mbr.SectorSize, data);
}

void DiskReadCluster(SFS_VOLUME* volume, qword offset, qword size, void* data) {
	DiskReadSector(volume, offset * volume->mbr.ClusterSize + SFS_LBA_DATA_SECTORS(volume->mbr), size * volume->mbr.ClusterSize, data);
}*/

void DiskWrite(HANDLE disk, qword offset, qword size, const void* data) {
	SetOffset(disk, offset);
	WriteFile(disk, data, size, 0, 0);
}

/*
void DiskWriteSector(SFS_VOLUME* volume, qword offset, qword size, const void* data) {
	DiskWrite(volume->handle, offset * volume->mbr.SectorSize, size * volume->mbr.SectorSize, data);
}

void DiskWriteCluster(SFS_VOLUME* volume, qword offset, qword size, const void* data) {
	DiskWriteSector(volume, offset * volume->mbr.ClusterSize + SFS_LBA_DATA_SECTORS(volume->mbr), size * volume->mbr.ClusterSize, data);
}*/

qword GetDiskSize(HANDLE disk) {
	dword high = 0;
	qword low = GetFileSize(disk, (LPDWORD)&high);
	return low | ((qword)high) << 32;
}

void GetDiskSectorSize(HANDLE disk, qword* sectors, word* bytes_per_sector) {

	dword ret = 0;
	DISK_GEOMETRY geo;

	DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_GEOMETRY, 0, 0, &geo, sizeof(DISK_GEOMETRY), (LPDWORD)&ret, 0);

	if (ret != sizeof(DISK_GEOMETRY)) {
		*bytes_per_sector = 512;
		*sectors = GetDiskSize(disk) / 512;
		return;
	}

	*bytes_per_sector = geo.BytesPerSector;
	*sectors = geo.Cylinders.QuadPart * geo.TracksPerCylinder * geo.SectorsPerTrack;
}