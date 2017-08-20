#pragma once
#include <Windows.h>

#ifdef BUILD_SFS
#define API __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif

typedef unsigned long long qword;
typedef unsigned int dword;
typedef unsigned short word;
typedef unsigned char byte;

/*
struct SFS_CLUSTRER { CLuster - nextCluster = available data
	...
	qword nextCluster;
};
*/

/*
Format layout

-Sector SFS_MBR (ReservedSectors) LBA 0

-ReservedSectors (if any)

-TrackingSectors LBA (ReservedSectors)

	Stores status of each cluster in a single bit, a 512 byte sector can track 4096 clusters

-DataSectors LBA (ReservedSectors + TrackingSectors):
	
	First cluster is the first file directory (root)

	Second cluster is the first string directory

	Following clusters will be allocated as needed
*/

#define SFS_VERSION 0x0001;
#define SFS_ID		0xFF22

#pragma pack(push, 1)
struct SFS_MBR { // size: 45
	byte	Reserved0[3];
	word	ID; // ID to verify that it is an SFS volume (0xFF22)
	word	Version; // SFS Version
	word	Reserved1; // Reserved for future use
	byte	VolumeLable[16]; // Name of volume
	byte	ReservedSectors; // Reserved sectors in the beginning of the partition, including mbr. Must be at least 1
	word	SectorSize; //Size of one sector in bytes;
	byte	ClusterSize; //Size of a cluster in sectors
	qword	TrackingSectors; //Number of sectors used to track all clusters
	qword	DataSectors; //Number of sectors available for data use
};
#pragma pack(pop)

#pragma pack(push, 1)
struct SFS_DATE_TIME { //size: 4
	/*byte Minute : 6; 
	byte Hours : 5; //0-24
	byte Day : 5;
	word Year : 12; // Offset from 2000 to 3023
	byte Month : 4;*/
	word time;
	word date;
};
#pragma pack(pop)

#define SFS_STRING_SIZE 255

#pragma pack(push, 1)
struct SFS_STRING { //size: 5
	dword Index; //Index of string location
	byte  Size; //Length of string
};
#pragma pack(pop)

enum SFS_FILE_TYPE : byte {
	SFS_FILE_TYPE_UNUSED,
	SFS_FILE_TYPE_FILE,
	SFS_FILE_TYPE_FOLDER
};

#pragma pack(push, 1)
struct SFS_FILE_ENTRY { //size: 34
	SFS_FILE_TYPE	Type; //File type (file or folder descriptor)
	qword			Reserved; //Reserved for future use
	SFS_STRING		Name;  //File (including extension) or folder name
	SFS_DATE_TIME	Date; //Date of creation
	qword			Size; //File or folder(size of all files and folders inside it) size
	qword			LBA; //First cluster storing the data
};
#pragma pack(pop)

#define SFS_LBA_TRACKING_SECTORS(x) (x.ReservedSectors)
#define SFS_LBA_DATA_SECTORS(x) (SFS_LBA_TRACKING_SECTORS(x) + x.TrackingSectors)
#define SFS_LBA_ROOT_DIRECTORY(x) (SFS_LBA_DATA_SECTORS(x))
#define SFS_LBA_FIRST_CLUSTER(x) (SFS_LBA_DATA_SECTORS(x))
#define SFS_LBA_STRING_DIRECTORY(x) (SFS_LBA_ROOT_DIRECTORY(x) + x.ClusterSize)

#define SFS_CLUSTER_DATA 0x00
#define SFS_CLUSTER_STRING 0x01
#define SFS_CLUSTER_INVALID (~0)

#define SFS_CLUSTER_FREE 0x00
#define SFS_CLUSTER_USED 0x01

#define SFS_CHECK_BIT(x, bit) (x & (1 << bit))
#define SFS_SET_BIT(x, bit) (x |= (1 << bit))
#define SFS_CLEAR_BIT(x, bit) (x ^= (1 << bit))

#define SFS_SUCCESS 0x00

#define SFS_ERROR				(~0)
#define SFS_ERROR_NONE			SFS_SUCCESS
#define SFS_ERROR_INVALID_PATH	0x01
#define SFS_ERROR_EXIST			0x02

#define SFS_ATTR_OVERWRITE		0x01

#undef CreateFile
#undef DeleteFile

class API SFS_VOLUME {
private:
	byte* tmpClusterS = nullptr;
	byte* tmpClusterD = nullptr;
	byte* tmpClusterN = nullptr;
	byte* tmpClusterA = nullptr;
public:
	~SFS_VOLUME() { delete[] tmpClusterD; delete[] tmpClusterS; delete[] tmpClusterN; }

	HANDLE handle;

	SFS_MBR mbr;

	void InitTmpClusters();
	bool IsFormatted() const;

	void Format(const char* volume_label, byte cluser_size, byte reserved_sectors);

	qword AllocateCluster(bool setNull = false) const;
	void  FreeCluster(qword lba) const;

	SFS_STRING CreateString(const char* string) const;
	void  FreeString(const SFS_STRING& str) const;
	char* GetString(const SFS_STRING& str) const;

	qword GetFolderRecursive(const char* path, qword parentLBA = 0, bool create = false) const;
	qword GetFolder(const char* name, qword parentLBA, bool create = false) const;


	qword CreateFolder(const char* path, qword parentLBA = SFS_CLUSTER_DATA) const;
	SFS_FILE_ENTRY* FindFile(qword folderCluster, const char* name, qword* lba, bool create = false) const;

	dword WriteBootCode(byte* data, dword size) const;

	dword WriteFile(const char* path, qword size, const void* data, dword attributes) const;
	dword ReadFile(const char* path, qword* size, void** data) const;
	dword DeleteFile(const char* path, qword parentFolder = SFS_CLUSTER_DATA);
	dword DeleteFolder(const char* path, qword parentFolder = SFS_CLUSTER_DATA);

	void ReadSector(qword offset, qword size, void* data) const;
	void ReadCluster(qword offset, qword size, void* data) const;

	void WriteSector(qword offset, qword size, const void* data) const;
	void WriteCluster(qword offset, qword size, const void* data) const;

	inline qword GetClusterSizeInBytes() const { return mbr.ClusterSize * mbr.SectorSize - 8; }

	template<typename T>
	inline qword GetEntriesInCluster() const { return GetClusterSizeInBytes() / sizeof(T); }
};

API SFS_VOLUME* LoadDisk(const char* path);
API void		CloseDisk(SFS_VOLUME* vol);

