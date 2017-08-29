#include <sfs.h>
#include <util.h>
#include <stdio.h>

bool SFS_VOLUME::Lock() const {
	if (!DeviceIoControl(handle, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, 0, 0)) {
		printf("Filed to dismount volume! Error: %u\n", GetLastError());
		return false;
	}

	if (!DeviceIoControl(handle, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, 0, 0)) {
		printf("Failed to lock volume! Error: %u\n", GetLastError());
		return false;
	}

	return true;
}

void SFS_VOLUME::InitTmpClusters() {

	tmpClusterD = new byte[mbr.ClusterSize * mbr.SectorSize];
	tmpClusterS = new byte[mbr.ClusterSize * mbr.SectorSize];
	tmpClusterN = new byte[mbr.ClusterSize * mbr.SectorSize];
	tmpClusterA = new byte[mbr.ClusterSize * mbr.SectorSize];
	memset(tmpClusterN, 0, mbr.ClusterSize * mbr.SectorSize);
}

bool SFS_VOLUME::IsFormatted() const {
	return mbr.ID == SFS_ID;
}

void SFS_VOLUME::Format(const char* volume_label, byte cluster_size, byte reserved_sectors) {
	this->~SFS_VOLUME();
	memset(&mbr, 0, sizeof(SFS_MBR));
	mbr.Version = SFS_VERSION;
	memset(mbr.VolumeLable, 0, sizeof(SFS_MBR::VolumeLable));

	size_t len = strlen(volume_label);

	len = len > 16 ? 16 : len;

	memcpy(mbr.VolumeLable, volume_label, len);

	qword totalSectors = 0;

	GetDiskSectorSize(handle, &totalSectors, &mbr.SectorSize);

	mbr.ReservedSectors = reserved_sectors < 1 ? 1 : reserved_sectors;
	mbr.ClusterSize = cluster_size;

	totalSectors -= mbr.ReservedSectors;

	qword sectorsPerSector = (mbr.SectorSize << 3) * mbr.ClusterSize;
	qword t = totalSectors;

	while (true) {
		if (t > sectorsPerSector + 1) {
			t -= sectorsPerSector + 1;
			mbr.TrackingSectors++;
		}
		else {
			if ((totalSectors - mbr.TrackingSectors) % sectorsPerSector) mbr.TrackingSectors++;
			break;
		}
	}

	mbr.DataSectors = totalSectors - mbr.TrackingSectors;
	mbr.ID = SFS_ID;

	mbr.Reserved1 = 0xFFFF;

	byte tmpp[4096] = { 0 };

	memcpy(tmpp, &mbr, sizeof(SFS_MBR));

	DiskWrite(handle, 0, mbr.SectorSize, tmpp);

	qword tmpSize = (mbr.ClusterSize * mbr.SectorSize) << 1;

	byte* tmp = new byte[tmpSize];
	memset(tmp, 0, tmpSize);

	for (qword i = 1; i < mbr.TrackingSectors; i++) {
		WriteSector(SFS_LBA_TRACKING_SECTORS(mbr) + i, 1, tmp);
	}

	tmp[0] |= 3;

	WriteSector(SFS_LBA_TRACKING_SECTORS(mbr), 1, tmp);

	memset(tmp, 0, tmpSize);

	WriteCluster(0, 1, tmp);



	WriteCluster(1, 1, tmp);

	delete[] tmp;

	InitTmpClusters();
}

qword SFS_VOLUME::AllocateCluster(bool setNull) const {
	for (qword i = 0; i < mbr.TrackingSectors; i++) {
		ReadSector(i + SFS_LBA_TRACKING_SECTORS(mbr), 1, tmpClusterA);
		for (word j = 0; j < mbr.SectorSize; j++) {
			byte& c = tmpClusterA[j];
			for (byte b = 0; b < 8; b++) {
				if (!SFS_CHECK_BIT(c, b)) {
					SFS_SET_BIT(c, b);

					WriteSector(i + mbr.ReservedSectors, 1, tmpClusterA);

					qword res = (i * mbr.SectorSize * 8) + j * 8 + b;
					if (setNull) {
						WriteCluster(res, 1, tmpClusterN);
					}

					return res;
				}
			}
		}
	}

	

	return SFS_CLUSTER_INVALID;
}

