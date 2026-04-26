#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>

class CPasswordStrengthWidget : public QWidget
{
    Q_OBJECT

public:
    CPasswordStrengthWidget(QWidget* parent = nullptr);

    void SetPassword(const QString& password);
    void SetKdfValue(int kdfValue);

    int GetEntropy() const { return m_Entropy; }
    int GetLength() const { return m_Length; }

    static QString GetPasswordStatusText(const QString& password, const QString& confirm);

private:
    struct PasswordInfo {
        int flags;
        double entropy;
        int length;
    };

    enum CharFlags {
        P_AZ_L  = 1,   // lowercase letters
        P_AZ_H  = 2,   // uppercase letters
        P_09    = 4,   // digits
        P_SPACE = 8,   // space
        P_SPCH  = 16,  // special characters
        P_NCHAR = 32   // national/other characters
    };

    void CheckPassword(const QString& password, PasswordInfo& info);
    double CalculateKdfEntropyBonus(int kdfValue);
    void UpdateDisplay();

    int m_KdfValue = 0;
    int m_Entropy = 0;           // Original password entropy (for display)
    int m_EffectiveEntropy = 0;  // Entropy with KDF bonus (for score/progress)
    int m_Length = 0;
    int m_Flags = 0;

    // UI elements
    QProgressBar* m_pStrengthBar;
    QLabel* m_pStrengthText;

    // Strength level labels
    QLabel* m_pLevelUnbreakable;
    QLabel* m_pLevelHigh;
    QLabel* m_pLevelMedium;
    QLabel* m_pLevelLow;
    QLabel* m_pLevelTrivial;

    // Character class labels
    QLabel* m_pCharSmallLatin;
    QLabel* m_pCharCapsLatin;
    QLabel* m_pCharDigits;
    QLabel* m_pCharSpace;
    QLabel* m_pCharSpecial;
    QLabel* m_pCharOther;
};
