#include "ui/dialogs/PreferencesDialog.h"

#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QVBoxLayout>

namespace sweet::ui {

QString PreferencesDialog::authorKey()
{
    return QStringLiteral("author");
}

QString PreferencesDialog::darkModeKey()
{
    return QStringLiteral("darkMode");
}

QString PreferencesDialog::defaultZoomKey()
{
    return QStringLiteral("defaultZoom");
}

QString PreferencesDialog::restoreSessionKey()
{
    return QStringLiteral("restoreSession");
}

QString PreferencesDialog::incrementalSaveKey()
{
    return QStringLiteral("incrementalSave");
}

QString PreferencesDialog::openDocumentsKey()
{
    return QStringLiteral("openDocuments");
}

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setModal(true);

    const QSettings settings;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 14);
    layout->setSpacing(14);

    auto *identity = new QGroupBox(tr("You"), this);
    auto *identityForm = new QFormLayout(identity);
    m_author = new QLineEdit(settings.value(authorKey(), qEnvironmentVariable("USER")).toString(),
                             identity);
    m_author->setPlaceholderText(tr("Your name"));
    identityForm->addRow(tr("Comment author:"), m_author);
    auto *authorHint = new QLabel(tr("Written onto every comment, highlight and signature you "
                                     "add."),
                                  identity);
    authorHint->setObjectName(QStringLiteral("SectionLabel"));
    authorHint->setWordWrap(true);
    identityForm->addRow(QString(), authorHint);
    layout->addWidget(identity);

    auto *appearance = new QGroupBox(tr("Appearance"), this);
    auto *appearanceForm = new QFormLayout(appearance);
    m_darkMode = new QCheckBox(tr("Dark theme"), appearance);
    m_darkMode->setChecked(settings.value(darkModeKey(), false).toBool());
    appearanceForm->addRow(QString(), m_darkMode);

    m_defaultZoom = new QComboBox(appearance);
    m_defaultZoom->addItem(tr("Fit width"), QStringLiteral("fitWidth"));
    m_defaultZoom->addItem(tr("Fit page"), QStringLiteral("fitPage"));
    m_defaultZoom->addItem(tr("100%"), QStringLiteral("actualSize"));
    const QString zoom = settings.value(defaultZoomKey(), QStringLiteral("fitWidth")).toString();
    m_defaultZoom->setCurrentIndex(qMax(0, m_defaultZoom->findData(zoom)));
    appearanceForm->addRow(tr("Open documents at:"), m_defaultZoom);

    layout->addWidget(appearance);

    auto *behaviour = new QGroupBox(tr("Behaviour"), this);
    auto *behaviourLayout = new QVBoxLayout(behaviour);
    m_restoreSession = new QCheckBox(tr("Reopen last documents on start"), behaviour);
    m_restoreSession->setChecked(settings.value(restoreSessionKey(), true).toBool());
    behaviourLayout->addWidget(m_restoreSession);

    m_incrementalSave = new QCheckBox(tr("Save changes by appending them (faster, keeps "
                                        "signatures)"),
                                     behaviour);
    m_incrementalSave->setChecked(settings.value(incrementalSaveKey(), true).toBool());
    behaviourLayout->addWidget(m_incrementalSave);
    layout->addWidget(behaviour);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void PreferencesDialog::apply()
{
    QSettings settings;
    settings.setValue(authorKey(), m_author->text().trimmed());
    settings.setValue(darkModeKey(), m_darkMode->isChecked());
    settings.setValue(defaultZoomKey(), m_defaultZoom->currentData().toString());
    settings.setValue(restoreSessionKey(), m_restoreSession->isChecked());
    settings.setValue(incrementalSaveKey(), m_incrementalSave->isChecked());

    Theme::instance().setMode(m_darkMode->isChecked() ? ThemeMode::Dark : ThemeMode::Light);
}

} // namespace sweet::ui
