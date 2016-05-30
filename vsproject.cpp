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

#include "vsproject.h"
#include "vsbuildconfiguration.h"
#include "vsprojectconstants.h"
#include "vsmanager.h"
#include "vsprojectnode.h"
#include "vsprojectfile.h"
#include "vsprojectdata.h"
#include "vsrunconfiguration.h"

#include <projectexplorer/abi.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/target.h>
#include <projectexplorer/headerpath.h>
#include <projectexplorer/buildinfo.h>
#include <projectexplorer/buildtargetinfo.h>
#include <projectexplorer/deploymentdata.h>
#include <extensionsystem/pluginmanager.h>
#include <cpptools/cppmodelmanager.h>
#include <cpptools/projectinfo.h>
#include <cpptools/projectpartbuilder.h>
#include <coreplugin/icore.h>
#include <coreplugin/icontext.h>
#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtkitinformation.h>
#include <utils/qtcassert.h>
#include <utils/filesystemwatcher.h>
#include <utils/algorithm.h>
#include <utils/stringutils.h>

#include <QFileInfo>
#include <QTimer>
#include <QPointer>
#include <QApplication>
#include <QCursor>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace VsProjectManager;
using namespace VsProjectManager::Internal;



VsProject::~VsProject()
{
    setRootProjectNode(nullptr);

    m_codeModelFuture.cancel();
    delete m_vsProjectData;

}

VsProject::VsProject(VsManager *manager, const QString &fileName) :
    m_fileWatcher(new Utils::FileSystemWatcher(this))
{

    setProjectManager(manager);
    setDocument(new VsProjectFile(fileName));
    setRootProjectNode(new VsProjectNode(projectFilePath()));
    setId(Constants::PROJECT_ID);
    setProjectContext(Core::Context(Constants::PROJECT_CONTEXT));
    setProjectLanguages(Core::Context(ProjectExplorer::Constants::LANG_CXX));

    rootProjectNode()->setDisplayName(projectFilePath().toFileInfo().absoluteDir().dirName());

    connect(this, &VsProject::activeTargetChanged, this, &VsProject::handleActiveTargetChanged);
    connect(m_fileWatcher, &Utils::FileSystemWatcher::fileChanged, this, &VsProject::onFileChanged);

    m_fileWatcher->addFile(fileName, Utils::FileSystemWatcher::WatchAllChanges);

    loadProjectTree();
}


QString VsProject::displayName() const
{
    return rootProjectNode()->displayName();
}

QString VsProject::defaultBuildDirectory(const QString &projectPath)
{
    return QFileInfo(projectPath).absolutePath();
}

QStringList VsProject::files(FilesMode fileMode) const
{
    QList<ProjectExplorer::FileNode *> nodes;
    gatherFileNodes(rootProjectNode(), nodes);
    nodes = Utils::filtered(nodes, [fileMode](const ProjectExplorer::FileNode *fn) {
        const bool isGenerated = fn->isGenerated();
        switch (fileMode)
        {
        case ProjectExplorer::Project::SourceFiles:
            return !isGenerated;
        case ProjectExplorer::Project::GeneratedFiles:
            return isGenerated;
        case ProjectExplorer::Project::AllFiles:
        default:
            return true;
        }
    });

    return Utils::transform(nodes, [](const ProjectExplorer::FileNode* fn) { return fn->filePath().toString(); });
}

// This function, is called at the very beginning, to
// restore the settings if there are some stored.
ProjectExplorer::Project::RestoreResult VsProject::fromMap(const QVariantMap &map, QString *errorMessage)
{
    RestoreResult result = Project::fromMap(map, errorMessage);
    if (result != RestoreResult::Ok)
        return result;

//    connect(m_fileWatcher, &Utils::FileSystemWatcher::fileChanged,
//            this, &VsProject::onFileChanged);

//    // Load the project tree structure.
//    loadProjectTree();

//    ProjectExplorer::Kit *defaultKit = ProjectExplorer::KitManager::defaultKit();
//    if (!activeTarget() && defaultKit)
//        addTarget(createTarget(defaultKit));

    return RestoreResult::Ok;
}