void SFS_VOLUME::FreeCluster(qword lba) const {
	qword sector = lba / (mbr.SectorSize * 8);
	word  byteInSector = (lba % (mbr.SectorSize * 8)) / 8;
	byte  bitInByte = lba % 8;

	ReadSector(mbr.ReservedSectors + sector, 1, tmpClusterD);

	SFS_CLEAR_BIT(tmpClusterD[byteInSector], bitInByte);

	WriteSector(mbr.ReservedSectors + sector, 1, tmpClusterD);
}

SFS_STRING SFS_VOLUME::CreateString(const char* name) const {
	qword numEntries = GetClusterSizeInBytes() / SFS_STRING_SIZE;
	qword cluster = SFS_CLUSTER_STRING;
	qword numClusters = 0;
	
	while(true) {
		ReadCluster(cluster, 1, tmpClusterS);

		for (qword i = 0; i < numEntries; i++) {
			byte* string = tmpClusterS + i * SFS_STRING_SIZE;

			if (*string == 0) {
				SFS_STRING str;

				size_t len = strlen(name);

				str.Size = len > 255 ? 255 : len;
				str.Index = numClusters * numEntries + i;

				memcpy(string, name, len);

				WriteCluster(cluster, 1, tmpClusterS);

				return str;
			}
		}

		cluster = *(qword*)(tmpClusterS + GetClusterSizeInBytes());

		if (!cluster) {
			qword newCluster = AllocateCluster(true);
			*(qword*)(tmpClusterS + GetClusterSizeInBytes()) = newCluster;
			WriteCluster(cluster, 1, tmpClusterS);
			cluster = newCluster;
		}

		numClusters++;
	}
}

void SFS_VOLUME::FreeString(const SFS_STRING& str) const {
	qword numEntries = GetClusterSizeInBytes() / SFS_STRING_SIZE;

	qword cluster = str.Index / numEntries;
	qword indexInCluster = str.Index % numEntries;

	ReadCluster(SFS_CLUSTER_STRING, 1, tmpClusterS);

	qword nextCluster = SFS_CLUSTER_STRING;

	for (qword i = 0; i < cluster; i++) {
		nextCluster = *(qword*)(tmpClusterS + GetClusterSizeInBytes());

		ReadCluster(nextCluster, 1, tmpClusterS);
	}

	memset(tmpClusterS + indexInCluster * SFS_STRING_SIZE, 0, SFS_STRING_SIZE);

	WriteCluster(nextCluster, 1, tmpClusterS);
}

char* SFS_VOLUME::GetString(const SFS_STRING& str) const {
	qword numEntries = GetClusterSizeInBytes() / SFS_STRING_SIZE;

	qword cluster = str.Index / numEntries;
	qword indexInCluster = str.Index % numEntries;

	ReadCluster(SFS_CLUSTER_STRING, 1, tmpClusterS);

	for (qword i = 0; i < cluster; i++) {
		qword nextCluster = *(qword*)(tmpClusterS + GetClusterSizeInBytes());

		ReadCluster(nextCluster, 1, tmpClusterS);
	}

	char* string = new char[str.Size];
	memcpy(string, tmpClusterS + indexInCluster * SFS_STRING_SIZE, str.Size);

	return string;
}

qword SFS_VOLUME::GetFolderRecursive(const char* path, qword parentLBA, bool create) const {
	char** names = nullptr;
	dword num = 0;

	if (!path) {
		return parentLBA;
	}

	num = (dword)CountSlash(path);

	if (num == 0) {
		if (strlen(path)) return GetFolder(path, parentLBA, create);

		return parentLBA;
	}

	SplitPath(path, &names, &num);

	qword lastLBA = parentLBA;

	for (dword i = 0; i < num; i++) {
		lastLBA = GetFolder(names[i], lastLBA, create);

		if (lastLBA == SFS_CLUSTER_INVALID) {
			return SFS_CLUSTER_INVALID;
		}
	}

	FreePaths(names, num);

	return lastLBA;
}

