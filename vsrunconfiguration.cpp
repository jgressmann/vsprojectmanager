/**************************************************************************
**
** The MIT License (MIT)
**
** Copyright (c) 2016 Jean Gressmann
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
****************************************************************************/

#include "vsrunconfiguration.h"

#include "vsbuildconfiguration.h"
#include "vsproject.h"
#include "vsprojectconstants.h"

#if 0
#include <coreplugin/coreicons.h>
#else
#include <QDir>
#endif
#include <coreplugin/helpmanager.h>
#include <qtsupport/qtkitinformation.h>
#include <projectexplorer/localenvironmentaspect.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/target.h>

#include <utils/detailswidget.h>
#include <utils/pathchooser.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/stringutils.h>
#include <utils/environment.h>

#include <QFormLayout>
#include <QLineEdit>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QCheckBox>


using namespace VsProjectManager;
using namespace VsProjectManager::Internal;
using namespace ProjectExplorer;

namespace {
const char RC_PREFIX[] = "VsProjectManager.VsRunConfiguration.";
const char TITLE_KEY[] = "VsProjectManager.VsRunConfiguration.Title";
} // namespace

VsRunConfiguration::VsRunConfiguration(Target *parent, Core::Id id, const QString &target,
                                             const QString &workingDirectory, const QString &title) :
    RunConfiguration(parent, id),
    m_buildTarget(target),
    m_title(title)
{

    addExtraAspect(new LocalEnvironmentAspect(this, LocalEnvironmentAspect::BaseEnvironmentModifier()));
    addExtraAspect(new ArgumentsAspect(this, QStringLiteral("VsProjectManager.VsRunConfiguration.Arguments")));
    addExtraAspect(new TerminalAspect(this, QStringLiteral("VsProjectManager.VsRunConfiguration.UseTerminal")));

    auto wd = new WorkingDirectoryAspect(this, QStringLiteral("VsProjectManager.VsRunConfiguration.UserWorkingDirectory"));


    wd->setDefaultWorkingDirectory(Utils::FileName::fromString(workingDirectory));

    addExtraAspect(wd);

    ctor();
}

VsRunConfiguration::VsRunConfiguration(Target *parent, VsRunConfiguration *source) :
    RunConfiguration(parent, source),
    m_buildTarget(source->m_buildTarget),
    m_title(source->m_title),
    m_enabled(source->m_enabled)
{
    ctor();
}

void VsRunConfiguration::ctor()
{
    setDefaultDisplayName(defaultDisplayName());
}


Runnable VsRunConfiguration::runnable() const
{
    StandardRunnable r;
    r.executable = m_buildTarget;
    r.commandLineArguments = extraAspect<ArgumentsAspect>()->arguments();
    r.workingDirectory = extraAspect<WorkingDirectoryAspect>()->workingDirectory().toString();
    r.environment = extraAspect<LocalEnvironmentAspect>()->environment();
    r.runMode = extraAspect<TerminalAspect>()->runMode();
    return r;
}


QString VsRunConfiguration::baseWorkingDirectory() const
{
    const QString exe = m_buildTarget;
    if (!exe.isEmpty())
        return QFileInfo(m_buildTarget).absolutePath();
    return QString();
}

QString VsRunConfiguration::title() const
{
    return m_title;
}

void VsRunConfiguration::setExecutable(const QString &executable)
{
    m_buildTarget = executable;
}

void VsRunConfiguration::setBaseWorkingDirectory(const QString &wd)
{
    extraAspect<WorkingDirectoryAspect>()
        ->setDefaultWorkingDirectory(Utils::FileName::fromString(wd));
}

QVariantMap VsRunConfiguration::toMap() const
{
    QVariantMap map(RunConfiguration::toMap());
    map.insert(QLatin1String(TITLE_KEY), m_title);
    return map;
}

bool VsRunConfiguration::fromMap(const QVariantMap &map)
{
    m_title = map.value(QLatin1String(TITLE_KEY)).toString();
    return RunConfiguration::fromMap(map);
}

QString VsRunConfiguration::defaultDisplayName() const
{
    if (m_title.isEmpty())
        return tr("Run Visual Studio");
    QString result = m_title;
    if (!m_enabled) {
        result += QLatin1Char(' ');
        result += tr("(disabled)");
    }
    return result;
}

QWidget *VsRunConfiguration::createConfigurationWidget()
{
    return new VsRunConfigurationWidget(this);
}

void VsRunConfiguration::setEnabled(bool b)
{
    if (m_enabled == b)
        return;
    m_enabled = b;
    emit enabledChanged();
    setDefaultDisplayName(defaultDisplayName());
}

