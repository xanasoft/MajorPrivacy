#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/IngressModel.h"
#include "../Core/Programs/WindowsService.h"

class CIngressView : public CPanelViewEx<CIngressModel>
{
	Q_OBJECT

public:
	CIngressView(QWidget *parent = 0);
	virtual ~CIngressView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QFlexGuid& EnclaveGuid = QString());

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnCleanUpDone();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbRole;
	QComboBox*				m_pCmbAction;
	QComboBox*				m_pCmbOperation;
	QToolButton*			m_pBtnPrivate;
	QToolButton*			m_pBtnAll;
	QToolButton*			m_pBtnExpand;

	QSet<CProgramFilePtr>					m_CurPrograms;
	QSet<CWindowsServicePtr>				m_CurServices;
	QFlexGuid								m_CurEnclaveGuid;
	QMap<SIngressKey, SIngressItemPtr>		m_ParentMap;
	QMap<SIngressKey, SIngressItemPtr>		m_IngressMap;
	qint32									m_FilterRole = 0;
	qint32									m_FilterAction = 0;
	qint32									m_FilterOperation = 0;
	bool									m_FilterPrivate = true;
	bool									m_FilterAll = false;	
	quint64									m_RecentLimit = 0;
};