void VsProject::loadProjectTree()
{
    parsingStarted();
    delete m_vsProjectData;
    m_vsProjectData = VsProjectData::load(projectFilePath());
    parsingFinished();
}

void VsProject::parsingStarted()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

void VsProject::parsingFinished()
{
    QApplication::restoreOverrideCursor();

    QList<ProjectExplorer::FileNode*> fileNodes;
    if (m_vsProjectData) {
            fileNodes = Utils::transform(m_vsProjectData->files(), [](const QString& filePath) {
        return new ProjectExplorer::FileNode(
                    Utils::FileName::fromString(filePath),
                    getFileType(filePath), false); });
    }

    buildTree(static_cast<VsProjectNode *>(rootProjectNode()), fileNodes);

    emit fileListChanged();

    updateApplicationAndDeploymentTargets();

    onTargetChanged();
}

void VsProject::onTargetChanged()
{
    if (ProjectExplorer::Target * t = activeTarget()) {
        updateTargetRunConfigurations(t);

        updateCppCodeModel();
    }
}

void VsProject::onFileChanged(const QString &file)
{
    Q_UNUSED(file);
    loadProjectTree();
}

bool VsProject::needsConfiguration() const
{
    return targets().isEmpty();
}

bool VsProject::requiresTargetPanel() const
{
    return !targets().isEmpty();
}

QList<ProjectExplorer::Node *> VsProject::nodes(ProjectExplorer::FolderNode *parent) const
{
    QList<ProjectExplorer::Node *> list;
    QTC_ASSERT(parent != 0, return list);

    foreach (ProjectExplorer::FolderNode *folder, parent->subFolderNodes()) {
        list.append(nodes(folder));
        list.append(folder);
    }
    foreach (ProjectExplorer::FileNode *file, parent->fileNodes())
        list.append(file);

    return list;
}

void VsProject::updateCppCodeModel()
{
    CppTools::CppModelManager *modelManager = CppTools::CppModelManager::instance();

    m_codeModelFuture.cancel();
    CppTools::ProjectInfo pInfo(this);
    CppTools::ProjectPartBuilder ppBuilder(pInfo);

    if (activeTarget()) {
        CppTools::ProjectPart::QtVersion activeQtVersion = CppTools::ProjectPart::NoQt;
        if (QtSupport::BaseQtVersion *qtVersion =
                QtSupport::QtKitInformation::qtVersion(activeTarget()->kit())) {
            if (qtVersion->qtVersion() < QtSupport::QtVersionNumber(5,0,0))
                activeQtVersion = CppTools::ProjectPart::Qt4;
            else
                activeQtVersion = CppTools::ProjectPart::Qt5;
        }

        ppBuilder.setQtVersion(activeQtVersion);
    }

//    QStringList cxxflags = m_makefileParserThread->data()->configurations()[0]->CxxFlags;
//    ppBuilder.setCFlags(cxxflags);
//    ppBuilder.setCxxFlags(cxxflags);
//    ppBuilder.setIncludePaths(m_makefileParserThread->data()->configurations()[0]->IncludeDirectories);
//    ppBuilder.setDefines(m_makefileParserThread->data()->configurations()[0]->Defines);
//    ppBuilder.setDisplayName(m_makefileParserThread->data()->configurations()[0]->Name);

//    const QList<Core::Id> languages = ppBuilder.createProjectPartsForFiles(m_files);
//    foreach (Core::Id language, languages)
//        setProjectLanguage(language, true);

//    pInfo.finish();
//    m_codeModelFuture = modelManager->updateProjectInfo(pInfo);


    foreach (const VsBuildTarget &target, buildTargets()) {
        ppBuilder.setIncludePaths(target.includeDirectories);
        ppBuilder.setCFlags(target.compilerOptions);
        ppBuilder.setCxxFlags(target.compilerOptions);
        ppBuilder.setDefines(target.defines);
        ppBuilder.setDisplayName(target.title);

        const QList<Core::Id> languages = ppBuilder.createProjectPartsForFiles(m_vsProjectData->files());
        foreach (Core::Id language, languages)
            setProjectLanguage(language, true);
    }

    m_codeModelFuture.cancel();
    pInfo.finish();
    m_codeModelFuture = modelManager->updateProjectInfo(pInfo);
}

