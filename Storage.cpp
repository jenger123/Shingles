//---------------------------------------------------------------------------
#include "Global.h"
#include "MyStorage.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
StorageFragmentClass::StorageFragmentClass()
{
	FileHandle = 0;
	StartOffset = 0;
	DataSize = 0;
}

//---------------------------------------------------------------------------
StorageFragmentClass::StorageFragmentClass(wstring fileName, ULONGLONG startOffset, ULONGLONG dataSize)
{
	FileName = fileName;
	StartOffset = startOffset;
	DataSize = dataSize;

	FileHandle = 0;
}
//---------------------------------------------------------------------------
WORD StorageFragmentClass::GetFileName(WCHAR *fileName)
{
	if (fileName)
	{
		wcscpy(fileName, FileName.c_str());
	}

	return FileName.length();
}
//---------------------------------------------------------------------------
ULONGLONG StorageFragmentClass::GetDataSize()
{
	return DataSize;
}
//---------------------------------------------------------------------------
ULONGLONG StorageFragmentClass::GetStartOffset()
{
	return StartOffset;
}
//---------------------------------------------------------------------------
HANDLE StorageFragmentClass::GetFileHandle()
{
	return FileHandle;
}
//---------------------------------------------------------------------------
void StorageFragmentClass::ResetFileHandle()
{
	FileHandle = 0;
}
//---------------------------------------------------------------------------
bool StorageFragmentClass::Open()
{
	FileHandle = CreateFileW(
			FileName.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			// FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
			// FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

	if (FileHandle == INVALID_HANDLE_VALUE)
	{
		FileHandle = 0;
		return false;
	}

	return true;
}
//---------------------------------------------------------------------------
void StorageFragmentClass::Close()
{
	if (FileHandle)
	{
		CloseHandle(FileHandle);
		FileHandle = 0;
	}
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
StorageClass *__fastcall StorageClass::OpenStorage(const WCHAR *fileName)
{
	StorageClass *dataStorage = NULL;
	DWORD fileAttributes = GetFileAttributesW(fileName);

	if ((fileName[wcslen(fileName) - 1] == L':'))
	{
		// Имя раздела может быть указано в формате "C:"
		dataStorage = new SimpleStorageClass(fileName);
	}
	else
	{
		// Указано имя файла
		dataStorage = new SimpleStorageClass(fileName);
	}

	if (!dataStorage->Open())
	{
		delete dataStorage;
		dataStorage = NULL;
	}

	return dataStorage;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
StorageClass::StorageClass()
{
	Type = StorageType::ErrorType;
	DataSize = 0;
}
//---------------------------------------------------------------------------
StorageClass::~StorageClass()
{
}
//---------------------------------------------------------------------------
StorageType StorageClass::GetType()
{
	return Type;
}
//---------------------------------------------------------------------------
ULONGLONG StorageClass::GetDataSize()
{
	return DataSize;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
SimpleStorageClass::SimpleStorageClass(SimpleStorageClass *srcStorage) : StorageClass()
{
	Type = srcStorage->Type;
	Fragments = srcStorage->Fragments;
	FragmentIndex = NULL;
	FragmentIndexSize = 0;
	ResetFragments();
}
//---------------------------------------------------------------------------
StorageClass *SimpleStorageClass::GetCopy()
{
	return new SimpleStorageClass(this);
}
//---------------------------------------------------------------------------
SimpleStorageClass::SimpleStorageClass(StorageType newType, vector<StorageFragmentClass> &newFragments) : StorageClass()
{
	Type = newType;
	Fragments = newFragments;
	DataSize = 0;
	FragmentIndex = NULL;
	FragmentIndexSize = 0;

	ResetFragments();
}
//---------------------------------------------------------------------------
SimpleStorageClass::SimpleStorageClass(const WCHAR *fileName) : StorageClass()
{
	wstring tempStr;
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD TotalNumberOfClusters;
	bool result;
	WCHAR *pBracketPosition;

	DataSize = 0;

	// Если последний символ - двоеточие, то имеем дело с логическим диском
	if (fileName[wcslen(fileName) - 1] == L':')
	{
		Type = StorageType::LogicalDrive;

		// Проверяем, есть ли в имени раздела ведущие "\\.\"
		if (wcscmp(fileName, L"\\\\.\\") == 0)
		{
			// Имя раздела в формате "\\.\X:"
			tempStr = wstring(fileName);
		}
		else
		{
			// Имя раздела предположительно в формате X:
			tempStr = wstring(L"\\\\.\\");
			tempStr.append(wstring(fileName));
		}

		// Определить размер раздела и отобразить его
		result = GetDiskFreeSpaceW(
				&fileName[wcslen(fileName) - 2], // address of root path
				&SectorsPerCluster,							 // address of sectors per cluster
				&BytesPerSector,								 // address of bytes per sector
				NULL,														 // address of number of free clusters
				&TotalNumberOfClusters					 // address of total number of clusters
		);

		if (result)
		{
			Fragments.push_back(StorageFragmentClass(tempStr.c_str(), 0, (ULONGLONG)TotalNumberOfClusters * (ULONGLONG)SectorsPerCluster * (ULONGLONG)BytesPerSector));
		}
		else
		{
			Type = StorageType::ErrorType;
		}
	}
	else
	{
		// Предполагаем, что указано имя файла
		Type = StorageType::ImageFile;
		const wchar_t *pos = wcschr(fileName, '[');
		pBracketPosition = (wchar_t *)pos;
		if (pBracketPosition)
			tempStr = wstring(fileName, pBracketPosition - fileName);
		else
			tempStr = wstring(fileName);
		ULONGLONG dataSize = GetFileSize(fileName);
		// wcout << "Storage. Open " << fileName << " . size: " << dataSize << "\n";
		Fragments.push_back(StorageFragmentClass(fileName, 0, dataSize));
	}

	FragmentIndex = NULL;
	FragmentIndexSize = 0;

	ResetFragments();
}
//---------------------------------------------------------------------------
SimpleStorageClass::~SimpleStorageClass()
{
	Close();
	ClearFragments();
}
//---------------------------------------------------------------------------
WORD SimpleStorageClass::GetBaseFilePath(WCHAR *filePath)
{
	return Fragments[0].GetFileName(filePath);
}
//---------------------------------------------------------------------------
void SimpleStorageClass::ResetFragments()
{
	for (vector<StorageFragmentClass>::iterator storageFragment = Fragments.begin(); storageFragment != Fragments.end(); storageFragment++)
	{
		storageFragment->ResetFileHandle();
	}
}
//---------------------------------------------------------------------------
void SimpleStorageClass::ClearFragments()
{
	// Очистить текущую информацию о фрагментах
	Fragments.clear();
	DataSize = 0;
}
//---------------------------------------------------------------------------
bool SimpleStorageClass::Open()
{
	if (Type == StorageType::ErrorType || Fragments.size() == 0)
		return false;

	// Если носитель открыт, его необходимо закрыть
	if (DataSize)
		Close();

	for (vector<StorageFragmentClass>::iterator storageFragment = Fragments.begin(); storageFragment != Fragments.end(); storageFragment++)
	{
		storageFragment->Open();
		DataSize += storageFragment->GetDataSize();
	}

	return PrepareFragmentIndex();
}
//---------------------------------------------------------------------------
void SimpleStorageClass::Close()
{
	if (FragmentIndexSize)
		ClearFragmentIndex();

	for (vector<StorageFragmentClass>::iterator storageFragment = Fragments.begin(); storageFragment != Fragments.end(); storageFragment++)
	{
		storageFragment->Close();
	}

	DataSize = 0;
}
//---------------------------------------------------------------------------
bool SimpleStorageClass::PrepareFragmentIndex()
{
	if (!DataSize)
		return false;

	FragmentIndexSize = Fragments.size();
	FragmentIndex = new StorageFragmentStruct[FragmentIndexSize];

	int i = 0;

	for (vector<StorageFragmentClass>::iterator storageFragment = Fragments.begin(); storageFragment != Fragments.end(); storageFragment++, i++)
	{
		FragmentIndex[i].FileHandle = storageFragment->GetFileHandle();
		FragmentIndex[i].StartOffset = storageFragment->GetStartOffset();
		FragmentIndex[i].EndOffset = FragmentIndex[i].StartOffset + storageFragment->GetDataSize();
	}

	return true;
}
//---------------------------------------------------------------------------
void SimpleStorageClass::ClearFragmentIndex()
{
	delete[] FragmentIndex;
	FragmentIndex = NULL;
	FragmentIndexSize = 0;
}
//---------------------------------------------------------------------------
DWORD SimpleStorageClass::ReadDataByOffset(ULONGLONG startOffset, DWORD bytesToRead, BYTE *dataBuffer, DWORD *leftToRead)
{
	LARGE_INTEGER sectorOffset;
	ULONG result;
	DWORD bytesRead;

	// Проверяем, создан ли индекс
	if (!FragmentIndexSize)
		return 0;

	if (FragmentIndexSize == 1) // Количество фрагментов равно 1, чтение в простом режиме
	{
		if (!FragmentIndex[0].FileHandle)
			return 0;

		sectorOffset.QuadPart = startOffset;

		// Задать позицию в файле
		result = SetFilePointer(
				FragmentIndex[0].FileHandle,
				sectorOffset.LowPart,
				&sectorOffset.HighPart,
				FILE_BEGIN);

		if (result != sectorOffset.LowPart)
			return 0;

		// Считать
		ReadFile(
				FragmentIndex[0].FileHandle,
				dataBuffer,
				bytesToRead,
				&bytesRead,
				NULL);

		if (leftToRead != NULL)
			*leftToRead = bytesToRead - bytesRead;
		return bytesRead;
	}
	else
	{
		if (leftToRead != NULL)
			*leftToRead = bytesToRead;
		return 0;
	}
}
//---------------------------------------------------------------------------
//FileSystemTypeEnum RecognizeFileSystem(const BYTE *buffer, DWORD bufferSize)
//{
//	// Ограничим минимальный размер файловой системы (2048 байт)
//	if (bufferSize < 0x800)
//		return FileSystemTypeEnum::FS_None;
//
//	// Возможно, это раздел HFS+
//	WORD bootRecordSignature;
//	memcpy(&bootRecordSignature, &buffer[0x400], 2);
//
//	if (bootRecordSignature == ByteReverse16(0x482B))
//	{
//		return FileSystemTypeEnum::HFSP;
//	}
//
//	return FileSystemTypeEnum::FS_None;
//}
//---------------------------------------------------------------------------
