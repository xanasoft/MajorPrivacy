#pragma once

#include "../Core/GenericRule.h"

//
// How long a rule lasts, as a combo box. The same choices the connection prompt offers, plus, for
// a rule which already has a deadline, an entry that keeps it as it is: editing something else
// must not quietly make a rule permanent, or restart its clock.
//
// The values are the prompt's: -1 permanent, -2 until restart, -4 keep the deadline it has, and
// otherwise a number of seconds from now. A rule with a deadline is temporary as well, which is
// what the service purges on a restart.
//

#define RULE_LIFETIME_PERMANENT		(-1)
#define RULE_LIFETIME_RESTART		(-2)
#define RULE_LIFETIME_KEEP			(-4)

inline void PopulateRuleLifetime(QComboBox* pCombo, const CGenericRule* pRule = NULL)
{
	pCombo->addItem(QCoreApplication::translate("RuleLifetime", "Permanent"), RULE_LIFETIME_PERMANENT);
	pCombo->addItem(QCoreApplication::translate("RuleLifetime", "Untill Restart"), RULE_LIFETIME_RESTART);
#ifdef _DEBUG
	pCombo->addItem(QCoreApplication::translate("RuleLifetime", "for 1 Minute"), 60);
#else
	pCombo->addItem(QCoreApplication::translate("RuleLifetime", "for 5 Minutes"), 300);
#endif
	pCombo->addItem(QCoreApplication::translate("RuleLifetime", "for 15 Minutes"), 900);
	pCombo->addItem(QCoreApplication::translate("RuleLifetime", "for 30 Minutes"), 1800);
	pCombo->addItem(QCoreApplication::translate("RuleLifetime", "for 60 Minutes"), 3600);

	if (!pRule)
		return;

	if (pRule->GetTimeOut() != (quint64)-1) {
		QDateTime Until = QDateTime::fromSecsSinceEpoch((qint64)pRule->GetTimeOut());
		pCombo->insertItem(0, QCoreApplication::translate("RuleLifetime", "Until %1").arg(QLocale().toString(Until, QLocale::ShortFormat)), RULE_LIFETIME_KEEP);
		pCombo->setCurrentIndex(0);
	}
	else if (pRule->IsTemporary())
		pCombo->setCurrentIndex(pCombo->findData(RULE_LIFETIME_RESTART));
	else
		pCombo->setCurrentIndex(pCombo->findData(RULE_LIFETIME_PERMANENT));
}

// the temporary flag is only touched where it changes, a firewall rule keeps it in its grouping and setting it rewrites that
inline void ApplyRuleLifetime(CGenericRule* pRule, int Lifetime)
{
	switch (Lifetime)
	{
	case RULE_LIFETIME_KEEP:
		break;
	case RULE_LIFETIME_PERMANENT:
		pRule->ClearTimeOut();
		if (pRule->IsTemporary())
			pRule->SetTemporary(false);
		break;
	case RULE_LIFETIME_RESTART:
		pRule->ClearTimeOut();
		if (!pRule->IsTemporary())
			pRule->SetTemporary(true);
		break;
	default:
		if (Lifetime > 0)
			pRule->SetTimeOut(Lifetime);
	}
}
