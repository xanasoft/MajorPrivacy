#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/LibraryModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/ProgramLibrary.h"

class CLibraryView : public CPanelViewEx<CLibraryModel>
{
	Q_OBJECT

public:
	CLibraryView(QWidget *parent = 0);
	virtual ~CLibraryView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, bool bAllPrograms);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbGrouping;

	QSet<CProgramFilePtr>					m_CurPrograms;
	QMap<SLibraryKey, SLibraryItemPtr>		m_ParentMap;
	QMap<SLibraryKey, SLibraryItemPtr>		m_LibraryMap;
	bool									m_bGroupByLibrary = false;
};
