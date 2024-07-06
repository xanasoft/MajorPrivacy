#include "pch.h"
#include "FilePath.h"
#include <QDebug>
#include "../../Library/Helpers/NtUtil.h"

bool CFilePath::Set(QString Path, EPathType Type)
{
	if (Path.isEmpty()) {
		m_NtPath.clear();
		m_DosPath.clear();
		return true;
	}

	if (Type == EPathType::eAuto){
		if (Path.startsWith("\\??\\")) {
			Path.remove(0, 4);
			Type = EPathType::eWin32;
		}
		else if (Path.length() >= 2 && Path.at(1) == ":")
			Type = EPathType::eWin32;
		else if (Path.startsWith("\\"))
			Type = EPathType::eNative;
		else {
			qDebug() << "Unknown path format: " << Path;
			return false;
		}
	}

	switch (Type)
	{
		case EPathType::eNative: {
			if (m_NtPath != Path) {
				m_NtPath = Path;
				m_DosPath = QString::fromStdWString(NtPathToDosPath(Path.toStdWString()));
			}
			return true;
		}
		case EPathType::eWin32: {
			if (m_DosPath != Path) {
				m_DosPath = Path;
				m_NtPath = QString::fromStdWString(DosPathToNtPath(Path.toStdWString()));
			}
			return true;
		}
	}
	return false;
}

QString CFilePath::Get(EPathType Type) const
{
	switch (Type)
	{
	case EPathType::eNative: return m_NtPath; break;
	case EPathType::eWin32: return m_DosPath; break;
	case EPathType::eDisplay: return !m_DosPath.isEmpty() ? m_DosPath : m_NtPath; break; // todo customize via global settings
	default: return QString();
	}
}