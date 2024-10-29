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
	void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

protected:

	QToolBar*				m_pToolBar;

	QSet<CProgramFilePtr>			m_CurPrograms;
	QSet<CWindowsServicePtr>		m_CurServices;
	QMap<QString, SAccessItemPtr>	m_CurAccess;
};
