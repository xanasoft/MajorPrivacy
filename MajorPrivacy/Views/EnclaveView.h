#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/EnclaveModel.h"

class CEnclaveView : public CPanelViewEx<CEnclaveModel>
{
	Q_OBJECT

public:
	CEnclaveView(QWidget *parent = 0);
	virtual ~CEnclaveView();

	void					Sync();
	void					Clear();

	QFlexGuid				GetSelectedEnclave();

private slots:
	void					OnAddEnclave();
	void					OnEnclaveAction();
	void					OnProcessAction();

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	virtual void			OnDoubleClicked(const QModelIndex& Index) override;

	void					OpenEnclaveDialog(const CEnclavePtr& pEnclave);

private:

	QToolBar*				m_pToolBar;
	QToolButton*			m_pBtnAdd;

	QAction*				m_pCreateEnclave;
	QAction*				m_pRunInEnclave;
	QAction*				m_pTerminateAll;
	QAction*				m_pRemoveEnclave;
	QAction*				m_pEditEnclave;
	QAction*				m_pCloneEnclave;

	QMenu*					m_pMenuProcess;
	QAction*				m_pTerminate;
	QAction*				m_pExplore;
	QAction*				m_pProperties;
};
