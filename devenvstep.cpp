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

#include "devenvstep.h"
#include "vsproject.h"
#include "vsprojectconstants.h"
#include "vsbuildconfiguration.h"

#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/target.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/msvcparser.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <qtsupport/qtkitinformation.h>
#include <qtsupport/qtparser.h>
#include <utils/qtcprocess.h>

#include <QVariantMap>
#include <QLineEdit>
#include <QFormLayout>

using namespace VsProjectManager;
using namespace VsProjectManager::Internal;
using namespace VsProjectManager::Constants;
using namespace ProjectExplorer;
using namespace ProjectExplorer::Constants;

namespace {
const char DEVENV_STEP_ID[] = "VsProjectManager.DevenvStep";
const char DEVENV_STEP_CLEAN_KEY[]  = "VsProjectManager.DevenvStep.Clean";
const char DEVENV_STEP_CONFIGURATION_KEY[] = "VsProjectManager.DevenvStep.Configuration";
} // namespace

DevenvStepFactory::DevenvStepFactory(QObject *parent) : IBuildStepFactory(parent)
{ setObjectName(QLatin1String("VsProjectManager::DevenvStepFactory")); }

QList<Core::Id> DevenvStepFactory::availableCreationIds(BuildStepList *parent) const
{
    if (parent->target()->project()->id() == PROJECT_ID)
        return QList<Core::Id>() << Core::Id(DEVENV_STEP_ID);
    return QList<Core::Id>();
}

QString DevenvStepFactory::displayNameForId(Core::Id id) const
{
    if (id == DEVENV_STEP_ID)
        return tr("Run", "Display name for VsProjectManager::DevenvStep id.");
    return QString();
}

bool DevenvStepFactory::canCreate(BuildStepList *parent, Core::Id id) const
{
    if (parent->target()->project()->id() == PROJECT_ID)
        return id == DEVENV_STEP_ID;
    return false;
}

BuildStep *DevenvStepFactory::create(BuildStepList *parent, Core::Id id)
{
    if (!canCreate(parent, id))
        return nullptr;
    return new DevenvStep(parent);
}

bool DevenvStepFactory::canClone(BuildStepList *parent, BuildStep *source) const
{
    return canCreate(parent, source->id());
}

BuildStep *DevenvStepFactory::clone(BuildStepList *parent, BuildStep *source)
{
    if (!canClone(parent, source))
        return nullptr;
    return new DevenvStep(parent, static_cast<DevenvStep *>(source));
}

bool DevenvStepFactory::canRestore(BuildStepList *parent, const QVariantMap &map) const
{
    return canCreate(parent, idFromMap(map));
}

BuildStep *DevenvStepFactory::restore(BuildStepList *parent, const QVariantMap &map)
{
    if (!canRestore(parent, map))
        return 0;
    DevenvStep *bs = new DevenvStep(parent);
    if (bs->fromMap(map))
        return bs;
    delete bs;
    return 0;
}


DevenvStep::DevenvStep(BuildStepList* bsl) : AbstractProcessStep(bsl, Core::Id(DEVENV_STEP_ID))
{
    ctor();
}

DevenvStep::DevenvStep(BuildStepList *bsl, Core::Id id) : AbstractProcessStep(bsl, id)
{
    ctor();
}

DevenvStep::DevenvStep(BuildStepList *bsl, DevenvStep *bs) : AbstractProcessStep(bsl, bs),
    m_clean(bs->m_clean),
    m_configuration(bs->m_configuration)
{
    ctor();
}

void DevenvStep::ctor()
{
    setDefaultDisplayName(tr("Run Visual Studio"));
}

QVariantMap DevenvStep::toMap() const
{
    QVariantMap map = AbstractProcessStep::toMap();

    map.insert(QLatin1String(DEVENV_STEP_CLEAN_KEY), m_clean);
    map.insert(QLatin1String(DEVENV_STEP_CONFIGURATION_KEY), m_configuration);
    return map;
}

