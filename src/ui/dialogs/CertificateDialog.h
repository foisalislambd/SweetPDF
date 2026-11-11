#pragma once

#include <QDialog>

class QLineEdit;

namespace sweet::ui {

/// Picks a PKCS#12 certificate and the reason recorded on the signature.
class CertificateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CertificateDialog(QWidget *parent = nullptr);

    QString certificatePath() const;
    QString password() const;
    QString reason() const;
    QString location() const;

private:
    void browse();

    QLineEdit *m_path = nullptr;
    QLineEdit *m_password = nullptr;
    QLineEdit *m_reason = nullptr;
    QLineEdit *m_location = nullptr;
};

} // namespace sweet::ui