ProjectExplorer::FileType VsProject::getFileType(const QString& fileName)
{
    static const char* HeaderExtensions[] = { ".h", ".hpp", ".hxx", ".inl" };
    static const char* SourceExtensions[] = { ".c", ".cpp", ".cxx", ".asm" };
    static const char* ResourceExtensions[] = { ".rc", ".rgs" };
    static const char* ProjectExtensions[] = { ".vcproj", ".vcxproj" };

    for (size_t i = 0; i < _countof(HeaderExtensions); ++i) {
        if (fileName.endsWith(QLatin1String(HeaderExtensions[i]), Qt::CaseInsensitive)) {
            return ProjectExplorer::HeaderType;
        }
    }

    for (size_t i = 0; i < _countof(SourceExtensions); ++i) {
        if (fileName.endsWith(QLatin1String(SourceExtensions[i]), Qt::CaseInsensitive)) {
            return ProjectExplorer::SourceType;
        }
    }

    for (size_t i = 0; i < _countof(ResourceExtensions); ++i) {
        if (fileName.endsWith(QLatin1String(ResourceExtensions[i]), Qt::CaseInsensitive)) {
            return ProjectExplorer::ResourceType;
        }
    }

    for (size_t i = 0; i < _countof(ProjectExtensions); ++i) {
        if (fileName.endsWith(QLatin1String(ProjectExtensions[i]), Qt::CaseInsensitive)) {
            return ProjectExplorer::ProjectFileType;
        }
    }

    return ProjectExplorer::UnknownFileType;
}


bool VsProject::knowsAllBuildExecutables() const
{
    return false;
}


bool VsProject::setupTarget(ProjectExplorer::Target *t)
{
    t->updateDefaultBuildConfigurations();
    if (t->buildConfigurations().isEmpty())
        return false;
    t->updateDefaultDeployConfigurations();

    return true;
}


QList<VsBuildTarget> VsProject::buildTargets() const
{
    QList<VsBuildTarget> result;
    if (m_vsProjectData) {
        if (activeTarget() && activeTarget()->activeBuildConfiguration()) {
            auto bc = static_cast<VsBuildConfiguration *>(activeTarget()->activeBuildConfiguration());
            result = Utils::filtered(m_vsProjectData->targets(),
                                     [bc](const VsBuildTarget &ct) {
                                         return bc->displayName() == ct.configuration;
                                     });
        }
    }

    return result;
}

QStringList VsProject::buildTargetTitles(bool runnable) const
{
    const QList<VsBuildTarget> targets
            = runnable ? Utils::filtered(buildTargets(),
                                         [](const VsBuildTarget &target) {
                                             return !target.output.isEmpty() && target.targetType == ExecutableType;
                                         })
                       : buildTargets();
    return Utils::transform(targets, [](const VsBuildTarget &target) { return target.title; });
}

void VsProject::handleActiveTargetChanged()
{
    if (m_connectedTarget) {
        disconnect(m_connectedTarget, &ProjectExplorer::Target::activeBuildConfigurationChanged,
                   this, &VsProject::handleActiveBuildConfigurationChanged);
        disconnect(m_connectedTarget, &ProjectExplorer::Target::kitChanged,
                   this, &VsProject::handleActiveBuildConfigurationChanged);
    }

    m_connectedTarget = activeTarget();

    if (m_connectedTarget) {
        connect(m_connectedTarget, &ProjectExplorer::Target::activeBuildConfigurationChanged,
                this, &VsProject::handleActiveBuildConfigurationChanged);
        connect(m_connectedTarget, &ProjectExplorer::Target::kitChanged,
                this, &VsProject::handleActiveBuildConfigurationChanged);
    }

    handleActiveBuildConfigurationChanged();
}

void VsProject::handleActiveBuildConfigurationChanged()
{
    if (!activeTarget() || !activeTarget()->activeBuildConfiguration())
        return;
//    auto activeBc = qobject_cast<VsBuildConfiguration *>(activeTarget()->activeBuildConfiguration());

//    foreach (ProjectExplorer::Target *t, targets()) {
//        foreach (ProjectExplorer::BuildConfiguration *bc, t->buildConfigurations()) {
//            auto i = qobject_cast<VsBuildConfiguration *>(bc);
//            QTC_ASSERT(i, continue);
//            if (i == activeBc)
//                i->maybeForceReparse();
//            else
//                i->resetData();
//        }
//    }

    onTargetChanged();
}

