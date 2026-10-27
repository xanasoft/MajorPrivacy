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

	void					Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, QString RootPath = QString());
	bool					ShowPrivate() const { return m_pBtnPrivate->isChecked(); }
	void					Clear();

	QList<SAccessItemPtr>	GetSelectedItemsWithChildren() override
	{
		std::function<void(const SAccessItemPtr&, QList<SAccessItemPtr>&)> AddWitnChildren = [&](const SAccessItemPtr& pItem, QList<SAccessItemPtr>& List) {
			List.append(pItem);
			foreach(const SAccessItemPtr & pChild, pItem->Branches) {
				if (m_pItemModel->FilterAccepts(pChild)) // only what the view shows
					AddWitnChildren(pChild, List);
			}
		};

		QList<SAccessItemPtr> List;
		foreach(const QModelIndex & Index, m_pTreeView->selectedRows())
		{
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			SAccessItemPtr pItem = m_pItemModel->GetItem(ModelIndex);
			if (!pItem)
				continue;
			AddWitnChildren(pItem, List);
		}
		return List;
	}

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnRefresh();

	void					CleanUpTree();

	void					OnCleanUpDone();
	void					OnClearRecords();

	void					OnMenuAction();

	void					SetFilter(const QRegularExpression& Exp, int iOptions, int Column);

protected:

	QAction*				m_pCopyPath;

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbAccess;
	QComboBox*				m_pCmbAction;

	QToolButton*			m_pBtnHold;
	QToolButton*			m_pBtnRefresh;
	QToolButton*			m_pBtnPrivate;
	QToolButton*			m_pBtnCleanUp;
	QToolButton*			m_pBtnClear;

	QSet<CProgramFilePtr>			m_CurPrograms;
	QSet<CWindowsServicePtr>		m_CurServices;
	SAccessItemPtr					m_CurRoot;
	struct SItem
	{
		qint64 LastAccess = -1;
		quint64 LastSeq = 0; // newest change of the program item's access log already added
	};
	QHash<quint64, SItem>			m_CurItems;
	quint64							m_LastUpdate = 0;
	bool							m_ForceUpdate = false;

	int						m_iAccessFilter = 0;
	int						m_iStatusFilter = 0;
	quint64					m_RecentLimit = 0;
	QString					m_RootPath;

	bool					m_FullRefresh = false;
	int 					m_RefreshCount = 0;

	int 					m_SlowCount = 0;

	QRegularExpression		m_FilterExp;
	bool					m_bHighLight = false;
	//QTimer*					m_pFilterTimer = nullptr;
	bool					m_FilterChanged = false;
};
