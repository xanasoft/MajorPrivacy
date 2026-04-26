#include "pch.h"
#include "PasswordStrengthWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <cmath>

CPasswordStrengthWidget::CPasswordStrengthWidget(QWidget* parent)
    : QWidget(parent)
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(8);

    // Left side: Vertical progress bar
    m_pStrengthBar = new QProgressBar;
    m_pStrengthBar->setOrientation(Qt::Vertical);
    m_pStrengthBar->setRange(0, 256);
    m_pStrengthBar->setValue(0);
    m_pStrengthBar->setTextVisible(false);
    m_pStrengthBar->setFixedSize(12, 60);
    m_pStrengthBar->setStyleSheet(
        "QProgressBar { border: 1px solid gray; background-color: #e0e0e0; }"
        "QProgressBar::chunk { background-color: #3080ff; }");
    mainLayout->addWidget(m_pStrengthBar);

    // Strength level labels
    QVBoxLayout* levelsLayout = new QVBoxLayout;
    levelsLayout->setSpacing(0);
    levelsLayout->setContentsMargins(0, 0, 0, 0);

    m_pLevelUnbreakable = new QLabel(tr("Unbreakable"));
    m_pLevelHigh = new QLabel(tr("High"));
    m_pLevelMedium = new QLabel(tr("Medium"));
    m_pLevelLow = new QLabel(tr("Low"));
    m_pLevelTrivial = new QLabel(tr("Trivial"));

    QString levelStyle = "QLabel { padding: 0px; margin: 0px; font-size: 11px; }";

    m_pLevelUnbreakable->setStyleSheet(levelStyle);
    m_pLevelHigh->setStyleSheet(levelStyle);
    m_pLevelMedium->setStyleSheet(levelStyle);
    m_pLevelLow->setStyleSheet(levelStyle);
    m_pLevelTrivial->setStyleSheet(levelStyle);

    levelsLayout->addWidget(m_pLevelUnbreakable);
    levelsLayout->addWidget(m_pLevelHigh);
    levelsLayout->addWidget(m_pLevelMedium);
    levelsLayout->addWidget(m_pLevelLow);
    levelsLayout->addWidget(m_pLevelTrivial);

    mainLayout->addLayout(levelsLayout);
    mainLayout->addSpacing(10);

    // Character class indicators
    QGridLayout* charLayout = new QGridLayout;
    charLayout->setSpacing(2);
    charLayout->setContentsMargins(0, 0, 0, 0);

    m_pCharSmallLatin = new QLabel(tr("Small Latin"));
    m_pCharCapsLatin = new QLabel(tr("Caps Latin"));
    m_pCharDigits = new QLabel(tr("Digits"));
    m_pCharSpace = new QLabel(tr("Space"));
    m_pCharSpecial = new QLabel(tr("Symbols"));
    m_pCharOther = new QLabel(tr("All Other"));

    QString charStyle = "QLabel { padding: 0px 2px; background-color: #e0e0e0; border: 1px solid #c0c0c0; }";

    m_pCharSmallLatin->setStyleSheet(charStyle);
    m_pCharCapsLatin->setStyleSheet(charStyle);
    m_pCharDigits->setStyleSheet(charStyle);
    m_pCharSpace->setStyleSheet(charStyle);
    m_pCharSpecial->setStyleSheet(charStyle);
    m_pCharOther->setStyleSheet(charStyle);

    charLayout->addWidget(m_pCharSmallLatin, 0, 0);
    charLayout->addWidget(m_pCharCapsLatin, 1, 0);
    charLayout->addWidget(m_pCharDigits, 2, 0);
    charLayout->addWidget(m_pCharSpace, 0, 1);
    charLayout->addWidget(m_pCharSpecial, 1, 1);
    charLayout->addWidget(m_pCharOther, 2, 1);

    mainLayout->addLayout(charLayout);
    mainLayout->addStretch();

    // Entropy display
    m_pStrengthText = new QLabel;
    mainLayout->addWidget(m_pStrengthText);

    UpdateDisplay();
}

void CPasswordStrengthWidget::SetPassword(const QString& password)
{
    PasswordInfo info;
    CheckPassword(password, info);

    m_Length = info.length;
    m_Flags = info.flags;
    m_Entropy = (int)info.entropy;  // Original password entropy (for display)

    // Calculate effective entropy with KDF bonus (for score/progress bar only)
    m_EffectiveEntropy = m_Entropy;
    if (m_KdfValue > 0 && info.entropy > 0) {
        m_EffectiveEntropy += (int)ceil(CalculateKdfEntropyBonus(m_KdfValue));
    }

    UpdateDisplay();
}

