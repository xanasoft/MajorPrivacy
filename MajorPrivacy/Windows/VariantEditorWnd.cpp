#include "pch.h"
#include "VariantEditorWnd.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include <QDateTimeEdit>
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../Library/Common/FileIO.h"
#include "../Library/API/PrivacyAPI.h"

QString VariantTypeToString(uint8_t type)
{
	switch (type) {
	case VAR_TYPE_EMPTY:
		return "Empty";
	case VAR_TYPE_MAP:
		return "Dictionary";
	case VAR_TYPE_LIST:
		return "List";
	case VAR_TYPE_INDEX:
		return "Index";
	case VAR_TYPE_BYTES:
		return "Binary BLOB";
	case VAR_TYPE_ASCII:
		return "String ASCII";
	case VAR_TYPE_UTF8:
		return "String UTF8";
	case VAR_TYPE_UNICODE:
		return "String UTF16";
	case VAR_TYPE_UINT:
		return "Unsigned Int";
	case VAR_TYPE_SINT:
		return "Signed Int";
	case VAR_TYPE_DOUBLE:
		return "Floating";
	case VAR_TYPE_CUSTOM:
		return "Custom";
	default:
		return "Unknown Type";
	}
}

QString VariantFormatInt(const QtVariant& Value)
{
	QString Number = ((QtVariant*)&Value)->AsQStr();
	size_t Size = Value.GetSize();
	if (Size <= 4) {
		union {
			uint32 Index;
			uint8 Chars[4];
		};
		Index = Value.To<uint32>();

		size_t i = 0;
		for (; i < 4; i++) {
			if (Chars[i] < 32 || Chars[i] >= 127)
				break;
		}

		if (i >= 2) {
			QString Id = QString::fromLatin1((char*)&Index, sizeof(Index));
			std::reverse(Id.begin(), Id.end());
			return QString("%1 ('%2')").arg(Number).arg(Id);
		}
	}
	return Number;
}

QString VariantIndexToNameEx(uint32 Index);

CVariantEditorWnd::CVariantEditorWnd(QWidget *parent)
:QMainWindow(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	m_ReadOnly = false;

	m_pFileMenu = menuBar()->addMenu(tr("File"));
		m_pFileMenu->addAction(tr("Open File"), this, SLOT(OpenFile()));
		//m_pFileMenu->addAction(tr("Save File"), this, SLOT(SaveFile()));
		m_pFileMenu->addSeparator();
		m_pFileMenu->addAction(tr("Close Editor"), this, SLOT(close()));


	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainWidget->setLayout(m_pMainLayout);
	setCentralWidget(m_pMainWidget);

	/*m_pPropertiesTree = new QTreeWidget();
	m_pPropertiesTree->setHeaderLabels(tr("Key|Value|Type").split("|"));
	//m_pPropertiesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);*/
	m_pPropertiesTree = new QTreeView();
	m_pVariantModel = new CVariantModel();
	m_pPropertiesTree->setModel(m_pVariantModel);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pPropertiesTree->setStyle(pStyle);
	m_pPropertiesTree->setSortingEnabled(true);
	m_pPropertiesTree->setUniformRowHeights(true); // critical for good performance with huge data sets
	
	m_pPropertiesTree->setExpandsOnDoubleClick(false);
	connect(m_pPropertiesTree, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));

	m_pPropertiesTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pPropertiesTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	m_pMenu = new QMenu();

	m_pAddRoot = new QAction(tr("Add to Root"), m_pMenu);
	connect(m_pAddRoot, SIGNAL(triggered()), this, SLOT(OnAddRoot()));
	m_pMenu->addAction(m_pAddRoot);

	m_pAdd = new QAction(tr("Add"), m_pMenu);
	connect(m_pAdd, SIGNAL(triggered()), this, SLOT(OnAdd()));
	m_pMenu->addAction(m_pAdd);

	m_pRemove = new QAction(tr("Remove"), m_pMenu);
	connect(m_pRemove, SIGNAL(triggered()), this, SLOT(OnRemove()));
	m_pMenu->addAction(m_pRemove);

	m_pMainLayout->addWidget(m_pPropertiesTree);

	/*if(bWithButtons)
	{
		m_pButtons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Reset, Qt::Horizontal, this);
		QObject::connect(m_pButtons, SIGNAL(clicked(QAbstractButton *)), this, SLOT(OnClicked(QAbstractButton*)));
		m_pMainLayout->addWidget(m_pButtons);
	}
	else
		m_pButtons = NULL;*/
	
	setWindowState(Qt::WindowNoState);
	restoreGeometry(theConf->GetBlob("VariantEditor/Window_Geometry"));
	restoreState(theConf->GetBlob("VariantEditor/Window_State"));

	m_pPropertiesTree->header()->restoreState(theConf->GetBlob("VariantEditor/AccessView_Columns"));
}