static bool sortNodesByPath(ProjectExplorer::Node *a, ProjectExplorer::Node *b)
{
    return a->filePath() < b->filePath();
}


void VsProject::buildTree(VsProjectNode *rootNode, QList<ProjectExplorer::FileNode *> newList)
{
    // Gather old list
    QList<ProjectExplorer::FileNode *> oldList;
    gatherFileNodes(rootNode, oldList);
    Utils::sort(oldList, sortNodesByPath);
    Utils::sort(newList, sortNodesByPath);

    QList<ProjectExplorer::FileNode *> added;
    QList<ProjectExplorer::FileNode *> deleted;

    ProjectExplorer::compareSortedLists(oldList, newList, deleted, added, sortNodesByPath);

    qDeleteAll(ProjectExplorer::subtractSortedList(newList, added, sortNodesByPath));

    // add added nodes
    foreach (ProjectExplorer::FileNode *fn, added) {
        // Get relative path to rootNode
        QString parentDir = fn->filePath().toFileInfo().absolutePath();
        ProjectExplorer::FolderNode *folder = findOrCreateFolder(rootNode, parentDir);
        folder->addFileNodes(QList<ProjectExplorer::FileNode *>()<< fn);
    }

    // remove old file nodes and check whether folder nodes can be removed
    foreach (ProjectExplorer::FileNode *fn, deleted) {
        ProjectExplorer::FolderNode *parent = fn->parentFolderNode();
        parent->removeFileNodes(QList<ProjectExplorer::FileNode *>() << fn);

        // Check for empty parent
        while (parent != rootNode &&
               parent->subFolderNodes().isEmpty() &&
               parent->fileNodes().isEmpty()) {
            ProjectExplorer::FolderNode *grandparent = parent->parentFolderNode();
            grandparent->removeFolderNodes(QList<ProjectExplorer::FolderNode *>() << parent);
            parent = grandparent;
        }
    }
}

ProjectExplorer::FolderNode *VsProject::findOrCreateFolder(VsProjectNode *rootNode, QString directory)
{

    Utils::FileName path = rootNode->filePath().parentDir();
    QDir rootParentDir(path.toString());
    QString relativePath = rootParentDir.relativeFilePath(directory);
    if (relativePath == QLatin1String("."))
        relativePath.clear();
    QStringList parts = relativePath.split(QLatin1Char('/'), QString::SkipEmptyParts);
    ProjectExplorer::FolderNode *parent = rootNode;
    foreach (const QString &part, parts) {
        path.appendPath(part);
        // Find folder in subFolders
        bool found = false;
        foreach (ProjectExplorer::FolderNode *folder, parent->subFolderNodes()) {

            if (folder->filePath() == path) {
                // yeah found something :)
                parent = folder;
                found = true;
                break;
            }
        }
        if (!found) {
            // No FolderNode yet, so create it
            auto tmp = new ProjectExplorer::FolderNode(path);
            tmp->setDisplayName(part);
            parent->addFolderNodes(QList<ProjectExplorer::FolderNode *>() << tmp);
            parent = tmp;
        }
    }
    return parent;
}

void VsProject::gatherFileNodes(ProjectExplorer::FolderNode *parent, QList<ProjectExplorer::FileNode *> &list) const
{
    foreach (ProjectExplorer::FolderNode *folder, parent->subFolderNodes())
        gatherFileNodes(folder, list);
    foreach (ProjectExplorer::FileNode *file, parent->fileNodes())
        list.append(file);
}

bool VsProject::supportsKit(ProjectExplorer::Kit *k, QString *errorMessage) const
{
    Q_UNUSED(k);
    Q_UNUSED(errorMessage);

    // The kit system doesn't really apply, we are using Visual Studio directly
    return true;
}