void CPasswordStrengthWidget::SetKdfValue(int kdfValue)
{
    m_KdfValue = kdfValue;
}

QString CPasswordStrengthWidget::GetPasswordStatusText(const QString& password, const QString& confirm)
{
    int length = password.length();

    if (!confirm.isEmpty() && password != confirm) {
        return tr("<span style='color:red;'>Passwords do not match!</span>");
    } else if (length > 128) {
        return tr("<span style='color:red;'>Password is too long. Maximum length is 128 characters.</span>");
    } else if (length > 0 && length < 8) {
        return tr("<span style='color:red;'>Password is very weak. Please use at least 8 characters.</span>");
    } else if (length >= 8 && length < 12) {
        return tr("<span style='color:orange;'>Password is weak. Consider using more characters.</span>");
    } else if (length >= 12 && length < 20) {
        return tr("<span style='color:#c0a000;'>Password strength: Fair. 20+ characters recommended.</span>");
    } else if (length >= 20) {
        return tr("<span style='color:green;'>Password strength: Good</span>");
    }
    return QString();
}

void CPasswordStrengthWidget::CheckPassword(const QString& password, PasswordInfo& info)
{
    info.flags = 0;
    info.length = password.length();
    int chars = 0;

    for (int i = 0; i < password.length(); i++) {
        QChar c = password[i];
        ushort code = c.unicode();

        if (c >= 'a' && c <= 'z') {
            info.flags |= P_AZ_L;
        } else if (c >= 'A' && c <= 'Z') {
            info.flags |= P_AZ_H;
        } else if (c >= '0' && c <= '9') {
            info.flags |= P_09;
        } else if (c == ' ') {
            info.flags |= P_SPACE;
        } else if ((code >= '!' && code <= '/') ||
                   (code >= ':' && code <= '@') ||
                   (code >= '[' && code <= '`') ||
                   (code >= '{' && code <= '~') ||
                   (code >= 0x00A1 && code <= 0x00BF) ||
                   code == 0x20AC || code == 0x00A3 || code == 0x00A5 ||
                   code == 0x00A7 || code == 0x00B6 || code == 0x00D7) {
            info.flags |= P_SPCH;
        } else {
            info.flags |= P_NCHAR;
        }
    }

    // Calculate character pool size based on character classes used
    if (info.flags & P_09)    chars += 10;                           // 0-9
    if (info.flags & P_AZ_L)  chars += 26;                           // a-z
    if (info.flags & P_AZ_H)  chars += 26;                           // A-Z
    if (info.flags & P_SPACE) chars += 1;                            // space
    if (info.flags & P_SPCH)  chars += 15 + 7 + 6 + 4 + 31 + 6;      // special characters
    if (info.flags & P_NCHAR) chars += 64;                           // national/other characters

    // Calculate entropy: bits = length * log2(pool_size)
    info.entropy = chars > 0 ? info.length * log((double)chars) / log(2.0) : 0;
}

double CPasswordStrengthWidget::CalculateKdfEntropyBonus(int kdfValue)
{
    if (kdfValue <= 0)
        return 0;

    // Memory and time cost tables
    static const int memory_mib_table[] = {
        64, 128, 192, 256,      // +64
        384, 512,               // +128
        768, 1024,              // +256
        1536, 2048              // +512
    };

    static const int time_cost_table[] = {
        3,  // for 64 MiB
        3,  // for 128 MiB
        4,  // for 192 MiB
        4,  // for 256 MiB
        5,  // for 384 MiB
        5,  // for 512 MiB
        5,  // for 768 MiB
        6,  // for 1024 MiB
        6,  // for 1536 MiB
        6   // for 2048 MiB
    };

    int idx = kdfValue - 1;
    if (idx < 0 || idx >= 10)
        return 0;

    const double PBKDF2_SHA512_1000_HPS = 2267000.0;
    const double GPU_VRAM_MIB = 24576.0;
    const double A2_REF_HPS = 1703.0;
    const double A2_REF_M_MIB = 64.0;
    const double A2_REF_T = 3.0;

    const double K = (A2_REF_HPS * A2_REF_T * (A2_REF_M_MIB * A2_REF_M_MIB)) / GPU_VRAM_MIB;

    double m = (double)memory_mib_table[idx];
    double t = (double)time_cost_table[idx];
    double Argon2_rate = (K * GPU_VRAM_MIB) / (t * m * m);
    if (Argon2_rate <= 0.0)
        return 0.0;

    double added_bits = log(PBKDF2_SHA512_1000_HPS / Argon2_rate) / log(2.0);
    return added_bits > 0.0 ? added_bits : 0.0;
}

