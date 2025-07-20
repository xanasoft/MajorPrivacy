#pragma once
#include "../../Library/Status.h"
#include <QObject>

#define ERROR_OK (1)
#define OP_ASYNC (2)
#define OP_CONFIRM (3)
#define OP_CANCELED (4)

class CAsyncProgress : public QObject
{
	Q_OBJECT
public:
	CAsyncProgress() : m_Status(OP_ASYNC), m_Canceled(false) {}

	void Cancel() { m_Canceled = true; emit Canceled(); }
	bool IsCanceled() { return m_Canceled; }

	void ShowMessage(const QString& text) { emit Message(text);}
	void SetProgress(int value) { emit Progress(value); }
	void Finish(STATUS status) { m_Status = m_Canceled ? ERR(OP_CANCELED) : status; emit Finished(); }

	STATUS GetStatus() { return m_Status; }
	bool IsFinished() { return m_Status.GetStatus() != OP_ASYNC; }

signals:
	//void Progress(int procent);
	void Message(const QString& text);
	void Progress(int value);
	void Canceled();
	void Finished();

protected:
	volatile bool m_Canceled;
	STATUS m_Status;
};

typedef QSharedPointer<CAsyncProgress> CAsyncProgressPtr;

#define PROGRESS CResult<CAsyncProgressPtr>