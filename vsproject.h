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

#pragma once

#include "vsprojectdata.h"

#include <projectexplorer/project.h>
#include <projectexplorer/projectnodes.h>

#include <QFuture>

QT_FORWARD_DECLARE_CLASS(QDir)

namespace Utils {
class FileSystemWatcher;
class FileName;
} // namespace Utils


namespace Core { class IDocument; }
namespace ProjectExplorer {
class Node;
class FolderNode;
class Target;
class Kit;
class BuildInfo;
class IBuildConfigurationFactory;
} // namespace ProjectExplorer

namespace VsProjectManager {
namespace Internal {
class VsConfigurationFactory;
class VsProjectFile;
class VsProjectNode;
class VsManager;
class VsProjectFolder;


class VsProject : public ProjectExplorer::Project
{
    Q_OBJECT

public:
    VsProject(VsManager *manager, const QString &fileName);
    ~VsProject() override;

    QString displayName() const override;
    QStringList files(FilesMode fileMode) const override;

    QStringList buildTargetTitles(bool runnable = false) const;
    QList<VsBuildTarget> buildTargets() const;
    bool hasBuildTarget(const QString &title) const;
    VsBuildTarget buildTargetForTitle(const QString &title) const;

    static QString defaultBuildDirectory(const QString &projectPath);
    bool needsConfiguration() const override;
    bool requiresTargetPanel() const override;
    bool knowsAllBuildExecutables() const override;
    bool supportsKit(ProjectExplorer::Kit *k, QString *errorMessage) const override;
    VsProjectData* vsProjectData() const { return m_vsProjectData; }

protected:
    RestoreResult fromMap(const QVariantMap &map, QString *errorMessage) override;
    virtual bool setupTarget(ProjectExplorer::Target *t);

private:
    void handleActiveTargetChanged();
    void handleActiveBuildConfigurationChanged();
    static ProjectExplorer::FileType getFileType(const QString& fileName);
    void onTargetChanged();
    void updateApplicationAndDeploymentTargets();
    void updateTargetRunConfigurations(ProjectExplorer::Target *t);

    void loadProjectTree();
    void parsingStarted();
    void parsingFinished();
    void onFileChanged(const QString &file);
    void updateCppCodeModel();

    void buildTree();
    void buildTreeRec(ProjectExplorer::FolderNode* parent, const VsProjectFolder* folder) const;
    void gatherFileNodes(ProjectExplorer::FolderNode *parent, QList<ProjectExplorer::FileNode *> &list) const;

private:
    // Watches project files for changes.
    Utils::FileSystemWatcher *m_fileWatcher;

    QFuture<void> m_codeModelFuture;

    ProjectExplorer::Target *m_connectedTarget = nullptr;
    VsProjectData* m_vsProjectData = nullptr;
};

} // namespace Internal
} // namespace VsProjectManager
