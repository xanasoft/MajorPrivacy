#pragma once
#include "Tweak.h"
#include "../Library/Status.h"

class CTweakManager : public QObject
{
	Q_OBJECT
public:
	CTweakManager(QObject* parent = nullptr);

	void Update();

	STATUS ApplyTweak(const CTweakPtr& pTweak);
	STATUS UndoTweak(const CTweakPtr& pTweak);

	CTweakPtr GetRoot();
	QMap<QString, CTweakPtr> GetTweaks() const { return m_Map; }

	uint32 GetRevision() const { return m_Revision; }

	int GetAppliedCount() const { return m_AppliedCount; }
	int GetFailedCount() const { return m_FailedCount; }

signals:
	void TweaksChanged();

protected:
	QSharedPointer<CTweakList> m_pRoot;
	QMap<QString, CTweakPtr> m_Map;
	uint32 m_Revision = 0;

	int m_AppliedCount = 0;
	int m_FailedCount = 0;

	void LoadTranslations(QString Lang);
	QHash<QString, QString> m_Translations;
	QString m_Language;
};