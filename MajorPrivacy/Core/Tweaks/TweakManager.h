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

protected:
	CTweakPtr m_pRoot;
};