void CPasswordStrengthWidget::UpdateDisplay()
{
    // Style definitions
    QString levelStyle = "QLabel { padding: 0px; margin: 0px; font-size: 11px; }";
    QString levelStyleActive = "QLabel { padding: 0px; margin: 0px; font-size: 11px; color: #3080ff; font-weight: bold; }";
    QString charStyle = "QLabel { padding: 0px 2px; background-color: #e0e0e0; border: 1px solid #c0c0c0; }";
    QString charStyleActive = "QLabel { padding: 0px 2px; background-color: #3080ff; color: white; border: 1px solid #2060cf; }";

    // Reset all indicators
    m_pLevelUnbreakable->setStyleSheet(levelStyle);
    m_pLevelHigh->setStyleSheet(levelStyle);
    m_pLevelMedium->setStyleSheet(levelStyle);
    m_pLevelLow->setStyleSheet(levelStyle);
    m_pLevelTrivial->setStyleSheet(levelStyle);

    m_pCharSmallLatin->setStyleSheet(charStyle);
    m_pCharCapsLatin->setStyleSheet(charStyle);
    m_pCharDigits->setStyleSheet(charStyle);
    m_pCharSpace->setStyleSheet(charStyle);
    m_pCharSpecial->setStyleSheet(charStyle);
    m_pCharOther->setStyleSheet(charStyle);

    if (m_Length == 0) {
        m_pStrengthText->setText("");
        m_pStrengthBar->setValue(0);
        m_pStrengthBar->setStyleSheet(
            "QProgressBar { border: 1px solid gray; background-color: #e0e0e0; }"
            "QProgressBar::chunk { background-color: #3080ff; }");
        return;
    }

    // Update character class indicators
    if (m_Flags & P_AZ_L)  m_pCharSmallLatin->setStyleSheet(charStyleActive);
    if (m_Flags & P_AZ_H)  m_pCharCapsLatin->setStyleSheet(charStyleActive);
    if (m_Flags & P_09)    m_pCharDigits->setStyleSheet(charStyleActive);
    if (m_Flags & P_SPACE) m_pCharSpace->setStyleSheet(charStyleActive);
    if (m_Flags & P_SPCH)  m_pCharSpecial->setStyleSheet(charStyleActive);
    if (m_Flags & P_NCHAR) m_pCharOther->setStyleSheet(charStyleActive);

    // Update progress bar using effective entropy (with KDF bonus)
    m_pStrengthBar->setValue(qMin(m_EffectiveEntropy, 256));

    // Determine strength level using effective entropy (with KDF bonus)
    if (m_EffectiveEntropy > 192) {
        m_pLevelUnbreakable->setStyleSheet(levelStyleActive);
        m_pStrengthBar->setStyleSheet(
            "QProgressBar { border: 1px solid gray; background-color: #e0e0e0; }"
            "QProgressBar::chunk { background-color: #30a030; }");
    } else if (m_EffectiveEntropy >= 129) {
        m_pLevelHigh->setStyleSheet(levelStyleActive);
        m_pStrengthBar->setStyleSheet(
            "QProgressBar { border: 1px solid gray; background-color: #e0e0e0; }"
            "QProgressBar::chunk { background-color: #60c060; }");
    } else if (m_EffectiveEntropy >= 81) {
        m_pLevelMedium->setStyleSheet(levelStyleActive);
        m_pStrengthBar->setStyleSheet(
            "QProgressBar { border: 1px solid gray; background-color: #e0e0e0; }"
            "QProgressBar::chunk { background-color: #f0c040; }");
    } else if (m_EffectiveEntropy >= 65) {
        m_pLevelLow->setStyleSheet(levelStyleActive);
        m_pStrengthBar->setStyleSheet(
            "QProgressBar { border: 1px solid gray; background-color: #e0e0e0; }"
            "QProgressBar::chunk { background-color: #f08040; }");
    } else {
        m_pLevelTrivial->setStyleSheet(levelStyleActive);
        m_pStrengthBar->setStyleSheet(
            "QProgressBar { border: 1px solid gray; background-color: #e0e0e0; }"
            "QProgressBar::chunk { background-color: #e04040; }");
    }

    // Update entropy text - show original password entropy (without KDF bonus)
    m_pStrengthText->setText(tr("%1 bits\n%2 chars").arg(m_Entropy).arg(m_Length));
}
