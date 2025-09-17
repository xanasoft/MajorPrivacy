#include "pch.h"
#include "SidResolver.h"
#include <QAbstractEventDispatcher>
#include "../../Library/Helpers/WinUtil.h"

CSidResolver::CSidResolver(QObject* parent)
{
	m_bRunning = false;
}

CSidResolver::~CSidResolver()
{
	m_bRunning = false;

	if (!wait(10 * 1000))
		terminate();

	// cleanup unfinished tasks
	foreach(CSidResolverJob* pJob, m_JobQueue)
		pJob->deleteLater();
	m_JobQueue.clear();
}

bool CSidResolver::Init()
{
	return true;
}

QString CSidResolver::GetSidFullName(const QByteArray& Sid, QObject *receiver, const char *member)
{
    if (receiver && !QAbstractEventDispatcher::instance(QThread::currentThread())) {
        qWarning("CSidResolver::GetSidFullName() called with no event dispatcher");
        return "";
    }

	QReadLocker ReadLocker(&m_Mutex);
	QMap<QByteArray, QString>::iterator I = m_Sid2NameCache.find(Sid);
	if (I != m_Sid2NameCache.end())
		return I.value();
	ReadLocker.unlock();

	QMutexLocker Locker(&m_JobMutex);
	if (!m_bRunning)
	{
		m_bRunning = true;
		start();
	}

	CSidResolverJob* &pJob = m_JobQueue[Sid];
	if (!pJob)
	{
		pJob = new CSidResolverJob(Sid);
		pJob->moveToThread(this);
		QObject::connect(pJob, SIGNAL(SidResolved(const QByteArray&, const QString&)), this, SLOT(OnSidResolved(const QByteArray&, const QString&)), Qt::QueuedConnection);
	}
	if (receiver)
	{
		QObject::connect(pJob, SIGNAL(SidResolved(const QByteArray&, const QString&)), receiver, member, Qt::QueuedConnection);
		return tr("Resolving...");
	}
	return tr("Not resolved...");
}

void CSidResolver::OnSidResolved(const QByteArray& Sid, const QString& FullName)
{
	QWriteLocker WriteLocker(&m_Mutex);
	m_Sid2NameCache.insert(Sid, FullName);
}

NTSTATUS PhOpenLsaPolicy(
    _Out_ PLSA_HANDLE PolicyHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PUNICODE_STRING SystemName
)
{
    OBJECT_ATTRIBUTES objectAttributes;

    InitializeObjectAttributes(&objectAttributes, NULL, 0, NULL, NULL);

    return LsaOpenPolicy(
        SystemName,
        &objectAttributes,
        DesiredAccess,
        PolicyHandle
    );
}

LSA_HANDLE PhGetLookupPolicyHandle(
    VOID
)
{
    static LSA_HANDLE cachedLookupPolicyHandle = NULL;
    LSA_HANDLE lookupPolicyHandle;
    LSA_HANDLE newLookupPolicyHandle;

    // Use the cached value if possible.

    lookupPolicyHandle = InterlockedCompareExchangePointer(&cachedLookupPolicyHandle, NULL, NULL);

    // If there is no cached handle, open one.

    if (!lookupPolicyHandle)
    {
        if (NT_SUCCESS(PhOpenLsaPolicy(
            &newLookupPolicyHandle,
            POLICY_LOOKUP_NAMES,
            NULL
        )))
        {
            // We succeeded in opening a policy handle, and since we did not have a cached handle
            // before, we will now store it.

            lookupPolicyHandle = InterlockedCompareExchangePointer(
                &cachedLookupPolicyHandle,
                newLookupPolicyHandle,
                NULL
            );

            if (!lookupPolicyHandle)
            {
                // Success. Use our handle.
                lookupPolicyHandle = newLookupPolicyHandle;
            }
            else
            {
                // Someone already placed a handle in the cache. Close our handle and use their
                // handle.
                LsaClose(newLookupPolicyHandle);
            }
        }
    }

    return lookupPolicyHandle;
}

QString PhGetSidFullName(_In_ PSID Sid, _In_ BOOLEAN IncludeDomain, _Out_opt_ PSID_NAME_USE NameUse)
{
    NTSTATUS status;
    QString fullName;
    LSA_HANDLE policyHandle;
    PLSA_REFERENCED_DOMAIN_LIST referencedDomains;
    PLSA_TRANSLATED_NAME names;

    policyHandle = PhGetLookupPolicyHandle();

    referencedDomains = NULL;
    names = NULL;

    if (NT_SUCCESS(status = LsaLookupSids(
        policyHandle,
        1,
        &Sid,
        &referencedDomains,
        &names
    )))
    {
        if (names[0].Use != SidTypeInvalid && names[0].Use != SidTypeUnknown)
        {
            PWSTR domainNameBuffer;
            ULONG domainNameLength;

            if (IncludeDomain && names[0].DomainIndex >= 0)
            {
                PLSA_TRUST_INFORMATION trustInfo;

                trustInfo = &referencedDomains->Domains[names[0].DomainIndex];
                domainNameBuffer = trustInfo->Name.Buffer;
                domainNameLength = trustInfo->Name.Length;
            }
            else
            {
                domainNameBuffer = NULL;
                domainNameLength = 0;
            }

            if (domainNameBuffer && domainNameLength != 0)
            {
                fullName = QString::fromWCharArray(domainNameBuffer, domainNameLength/sizeof(wchar_t));
                fullName += "\\" + QString::fromWCharArray(names[0].Name.Buffer, names[0].Name.Length/sizeof(wchar_t));
            }
            else
            {
                fullName = QString::fromWCharArray(names[0].Name.Buffer, names[0].Name.Length / sizeof(wchar_t));
            }

            if (NameUse)
            {
                *NameUse = names[0].Use;
            }
        }
        else
        {
            if (NameUse)
            {
                *NameUse = SidTypeUnknown;
            }
        }
    }
    else
    {
        if (NameUse)
        {
            *NameUse = SidTypeUnknown;
        }
    }

    if (referencedDomains)
        LsaFreeMemory(referencedDomains);
    if (names)
        LsaFreeMemory(names);

    return fullName;
}

void CSidResolver::run()
{
#ifdef _DEBUG
	MySetThreadDescription(GetCurrentThread(), L"SID Resolver");
#endif

	int IdleCount = 0;
	while (m_bRunning)
	{
		QMutexLocker Locker(&m_JobMutex);
		if (m_JobQueue.isEmpty()) 
		{
			if (IdleCount++ > 4 * 10) // if we were idle for 10 seconds end the thread
			{
				m_bRunning = false;
				break;
			}
			Locker.unlock();
			QThread::msleep(250);
			continue;
		}
		IdleCount = 0;
		CSidResolverJob* pJob = m_JobQueue.begin().value();
		Locker.unlock();

		QString FullName = PhGetSidFullName((PSID)pJob->m_SID.data(), TRUE, NULL);

		Locker.relock();
		emit pJob->SidResolved(pJob->m_SID, FullName);
		m_JobQueue.take(pJob->m_SID)->deleteLater();
		Locker.unlock();
	}
}