bool VsRunConfiguration::isEnabled() const
{
    return m_enabled;
}

QString VsRunConfiguration::disabledReason() const
{
    if (!m_enabled)
        return tr("The executable is not built by the current build configuration");
    return QString();
}

// Configuration widget
VsRunConfigurationWidget::VsRunConfigurationWidget(VsRunConfiguration *VsRunConfiguration, QWidget *parent)
    : QWidget(parent)
{
    auto fl = new QFormLayout();
    fl->setMargin(0);
    fl->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    VsRunConfiguration->extraAspect<ArgumentsAspect>()->addToMainConfigurationWidget(this, fl);
    VsRunConfiguration->extraAspect<WorkingDirectoryAspect>()->addToMainConfigurationWidget(this, fl);
    VsRunConfiguration->extraAspect<TerminalAspect>()->addToMainConfigurationWidget(this, fl);

    auto detailsContainer = new Utils::DetailsWidget(this);
    detailsContainer->setState(Utils::DetailsWidget::NoSummary);

    auto detailsWidget = new QWidget(detailsContainer);
    detailsContainer->setWidget(detailsWidget);
    detailsWidget->setLayout(fl);

    auto vbx = new QVBoxLayout(this);
    vbx->setMargin(0);
    vbx->addWidget(detailsContainer);

    setEnabled(VsRunConfiguration->isEnabled());
}

// Factory
VsRunConfigurationFactory::VsRunConfigurationFactory(QObject *parent) :
    IRunConfigurationFactory(parent)
{ setObjectName(QLatin1String("VsRunConfigurationFactory")); }

// used to show the list of possible additons to a project, returns a list of ids
QList<Core::Id> VsRunConfigurationFactory::availableCreationIds(Target *parent, CreationMode mode) const
{
    Q_UNUSED(mode)
    if (!canHandle(parent))
        return QList<Core::Id>();
    VsProject *project = static_cast<VsProject *>(parent->project());
    QList<Core::Id> allIds;
    foreach (const QString &buildTarget, project->buildTargetTitles(true))
        allIds << idFromBuildTarget(buildTarget);
    return allIds;
}

// used to translate the ids to names to display to the user
QString VsRunConfigurationFactory::displayNameForId(Core::Id id) const
{
    return buildTargetFromId(id);
}

bool VsRunConfigurationFactory::canHandle(Target *parent) const
{
    if (!parent->project()->supportsKit(parent->kit()))
        return false;
    return qobject_cast<VsProject *>(parent->project());
}

bool VsRunConfigurationFactory::canCreate(Target *parent, Core::Id id) const
{
    if (!canHandle(parent))
        return false;
    VsProject *project = static_cast<VsProject *>(parent->project());
    return project->hasBuildTarget(buildTargetFromId(id));
}

RunConfiguration *VsRunConfigurationFactory::doCreate(Target *parent, Core::Id id)
{
    VsProject *project = static_cast<VsProject *>(parent->project());
    const QString title(buildTargetFromId(id));
    const VsBuildTarget &target = project->buildTargetForTitle(title);
    return new VsRunConfiguration(parent, id, target.output, project->projectFilePath().toFileInfo().absoluteDir().absolutePath(), target.title);
}

bool VsRunConfigurationFactory::canClone(Target *parent, RunConfiguration *source) const
{
    if (!canHandle(parent))
        return false;
    return source->id().name().startsWith(RC_PREFIX);
}

RunConfiguration *VsRunConfigurationFactory::clone(Target *parent, RunConfiguration * source)
{
    if (!canClone(parent, source))
        return 0;
    VsRunConfiguration *crc(static_cast<VsRunConfiguration *>(source));
    return new VsRunConfiguration(parent, crc);
}

bool VsRunConfigurationFactory::canRestore(Target *parent, const QVariantMap &map) const
{
    if (!qobject_cast<VsProject *>(parent->project()))
        return false;
    return idFromMap(map).name().startsWith(RC_PREFIX);
}

RunConfiguration *VsRunConfigurationFactory::doRestore(Target *parent, const QVariantMap &map)
{
    return new VsRunConfiguration(parent, idFromMap(map), QString(), QString(), QString());
}

QString VsRunConfigurationFactory::buildTargetFromId(Core::Id id)
{
    return id.suffixAfter(RC_PREFIX);
}

Core::Id VsRunConfigurationFactory::idFromBuildTarget(const QString &target)
{
    return Core::Id(RC_PREFIX).withSuffix(target);
}
