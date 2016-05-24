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

#include "vsbuildconfiguration.h"
#include "devenvstep.h"
#include "vsproject.h"
#include "vsprojectdata.h"
#include "vsprojectconstants.h"

#include <coreplugin/icore.h>
#include <projectexplorer/buildinfo.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/target.h>
#include <projectexplorer/toolchain.h>
#include <utils/mimetypes/mimedatabase.h>
#include <utils/qtcassert.h>

#include <QFileInfo>
#include <QInputDialog>

using namespace VsProjectManager;
using namespace VsProjectManager::Constants;
using namespace Internal;


namespace {
const char CONFIGURATION_KEY[] = "VsProjectManager.BuildConfiguration";

ProjectExplorer::BuildConfiguration::BuildType deriveBuildType(const QString& configuration)
{
    if (configuration.indexOf(QLatin1String("Release"), Qt::CaseInsensitive)) {
        return ProjectExplorer::BuildConfiguration::Release;
    }

    return ProjectExplorer::BuildConfiguration::Debug;
}
class VsBuildInfo : public ProjectExplorer::BuildInfo
{
public:
    VsBuildInfo(const ProjectExplorer::IBuildConfigurationFactory *f) :
        ProjectExplorer::BuildInfo(f)
    { }

    VsBuildInfo(const Internal::VsBuildConfiguration *bc) :
        ProjectExplorer::BuildInfo(ProjectExplorer::IBuildConfigurationFactory::find(bc->target()))
    {
        displayName = bc->displayName();
        buildDirectory = bc->buildDirectory();
        kitId = bc->target()->kit()->id();
    }
};
} // namespace


VsBuildConfiguration::VsBuildConfiguration(ProjectExplorer::Target *parent) :
    BuildConfiguration(parent, Core::Id(BC_ID))
{
    ctor();
}


VsBuildConfiguration::VsBuildConfiguration(ProjectExplorer::Target *parent, Core::Id id) :
    ProjectExplorer::BuildConfiguration(parent, id)
{
    ctor();
}

VsBuildConfiguration::VsBuildConfiguration(
        ProjectExplorer::Target *parent,
        VsBuildConfiguration *source) :
    ProjectExplorer::BuildConfiguration(parent, source)//,
//    vsProjectConfiguration(source->vsProjectConfiguration),
    //vsBuildTarget(source->vsBuildTarget)
//    vsBuildConfiguration(source->vsBuildConfiguration)
{
    cloneSteps(source);
}


ProjectExplorer::BuildConfiguration::BuildType VsBuildConfiguration::buildType() const
{
//    if (vsBuildTarget.configuration.isEmpty())
//        return ProjectExplorer::BuildConfiguration::Unknown;

//    return deriveBuildType(vsBuildTarget.configuration);

//    const QString& name = vsBuildConfiguration;
    const QString& name = displayName();
    if (name.isEmpty())
        return ProjectExplorer::BuildConfiguration::Unknown;

    return deriveBuildType(name);
}

void VsBuildConfiguration::ctor()
{
    auto project = static_cast<VsProject *>(target()->project());
    setBuildDirectory(project->projectFilePath());

    //project->tryGetBuildTargetForTitle(displayName(), vsBuildTarget);
//    setBuildDirectory(shadowBuildDirectory(project->projectFilePath(),
//                                           target()->kit(),
//                                           displayName(), BuildConfiguration::Unknown));

//    m_buildDirManager = new BuildDirManager(this);
//    connect(m_buildDirManager, &BuildDirManager::dataAvailable,
//            this, &CMakeBuildConfiguration::dataAvailable);
//    connect(m_buildDirManager, &BuildDirManager::errorOccured,
//            this, &CMakeBuildConfiguration::setError);
//    connect(m_buildDirManager, &BuildDirManager::configurationStarted,
//            this, [this]() { m_completeConfigurationCache.clear(); emit parsingStarted(); });

//    connect(this, &CMakeBuildConfiguration::environmentChanged,
//            m_buildDirManager, &BuildDirManager::forceReparse);
//    connect(this, &CMakeBuildConfiguration::buildDirectoryChanged,
//            m_buildDirManager, &BuildDirManager::forceReparse);

//    connect(this, &CMakeBuildConfiguration::parsingStarted, project, &CMakeProject::handleParsingStarted);
//    connect(this, &CMakeBuildConfiguration::dataAvailable, project, &CMakeProject::parseCMakeOutput);
}


QVariantMap VsBuildConfiguration::toMap() const
{
    QVariantMap map(ProjectExplorer::BuildConfiguration::toMap());
    //map.insert(QLatin1String(CONFIGURATION_KEY), vsBuildConfiguration);
    return map;
}

bool VsBuildConfiguration::fromMap(const QVariantMap &map)
{
    if (!BuildConfiguration::fromMap(map))
        return false;

    //vsBuildConfiguration = map.value(QLatin1String(CONFIGURATION_KEY)).toString();

    return true;
}



VsBuildConfigurationFactory::VsBuildConfigurationFactory(QObject *parent) :
    IBuildConfigurationFactory(parent)
{ }

ProjectExplorer::NamedWidget *VsBuildConfiguration::createConfigWidget()
{
    return nullptr;
}
#if 0
bool VsBuildConfiguration::isEnabled() const
{
    return true;
}

QString VsBuildConfiguration::disabledReason() const
{
    return QString();
}
#endif

