#pragma once

enum class EPathType
{
	eAuto = 0,
	eNative,
	eWin32,
	eDisplay
};

class CFilePath
{
public:
	CFilePath() {}
	//CFilePath(const QString& Path) { Set(Path); }

	bool Set(QString Path, EPathType Type = EPathType::eAuto);
	void Set(const QString& NtPath, const QString& DosPath) { m_NtPath = NtPath; m_DosPath = DosPath; }

	QString Get(EPathType Type) const;

	bool IsEmpty() const { return m_NtPath.isEmpty(); }

protected:
	QString m_NtPath;
	QString m_DosPath;
};