CVariantEditorWnd::~CVariantEditorWnd()
{
	theConf->SetBlob("VariantEditor/AccessView_Columns",m_pPropertiesTree->header()->saveState());

	theConf->SetBlob("VariantEditor/Window_Geometry", saveGeometry());
	theConf->SetBlob("VariantEditor/Window_State", saveState());
}

void CVariantEditorWnd::ExpandRecursively(const QModelIndex& index, quint64 TimeOut)
{
	if (TimeOut < GetTickCount64())
		return;

	m_pPropertiesTree->expand(index);

	int rowCount = m_pVariantModel->rowCount(index);
	for (int i = 0; i < rowCount; ++i) {
		QModelIndex childIndex = m_pVariantModel->index(i, 0, index);
		ExpandRecursively(childIndex, TimeOut);
	}
}

void CVariantEditorWnd::OnDoubleClicked(const QModelIndex& index)
{
	if(!m_pPropertiesTree->isExpanded(index))
		ExpandRecursively(index, GetTickCount64() + 10*1000);
	else
		m_pPropertiesTree->collapse(index);
}

void CVariantEditorWnd::OpenFile()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Open File"), theCore->GetConfigDir(), tr("All Files (*.dat)"));
	if (FileName.isEmpty())
		return;

	CBuffer Buffer;
	if (!ReadFile(FileName.toStdWString(), Buffer)) {
		QMessageBox::critical(this, tr("Error"), tr("Can't open file"));
		return;
	}

	//QtVariant Data;
	//try {
	auto ret = m_Root.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != QtVariant::eErrNone) {
		QMessageBox::critical(this, tr("Error"), tr("Can't parse file"));
		return;
	}

	//WriteProperties(Data);
	m_pVariantModel->Update(m_Root);
}

void CVariantEditorWnd::SaveFile()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Save File"), "", tr("All Files (*.dat)"));
	if (FileName.isEmpty())
		return;

	CBuffer Buffer;
	m_Root.ToPacket(&Buffer);
	WriteFile(FileName.toStdWString(), Buffer);
}

void CVariantEditorWnd::OnMenuRequested(const QPoint &point)
{
	if(m_ReadOnly)
		return;
	m_pMenu->popup(QCursor::pos());	
}


//////////////////////////////////////////////////////////////////////////
// CVariantModel


CVariantModel::CVariantModel(QObject* parent) 
	: CAbstractTreeModel(parent) 
{
}

void CVariantModel::Update(const QtVariant& pRoot)
{
	CAbstractTreeModel::Clean();
	CAbstractTreeModel::Update(&pRoot);
}

CAbstractTreeModel::SAbstractTreeNode* CVariantModel::MkNode(const void* data, const QVariant& key, SAbstractTreeNode* parent)
{
	SVariantNode* pNode = new SVariantNode;
	pNode->data = *(QtVariant*)data;
	pNode->key = key;
	pNode->parentNode = parent;
	pNode->values.resize(columnCount());
	updateNodeData(pNode, data, QModelIndex());
	return pNode;
}