qword SFS_VOLUME::GetFolder(const char* name, qword parentLBA, bool create) const {
	ReadCluster(parentLBA, 1, tmpClusterD);

	qword numEntries = GetEntriesInCluster<SFS_FILE_ENTRY>();
	byte len = (byte)strlen(name);
	while (true) {
		for (qword i = 0; i < numEntries; i++) {
			SFS_FILE_ENTRY& e = ((SFS_FILE_ENTRY*)tmpClusterD)[i];

			if (e.Type != SFS_FILE_TYPE_UNUSED) {

				if (len != e.Name.Size) continue;

				char* folderName = GetString(e.Name);
				byte res = memcmp(name, folderName, e.Name.Size);

				delete[] folderName;

				if (res == 0) {
					if (e.Type == SFS_FILE_TYPE_FOLDER) {
						return e.LBA;
					} else {
						return SFS_CLUSTER_INVALID;
					}
				}
			}
		}

		qword nextCluster = *(qword*)(tmpClusterD + GetClusterSizeInBytes());

		if (!nextCluster) {
			break;
		}
	}

	if (create) {
		ReadCluster(parentLBA, 1, tmpClusterD);

		while (true) {
			for (qword i = 0; i < numEntries; i++) {
				SFS_FILE_ENTRY& e = ((SFS_FILE_ENTRY*)tmpClusterD)[i];

				if (e.Type == SFS_FILE_TYPE_UNUSED) {
					e.Type = SFS_FILE_TYPE_FOLDER;
					e.Name = CreateString(name);
					e.LBA = AllocateCluster(true);

					WriteCluster(parentLBA, 1, tmpClusterD);

					return e.LBA;
				}
			}

			qword nextCluster = *(qword*)(tmpClusterD + GetClusterSizeInBytes());

			if (!nextCluster) {
				nextCluster = AllocateCluster(true);
				*(qword*)(tmpClusterD + GetClusterSizeInBytes()) = nextCluster;
				WriteCluster(parentLBA, 1, tmpClusterD);
			}

			ReadCluster(nextCluster, 1, tmpClusterD);
		}
	}
	
	return SFS_CLUSTER_INVALID;
}

qword SFS_VOLUME::CreateFolder(const char* path, qword parentLBA) const {
	return GetFolderRecursive(path, parentLBA, true);
}

SFS_FILE_ENTRY* SFS_VOLUME::FindFile(qword folderCluster, const char* name, qword* lba, bool create) const {
	qword numEntries = GetEntriesInCluster<SFS_FILE_ENTRY>();
	qword cluster = folderCluster;
	if (lba) *lba = 0;
	byte len = (byte)strlen(name);

	while (true) {
		ReadCluster(cluster, 1, tmpClusterD);

		for (qword i = 0; i < numEntries; i++) {
			SFS_FILE_ENTRY& e = ((SFS_FILE_ENTRY*)tmpClusterD)[i];

			if (e.Type != SFS_FILE_TYPE_UNUSED) {
			
				if (len != e.Name.Size) continue;

				char* fname = GetString(e.Name);
				byte res = memcmp(name, fname, e.Name.Size);

				delete[] fname;

				if (res == 0) {
					if (e.Type == SFS_FILE_TYPE_FILE) {
						if (lba) *lba = cluster;
						return &e;
					} else {
						return nullptr;
					}
				}
			}

		}

		cluster = *(qword*)(tmpClusterD + GetClusterSizeInBytes());

		if (!cluster) {
			break;
		}
	}

	if (!create || !lba) return nullptr;

	cluster = folderCluster;

	while (true) {
		ReadCluster(cluster, 1, tmpClusterD);

		for (qword i = 0; i < numEntries; i++) {
			SFS_FILE_ENTRY& e = ((SFS_FILE_ENTRY*)tmpClusterD)[i];

			if (e.Type == SFS_FILE_TYPE_UNUSED) {
				e.Type = SFS_FILE_TYPE_FILE;
				e.Name = CreateString(name);
				e.LBA = AllocateCluster();
				e.Size = 0;

				WriteCluster(cluster, 1, tmpClusterD);

				*lba = cluster;
				return &e;
			}

		}

		cluster = *(qword*)(tmpClusterD + GetClusterSizeInBytes());

		if (!cluster) {
			qword newCluster = AllocateCluster(true);
			*(qword*)(tmpClusterD + GetClusterSizeInBytes()) = newCluster;
			WriteCluster(cluster, 1, tmpClusterD);
			cluster = newCluster;
		}
	}

	return nullptr;
}

