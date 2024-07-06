#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/IngressModel.h"
#include "../Core/Programs/ProgramFile.h"

class CIngressView : public CPanelViewEx<CIngressModel>
{
	Q_OBJECT

public:
	CIngressView(QWidget *parent = 0);
	virtual ~CIngressView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, bool bAllPrograms);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

protected:

	QSet<CProgramFilePtr>					m_CurPrograms;
	QMap<SIngressKey, SIngressItemPtr>		m_ParentMap;
	QMap<SIngressKey, SIngressItemPtr>		m_IngressMap;
};
