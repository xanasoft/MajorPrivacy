#pragma once

//QVariantMap ResolveShortcut(const QString& LinkPath);

QPixmap LoadWindowsIcon(const QString& Path, quint32 Index);

QIcon LoadWindowsIconEx(const QString& Path, quint32 Index = 0);

bool PickWindowsIcon(QWidget* pParent, QString& Path, quint32& Index);

void WindowsMoveFile(const QString& From, const QString& To);