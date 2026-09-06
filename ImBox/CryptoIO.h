#pragma once
#include "AbstractIO.h"

class CCryptoIO : public CAbstractIO
{
public:
	CCryptoIO(CAbstractIO* pIO, const struct SPassword* pass, const std::wstring& Cipher = std::wstring());
	virtual ~CCryptoIO();

	virtual ULONG64 GetAllocSize() const { return m_pIO->GetAllocSize(); }
	virtual ULONG64 GetDiskSize() const;
	virtual ULONG GetSectorSize() const { return m_pIO->GetSectorSize(); }
	virtual bool CanBeFormated() const;

	virtual int Init();
	virtual void PrepViewOfFile(BYTE* p) { m_pIO->PrepViewOfFile(p); }
	virtual int ChangePassword(const struct SPassword* new_pass);

	virtual bool DiskWrite(void* buf, int size, __int64 offset);
	virtual bool DiskRead(void* buf, int size, __int64 offset);
	virtual void TrimProcess(DEVICE_DATA_SET_RANGE* range, int n);

	static int BackupHeader(CAbstractIO* pIO, const std::wstring& Path, const struct SPassword* pass);
	static int RestoreHeader(CAbstractIO* pIO, const std::wstring& Path, const struct SPassword* pass); 

	virtual void SetDataSection(struct SSection* pSection);

	//virtual int SetData(const UCHAR* pData, SIZE_T uSize);
	//virtual int GetData(UCHAR* pData, SIZE_T* pSize);

protected:
	virtual int InitCrypto();
	virtual int WriteHeader(struct _dc_header* header);

	struct SCryptoIO* m;

public:
	CAbstractIO* m_pIO;
};

