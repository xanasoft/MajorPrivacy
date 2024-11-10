#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/AccessModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/WindowsService.h"

class CAccessView : public CPanelViewEx<CAccessModel>
{
	Q_OBJECT

public:
	CAccessView(QWidget *parent = 0);
	virtual ~CAccessView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbAccess;

	QSet<CProgramFilePtr>			m_CurPrograms;
	QSet<CWindowsServicePtr>		m_CurServices;
#ifndef USE_ACCESS_TREE
	QHash<QString, SAccessItemPtr>	m_CurAccess;
#else
	SAccessItemPtr					m_CurRoot;	
#endif
	struct SItem
	{
		qint64 LastAccess = -1;
		struct SInt64 {
			qint64 Value = -1;
		};
		QHash<quint64, SInt64> Items;
	};
	QHash<quint64, SItem>			m_CurItems;

	int						m_iAccessFilter = 0;
	quint64					m_RecentLimit = 0;
};