bool DevenvStep::fromMap(const QVariantMap &map)
{
    m_clean = map.value(QLatin1String(DEVENV_STEP_CLEAN_KEY)).toBool();
    m_configuration = map.value(QLatin1String(DEVENV_STEP_CONFIGURATION_KEY)).toString();

    return BuildStep::fromMap(map);
}

void DevenvStep::setClean(bool clean)
{
    m_clean = clean;
}

void DevenvStep::setConfiguration(const QString& configuration)
{
    m_configuration = configuration;
}

bool DevenvStep::init(QList<const BuildStep *> &earlierSteps)
{
    BuildConfiguration *bc = buildConfiguration();
    if (!bc)
        bc = target()->activeBuildConfiguration();
    if (!bc)
        emit addTask(Task::buildConfigurationMissingTask());

    if (!bc) {
        emitFaultyConfigurationMessage();
        return false;
    }

    auto project = static_cast<VsProject*>(bc->target()->project());
    QString cmd;
    QString args;
    if (project) {
        if (m_clean) {
            project->vsProjectData()->cleanCmd(bc->displayName(), &cmd, &args);
        } else {
            project->vsProjectData()->buildCmd(bc->displayName(), &cmd, &args);
        }
    }

    setIgnoreReturnValue(m_clean);

    ProcessParameters *pp = processParameters();
    pp->setMacroExpander(bc->macroExpander());
    Utils::Environment env = bc->environment();

    Utils::Environment::setupEnglishOutput(&env);

    pp->setEnvironment(env);
    pp->setWorkingDirectory(bc->buildDirectory().toString());
    pp->setCommand(cmd);
    pp->setArguments(args);
    pp->resolveAll();

    setOutputParser(new MsvcParser());
    outputParser()->setWorkingDirectory(pp->effectiveWorkingDirectory());
    return AbstractProcessStep::init(earlierSteps);
}


void DevenvStep::run(QFutureInterface<bool> &interface)
{
    AbstractProcessStep::run(interface);
}

ProjectExplorer::BuildStepConfigWidget *DevenvStep::createConfigWidget()
{
    return new DevenvStepConfigWidget(this);
}

bool DevenvStep::immutable() const
{
    return false;
}


DevenvStepConfigWidget::DevenvStepConfigWidget(DevenvStep *DevenvStep) :
    m_devenvStep(DevenvStep),
    m_summaryText(),
    m_additionalArguments(0)
{
    QFormLayout *fl = new QFormLayout(this);
    fl->setMargin(0);
    fl->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    setLayout(fl);

    updateDetails();

    connect(m_devenvStep->project(), &Project::environmentChanged,
            this, &DevenvStepConfigWidget::updateDetails);
}

QString DevenvStepConfigWidget::displayName() const
{
    return tr("Run", "VSProjectManager::DevenvStepConfigWidget display name.");
}

QString DevenvStepConfigWidget::summaryText() const
{
    return m_summaryText;
}

void DevenvStepConfigWidget::updateDetails()
{
    BuildConfiguration *bc = m_devenvStep->buildConfiguration();
    if (!bc)
        bc = m_devenvStep->target()->activeBuildConfiguration();

    if (bc) {
        QString args;
        QString cmd;
        auto project = static_cast<VsProject*>(bc->target()->project());
        if (project) {
            if (m_devenvStep->m_clean) {
                project->vsProjectData()->cleanCmd(bc->displayName(), &cmd, &args);
            } else {
                project->vsProjectData()->buildCmd(bc->displayName(), &cmd, &args);
            }
        }

        ProcessParameters param;
        param.setMacroExpander(bc->macroExpander());
        param.setEnvironment(bc->environment());
        param.setWorkingDirectory(bc->buildDirectory().toString());
        param.setCommand(cmd);
        param.setArguments(args);
        m_summaryText = param.summary(displayName());
    } else {
        m_summaryText = QLatin1String("<b>") + ProjectExplorer::ToolChainKitInformation::msgNoToolChainInTarget()  + QLatin1String("</b>");
    }

    emit updateSummary();
}