bool VsProject::hasBuildTarget(const QString &title) const
{
    return Utils::anyOf(buildTargets(), [title](const VsBuildTarget &target) { return target.title == title; });
}

VsBuildTarget VsProject::buildTargetForTitle(const QString &title) const
{
    foreach (const VsBuildTarget &target, buildTargets())
        if (target.title == title)
            return target;
    return VsBuildTarget();
}


void VsProject::updateApplicationAndDeploymentTargets()
{
    ProjectExplorer::Target *t = activeTarget();
    if (!t)
        return;

    QFile deploymentFile;
    QTextStream deploymentStream;
    QString deploymentPrefix;

    QDir sourceDir(t->project()->projectDirectory().toString());
    QDir buildDir(t->activeBuildConfiguration()->buildDirectory().toString());

    deploymentFile.setFileName(sourceDir.filePath(QLatin1String("QtCreatorDeployment.txt")));
    // If we don't have a global QtCreatorDeployment.txt check for one created by the active build configuration
    if (!deploymentFile.exists())
        deploymentFile.setFileName(buildDir.filePath(QLatin1String("QtCreatorDeployment.txt")));
    if (deploymentFile.open(QFile::ReadOnly | QFile::Text)) {
        deploymentStream.setDevice(&deploymentFile);
        deploymentPrefix = deploymentStream.readLine();
        if (!deploymentPrefix.endsWith(QLatin1Char('/')))
            deploymentPrefix.append(QLatin1Char('/'));
    }

    ProjectExplorer::BuildTargetInfoList appTargetList;
    ProjectExplorer::DeploymentData deploymentData;

    foreach (const VsBuildTarget &vt, buildTargets()) {
        if (vt.output.isEmpty())
            continue;

        if (vt.targetType == ExecutableType || vt.targetType == DynamicLibraryType)
            deploymentData.addFile(vt.output, deploymentPrefix + buildDir.relativeFilePath(QFileInfo(vt.output).dir().path()), ProjectExplorer::DeployableFile::TypeExecutable);
        if (vt.targetType == ExecutableType) {
            // TODO: Put a path to corresponding .cbp file into projectFilePath?
            appTargetList.list << ProjectExplorer::BuildTargetInfo(vt.title,
                                                  Utils::FileName::fromString(vt.output),
                                                  Utils::FileName::fromString(vt.output));
        }
    }

    QString absoluteSourcePath = sourceDir.absolutePath();
    if (!absoluteSourcePath.endsWith(QLatin1Char('/')))
        absoluteSourcePath.append(QLatin1Char('/'));
    if (deploymentStream.device()) {
        while (!deploymentStream.atEnd()) {
            QString line = deploymentStream.readLine();
            if (!line.contains(QLatin1Char(':')))
                continue;
            QStringList file = line.split(QLatin1Char(':'));
            deploymentData.addFile(absoluteSourcePath + file.at(0), deploymentPrefix + file.at(1));
        }
    }

    t->setApplicationTargets(appTargetList);
    t->setDeploymentData(deploymentData);
}

void VsProject::updateTargetRunConfigurations(ProjectExplorer::Target *t)
{
    // *Update* existing runconfigurations (no need to update new ones!):
    QHash<QString, const VsBuildTarget *> buildTargetHash;
    const QList<VsBuildTarget> buildTargetList = buildTargets();
    foreach (const VsBuildTarget &bt, buildTargetList) {
        if (bt.targetType != ExecutableType || bt.output.isEmpty())
            continue;

        buildTargetHash.insert(bt.title, &bt);
    }

    foreach (ProjectExplorer::RunConfiguration *rc, t->runConfigurations()) {
        auto vsrc = qobject_cast<VsRunConfiguration *>(rc);
        if (!vsrc)
            continue;

        auto btIt = buildTargetHash.constFind(vsrc->title());
        vsrc->setEnabled(btIt != buildTargetHash.constEnd());
        if (btIt != buildTargetHash.constEnd()) {
            vsrc->setExecutable(btIt.value()->output);
            vsrc->setBaseWorkingDirectory(projectFilePath().toFileInfo().absoluteDir().path());
        }
    }

    // create new and remove obsolete RCs using the factories
    t->updateDefaultRunConfigurations();
}

