#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace sweet::ui {

/// Settings that outlive a document: who you are, how the window starts up, and
/// what saving does by default.
///
/// Values are read from and written to QSettings directly, which is also where
/// the rest of the window looks them up, so there is no separate settings object
/// to keep in step. Applying emits nothing: MainWindow re-reads what it needs
/// when the dialog is accepted.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

    /// Writes every control back to QSettings. Called on accept.
    void apply();

    /// The keys this dialog owns, so callers know what to re-read.
    static QString authorKey();
    static QString darkModeKey();
    static QString defaultZoomKey();
    static QString restoreSessionKey();
    static QString incrementalSaveKey();

    /// Where the paths of the documents open at shutdown are kept, so they can be
    /// reopened when `restoreSessionKey` is on.
    static QString openDocumentsKey();

private:
    QLineEdit *m_author = nullptr;
    QCheckBox *m_darkMode = nullptr;
    QComboBox *m_defaultZoom = nullptr;
    QCheckBox *m_restoreSession = nullptr;
    QCheckBox *m_incrementalSave = nullptr;
};

} // namespace sweet::ui