int VsBuildConfigurationFactory::priority(const ProjectExplorer::Target *parent) const
{
    return canHandle(parent) ? 0 : -1;
}

QList<ProjectExplorer::BuildInfo *> VsBuildConfigurationFactory::availableBuilds(const ProjectExplorer::Target *parent) const
{
    return availableSetups(parent->kit(), parent->project()->projectFilePath().toString());
    QList<ProjectExplorer::BuildInfo *> result;



//    for (int type = BuildTypeNone; type != BuildTypeLast; ++type) {
//        CMakeBuildInfo *info = createBuildInfo(parent->kit(),
//                                               parent->project()->projectDirectory().toString(),
//                                               BuildType(type));
//        result << info;
//    }
    return result;
}


int VsBuildConfigurationFactory::priority(const ProjectExplorer::Kit *k, const QString &projectPath) const
{
    Utils::MimeDatabase mdb;
    if (k && mdb.mimeTypeForFile(projectPath).matchesName(QLatin1String(Constants::MIMETYPE)))
        return 0;
    return -1;
}

QList<ProjectExplorer::BuildInfo *> VsBuildConfigurationFactory::availableSetups(const ProjectExplorer::Kit *k, const QString &projectPath) const
{
    QList<ProjectExplorer::BuildInfo *> result;
    QScopedPointer<VsProjectData> data(VsProjectData::load(Utils::FileName::fromString(projectPath)));
    QTC_ASSERT(!data.isNull(), return result);

//    foreach (const VsBuildTarget& target, data->targets()) {
//        auto buildInfo = new VsBuildInfo(this);
//        buildInfo->buildType = deriveBuildType(target.configuration);
//        //buildInfo->vsBuildTarget = target;
//        buildInfo->displayName = target.title;
//        buildInfo->kitId = k->id();
//        buildInfo->typeName = target.title;

//        result << buildInfo;
//    }

    foreach (const QString& configuration, data->configurations()) {
        auto buildInfo = new VsBuildInfo(this);
        buildInfo->buildType = deriveBuildType(configuration);
        buildInfo->displayName = configuration;
        buildInfo->kitId = k->id();
        buildInfo->typeName = configuration; // this sets the name in the 'Add' configuration drop down

        result << buildInfo;
    }

    return result;
}

ProjectExplorer::BuildConfiguration *VsBuildConfigurationFactory::create(ProjectExplorer::Target *parent, const ProjectExplorer::BuildInfo *info) const
{
    QTC_ASSERT(parent, return 0);
    QTC_ASSERT(info->factory() == this, return 0);
    QTC_ASSERT(info->kitId == parent->kit()->id(), return 0);
    QTC_ASSERT(!info->displayName.isEmpty(), return 0);


    VsBuildConfiguration *bc = new VsBuildConfiguration(parent);
    bc->setDisplayName(info->displayName);
    bc->setDefaultDisplayName(info->displayName);
    bc->setBuildDirectory(info->buildDirectory);
    //bc->vsProjectConfiguration = static_cast<const VsBuildInfo*>(info)->vsProjectConfiguration;

    ProjectExplorer::BuildStepList *buildSteps = bc->stepList(Core::Id(ProjectExplorer::Constants::BUILDSTEPS_BUILD));


    // build
    auto buildStep = new DevenvStep(buildSteps);
    buildStep->setConfiguration(info->displayName);
    buildSteps->insertStep(0, buildStep);

    // clean
    ProjectExplorer::BuildStepList *cleanSteps = bc->stepList(Core::Id(ProjectExplorer::Constants::BUILDSTEPS_CLEAN));
    auto cleanStep = new DevenvStep(cleanSteps);
    cleanStep->setConfiguration(info->displayName);
    cleanStep->setClean(true);
    cleanSteps->insertStep(0, cleanStep);

    return bc;
}

bool VsBuildConfigurationFactory::canHandle(const ProjectExplorer::Target *t) const
{
    QTC_ASSERT(t, return false);

    if (!t->project()->supportsKit(t->kit()))
        return false;
    return t->project()->id() == Constants::PROJECT_ID;
}


bool VsBuildConfigurationFactory::canClone(const ProjectExplorer::Target *parent, ProjectExplorer::BuildConfiguration *source) const
{
    Q_UNUSED(parent);
    Q_UNUSED(source);
    // don't permit clone to create new
    return false;

//    if (!canHandle(parent))
//        return false;
//    return source->id() == VS_BC_ID;
}

VsBuildConfiguration *VsBuildConfigurationFactory::clone(ProjectExplorer::Target *parent, ProjectExplorer::BuildConfiguration *source)
{
    if (!canClone(parent, source))
        return 0;

    VsBuildConfiguration *origin = static_cast<VsBuildConfiguration *>(source);
    return new VsBuildConfiguration(parent, origin);
}

bool VsBuildConfigurationFactory::canRestore(const ProjectExplorer::Target *parent, const QVariantMap &map) const
{
    if (!canHandle(parent))
        return false;
    return ProjectExplorer::idFromMap(map) == Constants::BC_ID;
}

VsBuildConfiguration *VsBuildConfigurationFactory::restore(ProjectExplorer::Target *parent, const QVariantMap &map)
{
    if (!canRestore(parent, map))
        return 0;
    VsBuildConfiguration *bc = new VsBuildConfiguration(parent);
    if (bc->fromMap(map))
        return bc;
    delete bc;
    return 0;
}
