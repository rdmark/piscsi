//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  	Copyright (C) akuker
//
//  	Licensed under the BSD 3-Clause License. 
//  	See LICENSE file in the project root folder.
//
//  	[ SASI hard disk ]
//
//---------------------------------------------------------------------------
#include "sasihd.h"
#include "xm6.h"
#include "fileio.h"
#include "exceptions.h"
#include <sstream>

//===========================================================================
//
//	SASI Hard Disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SASIHD::SASIHD() : Disk("SAHD")
{
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void SASIHD::Reset()
{
	// Unlock, clear attention
	SetLocked(false);
	SetAttn(false);

	// Reset, clear the code
	SetReset(false);
	SetStatusCode(STATUS_NOERROR);
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
void SASIHD::Open(const Filepath& path)
{
	ASSERT(!IsReady());

	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw io_exception("Can't open hard disk file read-only");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	fio.Close();

	// Sector size (default 256 bytes) and number of blocks
	if (!SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 256, true)) {
		stringstream error;
		error << "Invalid sector size " << GetConfiguredSectorSize();
		throw io_exception(error.str());

	}
	SetBlockCount((DWORD)(size >> GetSectorSize()));

	#if defined(REMOVE_FIXED_SASIHD_SIZE)
	if (size % GetSectorSizeInBytes()) {
		stringstream error;
		error << "File size must be a multiple of " << GetSectorSizeInBytes() << " bytes but is " << size << " bytes";
		throw io_exception(error.str());
	}

	// 10MB or more
	if (size < 0x9f5400) {
		throw io_exception("File size must be at least 10 MB");
	}

	// Limit to about 512MB
	if (size > 512 * 1024 * 1024) {
		throw io_exception("File size must not exceed 512 MB");
	}
	#else
	// 10MB, 20MB, 40MBのみ
	switch (size) {
		// 10MB (10441728 BS=256 C=40788)
		case 0x9f5400:
			break;

		// 20MB (20748288 BS=256 C=81048)
		case 0x13c9800:
			break;

		// 40MB (41496576 BS=256 C=162096)
		case 0x2793000:
			break;

		// Other (Not supported )
		default:
			throw io_exception("Unsupported file size");
	}
	#endif	// REMOVE_FIXED_SASIHD_SIZE

	Disk::Open(path);
	FileSupport::SetPath(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SASIHD::Inquiry(const DWORD* /*cdb*/, BYTE* /*buf*/)
{
	SetStatusCode(STATUS_INVALIDCMD);
	return 0;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
int SASIHD::RequestSense(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// Size decision
	int size = (int)cdb[4];
	ASSERT(size >= 0 && size < 0x100);

	// Transfer 4 bytes when size 0 (Shugart Associates System Interface specification)
	if (size == 0) {
		size = 4;
	}

	// SASI fixed to non-extended format
	memset(buf, 0, size);
	buf[0] = (BYTE)(GetStatusCode() >> 16);
	buf[1] = (BYTE)(GetLun() << 5);

	return size;
}
