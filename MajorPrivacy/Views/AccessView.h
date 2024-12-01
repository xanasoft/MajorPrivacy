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

	void					Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QString& RootPath = QString());

	QList<SAccessItemPtr>	GetSelectedItemsWithChildren() override
	{
		std::function<void(const SAccessItemPtr&, QList<SAccessItemPtr>&)> AddWitnChildren = [&](const SAccessItemPtr& pItem, QList<SAccessItemPtr>& List) {
			List.append(pItem);
			foreach(const SAccessItemPtr & pChild, pItem->Branches)
				AddWitnChildren(pChild, List);
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

	void					CleanUpTree();

	void					OnCleanUpDone();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbAccess;

	QToolButton*			m_pBtnCleanUp;

	QSet<CProgramFilePtr>			m_CurPrograms;
	QSet<CWindowsServicePtr>		m_CurServices;
	SAccessItemPtr					m_CurRoot;
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
