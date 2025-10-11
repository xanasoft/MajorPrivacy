#include "pch.h"
#include "InfoView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"

CInfoView::CInfoView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pInfo = new CPanelWidgetEx();
	//m_pInfo->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pInfo->GetTree()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pInfo->GetTree()->setHeaderLabels(tr("Info").split("|"));
	m_pMainLayout->addWidget(m_pInfo);


	QByteArray Columns = theConf->GetBlob("MainWindow/ProgramInfoView_Columns");
	if (Columns.isEmpty()) {
		m_pInfo->GetTree()->setColumnWidth(0, 300);
	} else
		m_pInfo->GetTree()->header()->restoreState(Columns);
}

CInfoView::~CInfoView()
{
	theConf->SetBlob("MainWindow/ProgramInfoView_Columns", m_pInfo->GetTree()->header()->saveState());
}

void CInfoView::Sync(const QList<CProgramItemPtr>& Items)
{
	m_pInfo->GetTree()->clear();
	
	auto pRoot = theCore->ProgramManager()->GetRoot();

	if(Items.count() != 1)
	{
		m_pInfo->setEnabled(false);
	}
	else
	{
		m_pInfo->setEnabled(true);

		CProgramItemPtr pItem = Items[0];

		QTreeWidgetItem* pName = new QTreeWidgetItem();
		pName->setText(0, tr("Name: %1 (%2)").arg(pItem->GetName()).arg(pItem->GetUID()));
		m_pInfo->GetTree()->addTopLevelItem(pName);

		QTreeWidgetItem* pDosPath = new QTreeWidgetItem();
		pDosPath->setText(0, tr("Path: %1").arg(pItem->GetPath()));
		m_pInfo->GetTree()->addTopLevelItem(pDosPath);

		auto Groups = pItem->GetGroups();

		QTreeWidgetItem* pGroups = new QTreeWidgetItem();
		pGroups->setText(0, tr("Groups (%1)").arg(Groups.count()));
		m_pInfo->GetTree()->addTopLevelItem(pGroups);

		foreach(auto Group, Groups)
		{
			CProgramSetPtr pSet = Group.lock().objectCast<CProgramSet>();
			if(!pSet) continue;
			QTreeWidgetItem* pGroup = new QTreeWidgetItem();
			if(pRoot == pSet)
				pGroup->setText(0, tr("Root (%1)").arg(pSet->GetUID()));
			else
				pGroup->setText(0, tr("%1 (%2)").arg(pSet->GetName()).arg(pSet->GetUID()));
			pGroups->addChild(pGroup);
		}

		m_pInfo->GetTree()->expandAll();
	}
}

void CInfoView::Clear()
{
	m_pInfo->GetTree()->clear();
}