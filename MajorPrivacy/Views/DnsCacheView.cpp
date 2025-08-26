#include "pch.h"
#include "DnsCacheView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Network/NetworkManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CDnsCacheView::CDnsCacheView(QWidget *parent)
	:CPanelViewEx<CDnsCacheModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	QByteArray Columns = theConf->GetBlob("MainWindow/DnsCacheView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbStatus = new QComboBox();
	m_pCmbStatus->addItem(QIcon(":/Icons/NoAccess.png"), tr("Any Status"), (qint32)CDnsCacheEntry::eNone);
	m_pCmbStatus->addItem(QIcon(":/Icons/Go.png"), tr("Allowed"), (qint32)CDnsCacheEntry::eAllowed);
	m_pCmbStatus->addItem(QIcon(":/Icons/Disable.png"), tr("Blocked"), (qint32)CDnsCacheEntry::eBlocked);
	m_pCmbStatus->addItem(QIcon(":/Icons/Go2.png"), tr("Cached"), (qint32)CDnsCacheEntry::eCached);
	m_pToolBar->addWidget(m_pCmbStatus);

	m_pCmbType = new QComboBox();
	m_pCmbType->addItem(tr("Any Type"), (qint32)CDnsCacheEntry::EType::ANY);
	m_pCmbType->addItem(tr("A or AAAA"), (qint32)CDnsCacheEntry::EType::A | (((qint32)CDnsCacheEntry::EType::AAAA) << 16));
	m_pCmbType->addItem(tr("A"), (qint32)CDnsCacheEntry::EType::A);
	m_pCmbType->addItem(tr("AAAA"), (qint32)CDnsCacheEntry::EType::AAAA);
	m_pCmbType->addItem(tr("CNAME"), (qint32)CDnsCacheEntry::EType::CNAME);
	m_pCmbType->addItem(tr("MX"), (qint32)CDnsCacheEntry::EType::MX);
	m_pCmbType->addItem(tr("PTR"), (qint32)CDnsCacheEntry::EType::PTR);
	m_pCmbType->addItem(tr("SRV"), (qint32)CDnsCacheEntry::EType::SRV);
	m_pCmbType->addItem(tr("TXT"), (qint32)CDnsCacheEntry::EType::TXT);
	m_pCmbType->addItem(tr("WKS"), (qint32)CDnsCacheEntry::EType::WKS);
	m_pCmbType->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	m_pToolBar->addWidget(m_pCmbType);

	m_pToolBar->addSeparator();

	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Trash.png"));
	m_pBtnClear->setToolTip(tr("Clear System DNS Cache"));
	m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(FlushDnsCache()));
	m_pToolBar->addWidget(m_pBtnClear);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setFixedHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CDnsCacheView::~CDnsCacheView()
{
	theConf->SetBlob("MainWindow/DnsCacheView_Columns", m_pTreeView->saveState());
}

void CDnsCacheView::Sync()
{
	theCore->NetworkManager()->UpdateDnsCache();
	
	auto EntryMap = theCore->NetworkManager()->GetDnsCache();
	
	CDnsCacheEntry::EStatus Status = (CDnsCacheEntry::EStatus)m_pCmbStatus->currentData().toInt();
	qint32 Type = m_pCmbType->currentData().toInt();

	if (Status != CDnsCacheEntry::eNone || Type != (qint32)CDnsCacheEntry::EType::ANY)
	{
		for (auto I = EntryMap.begin(); I != EntryMap.end();)
		{
			if (Status != CDnsCacheEntry::eNone && I.value()->GetStatus() != Status)
				I = EntryMap.erase(I);
			else if (Type != (qint32)CDnsCacheEntry::EType::ANY)
			{
				quint16 EntryType = I.value()->GetType();
				if ((Type & 0xFFFF) == EntryType || (Type >> 16) == EntryType)
					++I;
				else
					I = EntryMap.erase(I);
			}
			else
				++I;
		}
	}

	m_pItemModel->Sync(EntryMap);
}

/*void CDnsCacheView::OnDoubleClicked(const QModelIndex& Index)
{

}*/

void CDnsCacheView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CDnsCacheView::FlushDnsCache()
{
	theCore->FlushDnsCache();
}