dword SFS_VOLUME::WriteBootCode(byte* data, dword size) const {

	if (size > mbr.SectorSize * mbr.ReservedSectors) return SFS_ERROR;

	byte tmpp[4096] = { 0 };

	dword sectors = size / mbr.SectorSize + 1;
	dword written = 0;

	for (dword i = 0; i < sectors; i++) {

		dword toWrite = size - written;
		toWrite = toWrite > mbr.SectorSize ? mbr.SectorSize : toWrite;

		memcpy(tmpp, data + written, toWrite);

		DiskWrite(handle, 0, mbr.SectorSize, tmpp);

		written += toWrite;
	}

	

	return SFS_ERROR_NONE;
}

dword SFS_VOLUME::WriteFile(const char* absolute_path, qword size, const void* data, dword attr) const {
	if (!data) return SFS_ERROR;
	bool overwrite = (attr & SFS_ATTR_OVERWRITE) == SFS_ATTR_OVERWRITE;

	char* filename;
	char* path;

	GetFileNameAndPath(absolute_path, &path, &filename);

	qword folder = GetFolderRecursive(path, 0, true);

	if (folder == SFS_CLUSTER_INVALID) {
		delete[] filename, path;
		return SFS_ERROR;
	}

	qword entryCluster = 0;

	SFS_FILE_ENTRY* file = FindFile(folder, filename, &entryCluster, true);

	delete[] filename, path;

	if (file == nullptr) return SFS_ERROR;

	if (file->Size) {
		if (!overwrite) return SFS_ERROR_EXIST;
	} else {
		overwrite = false;
	}

	file->Size = size;

	WriteCluster(entryCluster, 1, tmpClusterD);

	qword requiredClusters = size / GetClusterSizeInBytes() +(size % GetClusterSizeInBytes() ? 1 : 0) - 1;
	qword cluster = file->LBA;

	if (overwrite) {
		for (qword i = 0; i < requiredClusters; i++) {
			ReadCluster(cluster, 1, tmpClusterD);

			memcpy(tmpClusterD, (const byte*)data+(i * GetClusterSizeInBytes()), GetClusterSizeInBytes());

			qword* nextCluster = (qword*)(tmpClusterD + GetClusterSizeInBytes());

			if (*nextCluster == 0) {
				*nextCluster = AllocateCluster(true);
			}

			WriteCluster(cluster, 1, tmpClusterD);

			cluster = *nextCluster;
		}
		ReadCluster(cluster, 1, tmpClusterD);

		memcpy(tmpClusterD, (const byte*)data+(GetClusterSizeInBytes() * requiredClusters), size - GetClusterSizeInBytes() * requiredClusters);

		*(qword*)(tmpClusterD + GetClusterSizeInBytes()) = 0;

		WriteCluster(cluster, 1, tmpClusterD);
	} else {
		for (qword i = 0; i < requiredClusters; i++) {
			memcpy(tmpClusterD, (const byte*)data + (i * GetClusterSizeInBytes()), GetClusterSizeInBytes());

			qword nextCluster = AllocateCluster();

			*(qword*)(tmpClusterD + GetClusterSizeInBytes()) = nextCluster;

			WriteCluster(cluster, 1, tmpClusterD);

			cluster = nextCluster;
		}

		memcpy(tmpClusterD, (const byte*)data + (GetClusterSizeInBytes() * requiredClusters), size - GetClusterSizeInBytes() * requiredClusters);

		*(qword*)(tmpClusterD + GetClusterSizeInBytes()) = 0;

		WriteCluster(cluster, 1, tmpClusterD);
	}

	return SFS_ERROR_NONE;
}