void CVariantModel::ForEachChild(SAbstractTreeNode* parentNode, const std::function<void(const QVariant& key, const void* data)>& func)
{
	const QtVariant Value = ((SVariantNode*)parentNode)->data;

	switch (Value.GetType())
	{
	case VAR_TYPE_MAP: {
		for (uint32 i = 0; i < Value.Count(); i++)
		{
			const char* Key = Value.Key(i);
			func(QString(Key), Value[Key].Ptr());
		}
		break;
	}
	case VAR_TYPE_LIST: {
		for (uint32 i = 0; i < Value.Count(); i++)
		{
			func(QString::number(i), Value[i].Ptr());
		}
		break;
	}
	case VAR_TYPE_INDEX: {
		for (uint32 i = 0; i < Value.Count(); i++)
		{
			uint32 Id = Value.Id(i);
			//QString Name = QString::fromLatin1((char*)&Id, 4);
			//std::reverse(Name.begin(), Name.end());
			//func(Name, Value[Id].Ptr());
			func(Id, Value[Id].Ptr());
		}
		break;
	}
	}
}

void CVariantModel::updateNodeData(SAbstractTreeNode* node, const void* data, const QModelIndex& nodeIndex)
{
	SVariantNode* pNode = (SVariantNode*)node;
	pNode->data = *(QtVariant*)data;

	int Col = 0;
	bool State = false;
	int Changed = 0;

	for (int section = 0; section < columnCount(); section++)
	{
		if (!IsColumnEnabled(section))
			continue; // ignore columns which are hidden

		QVariant Value;
		switch (section)
		{
		case eName:			Value = pNode->key; break;
		case eType:			Value = pNode->data.GetType(); break;
		case eValue:		Value = ((QtVariant*)&pNode->data)->AsQStr(); break;
		}

		SAbstractTreeNode::SValue& ColValue = pNode->values[section];

		if (ColValue.Raw != Value)
		{
			if (Changed == 0)
				Changed = 1;
			ColValue.Raw = Value;

			switch (section)
			{
				case eName: ColValue.Formatted = pNode->key.type() == QVariant::UInt ? VariantIndexToNameEx(Value.toUInt()) : Value; break;
				case eType: ColValue.Formatted = VariantTypeToString(pNode->data.GetType()); break;
				case eValue: ColValue.Formatted = (pNode->data.GetType() == VAR_TYPE_UINT || pNode->data.GetType() == VAR_TYPE_SINT) ? VariantFormatInt(pNode->data) : Value; break;
			}
		}

		if (State != (Changed != 0))
		{
			if (State && nodeIndex.isValid())
				emit dataChanged(createIndex(nodeIndex.row(), Col, pNode), createIndex(nodeIndex.row(), section - 1, pNode));
			State = (Changed != 0);
			Col = section;
		}
		if (Changed == 1)
			Changed = 0;
	}
	if (State && nodeIndex.isValid())
		emit dataChanged(createIndex(nodeIndex.row(), Col, pNode), createIndex(nodeIndex.row(), columnCount() - 1, pNode));

	// Check if hasChildren() status has changed
	bool hadChildren = node->childCount > 0;
	bool hasChildrenNow = pNode->data.Count() > 0;

	node->childCount = pNode->data.Count();

	// Emit dataChanged() if necessary to update the expand/collapse indicator
	if (hadChildren != hasChildrenNow && nodeIndex.isValid())
		emit dataChanged(nodeIndex, nodeIndex);
}

const void* CVariantModel::childData(SAbstractTreeNode* node, const QVariant& key)
{
	QtVariant Value = ((SVariantNode*)node)->data;

	switch (Value.GetType())
	{
	case VAR_TYPE_MAP: {
		return Value[key.toString().toStdString().c_str()].Ptr();
	}
	case VAR_TYPE_LIST: {
		return Value[key.toUInt()].Ptr();
	}
	case VAR_TYPE_INDEX: {
		//QString Name = key.toString();
		//std::reverse(Name.begin(), Name.end());
		//uint32 Id = *(uint32*)Name.toStdString().c_str();
		//return Value[Id].Ptr();
		return Value[key.toUInt()].Ptr();
	}
	}
	return NULL;
}

