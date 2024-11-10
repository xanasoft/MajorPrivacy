#include "pch.h"
#include "AccessInfoView.h"
#include "AccessView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../Library/Helpers/NtUtil.h"
#include <QFileIconProvider>
#include "../MiscHelpers/Common/CustomStyles.h"
#include "..\Core\Access\ResLogEntry.h"

CAccessInfoView::CAccessInfoView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pInfo = new CPanelWidgetEx();
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pInfo->GetView()->setStyle(pStyle);
	m_pInfo->GetView()->setItemDelegate(new CTreeItemDelegate());
	m_pInfo->GetTree()->setHeaderLabels(tr("Program|Last Access|Access|Status").split("|"));
	m_pMainLayout->addWidget(m_pInfo);

	QByteArray Columns = theConf->GetBlob("MainWindow/AccessView_Columns");
	if (Columns.isEmpty()) {
		m_pInfo->GetTree()->setColumnWidth(0, 300);
	} else
		m_pInfo->GetTree()->header()->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pBtnExpand = new QToolButton();
	m_pBtnExpand->setIcon(QIcon(":/Icons/Expand.png"));
	m_pBtnExpand->setCheckable(true);
	m_pBtnExpand->setToolTip(tr("Auto Expand"));
	m_pBtnExpand->setMaximumHeight(22);
	connect(m_pBtnExpand, &QToolButton::toggled, this, [&](bool checked) {
		if(checked)
			m_pInfo->GetTree()->expandAll();
		else
			m_pInfo->GetTree()->collapseAll();
	});
	m_pToolBar->addWidget(m_pBtnExpand);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pInfo->GetFinder()->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);
}

CAccessInfoView::~CAccessInfoView()
{
	theConf->SetBlob("MainWindow/AccessView_Columns", m_pInfo->GetTree()->header()->saveState());
}

void CAccessInfoView::OnSelectionChanged()
{
	CAccessView* pView = (CAccessView*)sender();

	QList<SAccessItemPtr> Items = pView->GetSelectedItemsWithChildren();

	QMap<CProgramItemPtr, QList<SAccessStatsPtr>> Map;
	for (auto pItem : Items) {
		for(auto I = pItem->Stats.begin(); I != pItem->Stats.end(); ++I)
			Map[I.key()].append(I.value());
	}

	m_pInfo->GetTree()->clear();

	QFileIconProvider IconProvider;

	QList<QTreeWidgetItem*> ItemsList;
	for(auto I = Map.begin(); I != Map.end(); ++I)
	{
		QTreeWidgetItem* pEntry = new QTreeWidgetItem();
		pEntry->setText(eName, I.key()->GetNameEx());
		pEntry->setIcon(eName, I.key()->GetIcon());
		for(auto J = I.value().begin(); J != I.value().end(); ++J)
		{
			QTreeWidgetItem* pSubEntry = new QTreeWidgetItem();
			pSubEntry->setText(eName, (*J)->Path.Get(EPathType::eDisplay));
			pSubEntry->setIcon(eName, IconProvider.icon(QFileInfo((*J)->Path.Get(EPathType::eDisplay))));
			if((*J)->LastAccessTime)
				pSubEntry->setText(eLastAccess, QDateTime::fromMSecsSinceEpoch(FILETIME2ms((*J)->LastAccessTime)).toString("dd.MM.yyyy hh:mm:ss.zzz"));
			pSubEntry->setText(eAccess, CResLogEntry::GetAccessStr((*J)->AccessMask));
			pSubEntry->setText(eStatus, QString("0x%1 (%2)").arg((*J)->NtStatus, 8, 16, QChar('0')).arg((*J)->bBlocked ? tr("Blocked") : tr("Allowed")));
			pEntry->addChild(pSubEntry);
		}
		pEntry->setText(eAccess, QString::number(I.value().count()));
		ItemsList.append(pEntry);
	}
	m_pInfo->GetTree()->addTopLevelItems(ItemsList);

	if(m_pBtnExpand->isChecked())
		m_pInfo->GetTree()->expandAll();
}