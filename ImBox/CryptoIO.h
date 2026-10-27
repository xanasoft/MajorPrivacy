#pragma once
#include "AbstractIO.h"

#define CRYPTO_SLOTS_DEFAULT (-1) // use KEY_SLOT_COUNT when creating a container

class CCryptoIO : public CAbstractIO
{
public:
	CCryptoIO(CAbstractIO* pIO, const struct SPassword* pass, const std::wstring& Cipher = std::wstring(), int SlotCount = CRYPTO_SLOTS_DEFAULT);
	virtual ~CCryptoIO();

	virtual ULONG64 GetAllocSize() const { return m_pIO->GetAllocSize(); }
	virtual ULONG64 GetDiskSize() const;
	virtual ULONG GetSectorSize() const { return m_pIO->GetSectorSize(); }
	virtual bool CanBeFormated() const;

	virtual int Init();
	virtual void PrepViewOfFile(BYTE* p) { m_pIO->PrepViewOfFile(p); }

	//
	// ChangePassword covers all three key management operations, selected by
	// new_pass->slot and new_pass->size:
	//
	//   slot == 0              change the header password. Every active key
	//                          slot is re-wrapped onto the new header key and
	//                          the salt is kept, so all slot passwords stay
	//                          valid. Without key slots the header is re-sealed
	//                          under a fresh salt.
	//   slot >  0, size >  0   set the password of that key slot.
	//   slot >  0, size == 0   erase that key slot.
	//
	// Editing slots requires the container to have been opened with the header
	// password - a slot password cannot re-wrap another slot.
	//
	virtual int ChangePassword(const struct SPassword* new_pass);

	virtual bool DiskWrite(void* buf, int size, __int64 offset);
	virtual bool DiskRead(void* buf, int size, __int64 offset);
	virtual void TrimProcess(DEVICE_DATA_SET_RANGE* range, int n);

	static int BackupHeader(CAbstractIO* pIO, const std::wstring& Path, const struct SPassword* pass);
	static int RestoreHeader(CAbstractIO* pIO, const std::wstring& Path, const struct SPassword* pass);

	//
	// The header editor's two halves.
	//
	// GetHeader puts the decrypted header into the shared section and reports
	// the volume's parameters alongside it. SetHeader takes one back, sealed
	// under the key the volume is already using, checks that nothing it must
	// not change has changed, and writes it.
	//
	// They are the same pair DiskCryptor's GUI uses against its driver - see
	// dc_backup_header and dc_update_header in driver\misc_volume.c - with the
	// driver's mount-state handling left out, because ImBox only ever works on
	// an unmounted container.
	//
	static int GetHeader(CAbstractIO* pIO, struct SSection* pSection, ULONG uSecSize, const struct SPassword* pass);
	static int SetHeader(CAbstractIO* pIO, struct SSection* pSection, ULONG uSecSize, const struct SPassword* pass);

	virtual void SetDataSection(struct SSection* pSection);

	//virtual int SetData(const UCHAR* pData, SIZE_T uSize);
	//virtual int GetData(UCHAR* pData, SIZE_T* pSize);

protected:
	virtual int InitCrypto();
	virtual int CreateHeader();

	//
	// Seal the header and write it to sector 0.
	//
	// hdr_key != NULL re-seals under a key the caller already holds, which
	// keeps the salt exactly as it is. That is mandatory whenever key slots
	// must survive the write: a slot is unwrapped with a key derived from the
	// header salt, so a new salt orphans every slot.
	//
	// hdr_key == NULL generates a fresh salt and derives the key from
	// m->password, which is what a container without key slots gets.
	//
	virtual int WriteHeader(struct _dc_header* header, struct _xts_key* hdr_key);

	virtual void ReportVolumeInfo(struct _dc_header* header, int enc_len, int kdf, int slot);

	struct SCryptoIO* m;

public:
	CAbstractIO* m_pIO;
};