QVariant CVariantModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName: return tr("Name");
		case eType: return tr("Type");
		case eValue: return tr("Value");
		}
	}
	return QVariant();
}


//////////////////////////////////////////////////////////////////////////
// 


QString VariantIndexToName(uint32 Index)
{
	switch (Index)
	{
		////////////////////////////
		// Config
	case API_V_CONFIG:
		return "Config";
	case API_V_VERSION:
		return "Version";

	case API_V_KEY:
		return "Key";
	case API_V_VALUE:
		return "Value";
	case API_V_DATA:
		return "Data";
	case API_V_FOLDER:
		return "Folder";
	case API_V_ENTRY:
		return "Entry";

		//
	case API_V_GUI_CONFIG:
		return "GUI Config";
	case API_V_DRIVER_CONFIG:
		return "Driver Config";
	case API_V_USER_KEY:
		return "User Key";
	case API_V_SERVICE_CONFIG:
		return "Service Config";
	case API_V_PROGRAM_RULES:
		return "Program Rules";
	case API_V_ACCESS_RULES:
		return "Access Rules";
	case API_V_FW_RULES:
		return "Firewall Rules";
	case API_V_PROGRAMS:
		return "Programs";
	case API_V_LIBRARIES:
		return "Libraries";


		////////////////////////////
		// Crypto & Protection
	case API_V_PUB_KEY:
		return "Public Key";
	case API_V_PRIV_KEY:
		return "Private Key";
	case API_V_HASH:
		return "Hash";
	case API_V_KEY_BLOB:
		return "Key Blob";
	case API_V_LOCK:
		return "Lock";
	case API_V_UNLOCK:
		return "Unlock";
	case API_V_RAND:
		return "Random";
	case API_V_SIGNATURE:
		return "Signature";


		////////////////////////////
		// Generic Info

	case API_V_NAME:
		return "Name";
	case API_V_ICON:
		return "Icon";
	case API_V_INFO:
		return "Info";
	case API_V_RULE_GROUP:
		return "Rule Group";
	case API_V_RULE_DESCR:
		return "Rule Description";
	case API_V_CMD_LINE:
		return "Command Line";


		////////////////////////////
		// Generic Fields
	case API_V_ENUM_ALL:
		return "Enum All";
	case API_V_COUNT:
		return "Count";
	case API_V_RELOAD:
		return "Reload";
	case API_V_GET_DATA:
		return "Get Data";


		////////////////////////////
		// Program ID
	case API_V_ID:
		return "Program ID";
	case API_V_IDS:
		return "Program IDs";

	case API_V_PROG_TYPE:
		return "Program Type";
		// ...

	case API_V_FILE_PATH:
		return "File Path";
	case API_V_FILE_NT_PATH:
		return "File NT Path";
	case API_V_SERVICE_TAG:
		return "Service Tag";
	case API_V_APP_SID:
		return "App SID"; // App Container SID
	case API_V_APP_NAME:
		return "App Name"; // App Container Name
	case API_V_PACK_NAME:
		return "Package Name"; // Package Name
	case API_V_REG_KEY:
		return "Registry Key";
	case API_V_PROG_PATTERN:
		return "Program Pattern"; // like API_V_FILE_PATH but with wildcards
	case API_V_OWNER:
		return "Owner"; // used by firewall rules


		////////////////////////////
		// Programs
	case API_V_PROG_ITEMS:
		return "Program Items";

	case API_V_PROG_UID:
		return "Program UID";
	case API_V_PROG_UIDS:
		return "Program UIDs";

		// Status
	case API_V_PROG_ACCESS_COUNT:
		return "Program Access Count";
	case API_V_PROG_SOCKET_REFS:
		return "Program Socket References";
	case API_V_PROG_LAST_EXEC:
		return "Program Last Execution";
	case API_V_PROG_ITEM_MISSING:
		return "Program Item Missing";

		// Mgmt Fields
	case API_V_PROG_PARENT:
		return "Program Parent";
	case API_V_PURGE_RULES:
		return "Purge Rules";
	case API_V_DEL_WITH_RULES:
		return "Delete with Rules";


		////////////////////////////
		// Libraries
	case API_V_LIB_REF:
		return "Library Reference";
	case API_V_LIB_LOAD_TIME:
		return "Library Load Time";
	case API_V_LIB_LOAD_COUNT:
		return "Library Load Count";
	case API_V_LIB_STATUS:
		return "Library Status";


		////////////////////////////
		// Generic Rules
	case API_V_TYPE:
		return "Rule Type";
	case API_V_GUID:
		return "Rule GUID";
	case API_V_INDEX:
		return "Rule Index";
	case API_V_ENABLED:
		return "Rule Enabled";

		// Accompanying Data & Fields
	case API_V_RULE_REF_GUID:
		return "Rule Reference GUID";
	case API_V_RULE_HIT_COUNT:
		return "Rule Hit Count";


		////////////////////////////
		// Access Rules
	case API_V_ACCESS_RULE_ACTION:
		return "Access Rule Action";
		// ...
	case API_V_ACCESS_PATH:
		return "Access Path";
	case API_V_ACCESS_NT_PATH:
		return "Access NT Path";
	case API_V_VOL_RULE:
		return "Volume Rule";

		////////////////////////////
		// Execution Rules
	case API_V_EXEC_RULE_ACTION:
		return "Execution Rule Action";
		// ...
	case API_V_EXEC_SIGN_REQ:
		return "Execution Sign Requirement";
		// ...
	case API_V_EXEC_ON_TRUSTED_SPAWN:
		return "Execution on Trusted Spawn";
		// ...
	case API_V_EXEC_ON_SPAWN:
		return "Execution on Spawn";
		// ...
	case API_V_IMAGE_LOAD_PROTECTION:
		return "Image Load Protection";
		//API_V_PATH_PREFIX,
		//API_V_DEVICE_PATH,

		////////////////////////////
		// Firewall Rules
	case API_V_FW_RULE_ACTION:
		return "Firewall Rule Action";
		// ...
	case API_V_FW_RULE_DIRECTION:
		return "Firewall Rule Direction";
		// ...
	case API_V_FW_RULE_PROFILE:
		return "Firewall Rule Profile";
		// ...
	case API_V_FW_RULE_PROTOCOL:
		return "Firewall Rule Protocol";
	case API_V_FW_RULE_INTERFACE:
		return "Firewall Rule Interface";
		// ...
	case API_V_FW_RULE_LOCAL_ADDR:
		return "Firewall Rule Local Address";
	case API_V_FW_RULE_REMOTE_ADDR:
		return "Firewall Rule Remote Address";
	case API_V_FW_RULE_LOCAL_PORT:
		return "Firewall Rule Local Port";
	case API_V_FW_RULE_REMOTE_PORT:
		return "Firewall Rule Remote Port";
	case API_V_FW_RULE_REMOTE_HOST:
		return "Firewall Rule Remote Host";
	case API_V_FW_RULE_ICMP:
		return "Firewall Rule ICMP";
	case API_V_FW_RULE_OS:
		return "Firewall Rule OS";
	case API_V_FW_RULE_EDGE:
		return "Firewall Rule Edge";


		////////////////////////////
		// Firewall Config
	case API_V_FW_AUDIT_MODE:
		return "Firewall Audit Mode";			// FwAuditPolicy
	case API_V_FW_RULE_FILTER_MODE:
		return "Firewall Rule Filter Mode";		// FwFilterMode
	case API_V_FW_EVENT_STATE:
		return "Firewall Event State";			// FwEventState


		////////////////////////////
		// Process Info
	case API_V_PROCESSES:
		return "Processes"; // Process List
	case API_V_PROCESS_REF:
		return "Process Reference"; // Internal Process Reference

	case API_V_PID:
		return "PID";
	case API_V_PIDS:
		return "PIDs";

	case API_V_CREATE_TIME:
		return "Create Time";
	case API_V_PARENT_PID:
		return "Parent PID";
	case API_V_CREATOR_PID:
		return "Creator PID"; // API_V_EVENT_ACTOR_PID
	case API_V_CREATOR_TID:
		return "Creator TID"; // API_V_EVENT_ACTOR_TID
	case API_V_ENCLAVE:
		return "Enclave";

	case API_V_KPP_STATE:
		return "KPP State";
	case API_V_FLAGS:
		return "Flags";
	case API_V_SFLAGS:
		return "SFlags";

	case API_V_NUM_THREADS:
		return "Number of Threads";
	case API_V_SERVICES:
		return "Services";
	case API_V_HANDLES:
		return "Handles";
	case API_V_SOCKETS:
		return "Sockets";

	case API_V_USER_SID:
		return "User SID";

	case API_V_NUM_IMG:
		return "Number of Images";
	case API_V_NUM_MS_IMG:
		return "Number of MS Images";
	case API_V_NUM_AV_IMG:
		return "Number of AV Images";
	case API_V_NUM_V_IMG:
		return "Number of V Images";
	case API_V_NUM_S_IMG:
		return "Number of S Images";
	case API_V_NUM_U_IMG:
		return "Number of U Images";

	case API_V_UW_REFS:
		return "UW References";


		////////////////////////////
		// Handle Info
	case API_V_HANDLE:
		return "Handle";
	case API_V_HANDLE_TYPE:
		return "Handle Type";


		////////////////////////////
		// Event Logging

		// Program Access Log
	case API_V_ACCESS_LOG:
		return "Access Log";

	case API_V_PROG_RESOURCE_ACCESS:
		return "Program Resource Access";
	case API_V_PROG_EXEC_PARENTS:
		return "Program Execution Actors";
	case API_V_PROG_EXEC_CHILDREN:
		return "Program Execution Targets";
	case API_V_PROG_INGRESS_ACTORS:
		return "Program Ingress Actors";
	case API_V_PROG_INGRESS_TARGETS:
		return "Program Ingress Targets";

		// Resource Access
	case API_V_ACCESS_REF:
		return "Access Reference";
	case API_V_ACCESS_NAME:
		return "Access Name";
	case API_V_ACCESS_NODES:
		return "Access Nodes";

		// Exec/Ingress
	case API_V_EVENT_ACTOR_UID:
		return "Process Event Actor";	// UID
	case API_V_EVENT_TARGET_UID:
		return "Process Event Target";	// UID

		// Network Traffic Log
	case API_V_TRAFFIC_LOG:
		return "Traffic Log";

	case API_V_SOCK_REF:
		return "Socket Reference";		// Internal Socket Reference
	case API_V_SOCK_TYPE:
		return "Socket Type";
	case API_V_SOCK_LADDR:
		return "Socket Local Address";
	case API_V_SOCK_LPORT:
		return "Socket Local Port";
	case API_V_SOCK_RADDR:
		return "Socket Remote Address";
	case API_V_SOCK_RPORT:
		return "Socket Remote Port";
	case API_V_SOCK_STATE:
		return "Socket State";
	case API_V_SOCK_LSCOPE:
		return "Socket Local Scope";
	case API_V_SOCK_RSCOPE:
		return "Socket Remote Scope";
	case API_V_SOCK_LAST_NET_ACT:
		return "Socket Last Network Activity";
	case API_V_SOCK_LAST_ALLOW:
		return "Socket Last Allow";
	case API_V_SOCK_LAST_BLOCK:
		return "Socket Last Block";
	case API_V_SOCK_UPLOAD:
		return "Socket Upload";
	case API_V_SOCK_DOWNLOAD:
		return "Socket Download";
	case API_V_SOCK_UPLOADED:
		return "Socket Uploaded";
	case API_V_SOCK_DOWNLOADED:
		return "Socket Downloaded";
	case API_V_SOCK_RHOST:
		return "Socket Remote Host";


		////////////////////////////
		// Event Info
	case API_V_EVENT_REF:
		return "Event Reference";
	case API_V_EVENT_TYPE:
		return "Event Type";
	case API_V_EVENT_INDEX:
		return "Event Index";
	case API_V_EVENT_DATA:
		return "Event Data";

	case API_V_EVENT_ACTOR_PID:
		return "Event Actor PID";
	case API_V_EVENT_ACTOR_TID:
		return "Event Actor TID";
	case API_V_EVENT_ACTOR_EID:
		return "Event Actor EID";
	case API_V_EVENT_ACTOR_SVC:
		return "Event Actor Service"; // uint32 tag
	case API_V_EVENT_TARGET_PID:
		return "Event Target PID";
	case API_V_EVENT_TARGET_TID:
		return "Event Target TID";
	case API_V_EVENT_TARGET_EID:
		return "Event Target EID";
	case API_V_EVENT_TIME_STAMP:
		return "Event Time Stamp";

	case API_V_OPERATION:
		return "Operation";
	case API_V_ACCESS_MASK:
		return "Access Mask";  // desired access
	case API_V_NT_STATUS:
		return "NT Status"; // NTSTATUS
	case API_V_EVENT_STATUS:
		return "Event Status"; // EEventStatus
	case API_V_IS_DIRECTORY:
		return "Is Directory";
	case API_V_WAS_BLOCKED:
		return "Was Blocked"; // <- API_V_EVENT_STATUS

	case API_V_LAST_ACTIVITY:
		return "Last Activity"; // <-  API_V_EVENT_TIME_STAMP

		// Process Termination Event
	case API_V_EVENT_ECODE:
		return "Event ECode";

		// Process Ingress Event
	case API_V_EVENT_ROLE:
		return "Event Role"; // EExecLogRole
	case API_V_PROC_MISC_ID:
		return "Process Misc ID"; // Program UID or Library UID
	case API_V_THREAD_ACCESS_MASK:
		return "Thread Access Mask"; // desired access
	case API_V_PROCESS_ACCESS_MASK:
		return "Process Access Mask"; // desired access

		// Image Load Event
	case API_V_EVENT_IMG_BASE:
		return "Event Image Base";
	case API_V_EVENT_NO_PROTECT:
		return "Event No Protect";
	case API_V_EVENT_IMG_PROPS:
		return "Event Image Properties";
	case API_V_EVENT_IMG_SEL:
		return "Event Image Selector";
	case API_V_EVENT_IMG_SECT_NR:
		return "Event Image Section Number";

		// Network Firewall Event
	case API_V_FW_ALLOW_RULES:
		return "Firewall Allow Rules";
	case API_V_FW_BLOCK_RULES:
		return "Firewall Block Rules";

		// Event Configuration
	case API_V_SET_NOTIFY_BITMASK:
		return "Set Notify Bitmask";
	case API_V_CLEAR_NOTIFY_BITMASK:
		return "Clear Notify Bitmask";

		////////////////////////////
		// Event Trace Log
	case API_V_LOG_TYPE:
		return "Log Type"; // ETraceLogs
	case API_V_LOG_OFFSET:
		return "Log Offset";
	case API_V_LOG_DATA:
		return "Log Data";


		////////////////////////////
		// CI Info
	case API_V_SIGN_INFO:
		return "Sign Info";
	case API_V_SIGN_FLAGS:
		return "Sign Flags"; // --> API_V_CERT_STATUS
	case API_V_IMG_SIGN_AUTH:
		return "Image Sign Authority";
	case API_V_IMG_SIGN_LEVEL:
		return "Image Sign Level";
	case API_V_IMG_SIGN_POLICY:
		return "Image Sign Policy";
	case API_V_FILE_HASH:
		return "File Hash";
	case API_V_FILE_HASH_ALG:
		return "File Hash Algorithm";
	case API_V_CERT_STATUS:
		return "Certificate Status";
	case API_V_IMG_SIGN_NAME:
		return "Image Sign Name";
	case API_V_IMG_CERT_ALG:
		return "Image Certificate Algorithm";
	case API_V_IMG_CERT_HASH:
		return "Image Certificate Hash";


		////////////////////////////
		// DNS Cache
	case API_V_DNS_CACHE:
		return "DNS Cache";
	case API_V_DNS_CACHE_REF:
		return "DNS Cache Reference";
	case API_V_DNS_HOST:
		return "DNS Host";
	case API_V_DNS_TYPE:
		return "DNS Type";
	case API_V_DNS_ADDR:
		return "DNS Address";
	case API_V_DNS_DATA:
		return "DNS Data";
	case API_V_DNS_TTL:
		return "DNS TTL";
	case API_V_DNS_STATUS:
		return "DNS Status";
	case API_V_DNS_QUERY_COUNT:
		return "DNS Query Count";
	case API_V_DNS_RULES:
		return "DNS Rules";
	case API_V_DNS_RULE_ACTION:
		return "DNS Rule Action";


		////////////////////////////
		// Volume
	case API_V_VOLUMES:
		return "Volumes";
	case API_V_VOL_REF:
		return "Volume Reference";
	case API_V_VOL_PATH:
		return "Volume Path";
	case API_V_VOL_DEVICE_PATH:
		return "Volume Device Path";
	case API_V_VOL_MOUNT_POINT:
		return "Volume Mount Point";
	case API_V_VOL_SIZE:
		return "Volume Size";
	case API_V_VOL_PASSWORD:
		return "Volume Password";
	case API_V_VOL_PROTECT:
		return "Volume Protect";
	case API_V_VOL_CIPHER:
		return "Volume Cipher";
	case API_V_VOL_OLD_PASS:
		return "Volume Old Password";
	case API_V_VOL_NEW_PASS:
		return "Volume New Password";


		////////////////////////////
		// Tweaks
	case API_V_TWEAKS:
		return "Tweaks";
	case API_V_TWEAK_HINT:
		return "Tweak Hint";
		// ...
	case API_V_TWEAK_STATUS:
		return "Tweak Status";
		// ...
	case API_V_TWEAK_LIST:
		return "Tweak List";
	case API_V_TWEAK_TYPE:
		return "Tweak Type";
		// ...
	case API_V_TWEAK_IS_SET:
		return "Tweak Is Set";


		////////////////////////////
		// Support
	case API_V_SUPPORT_NAME:
		return "Support Name";
	case API_V_SUPPORT_STATUS:
		return "Support Status";
	case API_V_SUPPORT_HWID:
		return "Support HWID";


		////////////////////////////
		// Progress Info
	case API_V_PROGRESS_FINISHED:
		return "Progress Finished";
	case API_V_PROGRESS_TOTAL:
		return "Progress Total";
	case API_V_PROGRESS_DONE:
		return "Progress Done";


		////////////////////////////
		// Basic
	case API_V_CACHE_TOKEN:
		return "Cache Token";
	case API_V_ERR_CODE:
		return "Error Code";
	case API_V_ERR_MSG:
		return "Error Message";

	default:
		return "";
	}
}

QString VariantIndexToNameEx(quint32 Index)
{
	QString Id = QString::fromLatin1((char*)&Index, sizeof(Index));
	std::reverse(Id.begin(), Id.end());
	
	QString Name = VariantIndexToName(Index);
	if (Name.isEmpty())
		return QString("'%1'").arg(Id);
	return QString("%1 ('%2')").arg(Name).arg(Id);
}