dword SFS_VOLUME::ReadFile(const char* absolute_path, qword* size, void** data) const {
	char* filename;
	char* path;
	*size = 0;
	GetFileNameAndPath(absolute_path, &path, &filename);

	qword folder = GetFolderRecursive(path);

	if (folder == SFS_CLUSTER_INVALID) {
		delete[] filename, path;
		return SFS_ERROR_INVALID_PATH;
	}

	qword numEntries = GetEntriesInCluster<SFS_FILE_ENTRY>();
	
	SFS_FILE_ENTRY* e = FindFile(folder, filename, nullptr);

	delete[] filename, path;

	if (!e) return SFS_ERROR_INVALID_PATH;
	
	*size = e->Size;

	if (data == nullptr) return SFS_ERROR;

	*data = new byte[e->Size];

	qword numClusters = e->Size / GetClusterSizeInBytes() + (e->Size % GetClusterSizeInBytes() ? 1 : 0) - 1;
	qword cluster = e->LBA;

	for (qword i = 0; i < numClusters; i++) {
		ReadCluster(cluster, 1, tmpClusterD);

		memcpy((byte*)*data + GetClusterSizeInBytes() * i, tmpClusterD, GetClusterSizeInBytes());

		cluster = *(qword*)(tmpClusterD + GetClusterSizeInBytes());
	}

	ReadCluster(cluster, 1, tmpClusterD);

	memcpy((byte*)*data + GetClusterSizeInBytes() * numClusters, tmpClusterD, *size - GetClusterSizeInBytes() * numClusters);

	return SFS_ERROR_NONE;
}

dword SFS_VOLUME::DeleteFile(const char* absolute_path, qword parentFolder) {
	char* filename;
	char* path;

	GetFileNameAndPath(absolute_path, &path, &filename);

	qword folder = GetFolderRecursive(path, parentFolder);

	if (folder == SFS_CLUSTER_INVALID) {
		delete[] filename, path;
		return SFS_ERROR_INVALID_PATH;
	}

	qword lba = 0;

	SFS_FILE_ENTRY* e = FindFile(folder, filename, &lba);

	delete[] filename, path;

	if (!e) return SFS_ERROR_INVALID_PATH;

	SFS_FILE_ENTRY file = *e;

	memset(e, 0, sizeof(SFS_FILE_ENTRY));

	WriteCluster(lba, 1, tmpClusterD);

	FreeString(file.Name);

	qword numClusters = file.Size / GetClusterSizeInBytes() + (file.Size % GetClusterSizeInBytes() ? 1 : 0)-1;
	qword cluster = file.LBA;

	FreeCluster(cluster);

	for (qword i = 0; i < numClusters; i++) {

		ReadCluster(cluster, 1, tmpClusterD);

		cluster = *(qword*)(tmpClusterD + GetClusterSizeInBytes());

		FreeCluster(cluster);
	}

	return SFS_ERROR_NONE;
}

dword SFS_VOLUME::DeleteFolder(const char* path, qword parentFolder) {
	return SFS_ERROR;
}

void SFS_VOLUME::ReadSector(qword offset, qword size, void* data) const {
	DiskRead(handle, offset * mbr.SectorSize, size * mbr.SectorSize, data);
}

void SFS_VOLUME::ReadCluster(qword offset, qword size, void* data) const {
	ReadSector(offset * mbr.ClusterSize + SFS_LBA_FIRST_CLUSTER(mbr), size * mbr.ClusterSize, data);
}

void SFS_VOLUME::WriteSector(qword offset, qword size, const void* data) const {
	DiskWrite(handle, offset * mbr.SectorSize, size * mbr.SectorSize, data);
}

void SFS_VOLUME::WriteCluster(qword offset, qword size, const void* data) const {
	WriteSector(offset * mbr.ClusterSize + SFS_LBA_FIRST_CLUSTER(mbr), size * mbr.ClusterSize, data);
}