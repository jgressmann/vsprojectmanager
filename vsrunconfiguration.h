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

#if 0
#include <projectexplorer/runnables.h>
#else
#include <projectexplorer/localapplicationrunconfiguration.h>
#endif


namespace VsProjectManager {
namespace Internal {

#if 0 // 4.0
class VsRunConfiguration : public ProjectExplorer::RunConfiguration
#else
class VsRunConfiguration : public ProjectExplorer::LocalApplicationRunConfiguration
#endif
{
    Q_OBJECT
    friend class VsRunConfigurationWidget;
    friend class VsRunConfigurationFactory;

public:
    VsRunConfiguration(ProjectExplorer::Target *parent, Core::Id id, const QString &target,
                          const QString &workingDirectory, const QString &title);

#if 0 // 4.0
    ProjectExplorer::Runnable runnable() const override;
#else
    virtual QString executable() const override;
    virtual ProjectExplorer::ApplicationLauncher::Mode runMode() const override;
    virtual QString workingDirectory() const override;
    virtual QString commandLineArguments() const override;
#endif
    QWidget *createConfigurationWidget() override;

    void setExecutable(const QString &executable);
    void setBaseWorkingDirectory(const QString &workingDirectory);

    QString title() const;

    QVariantMap toMap() const override;

    void setEnabled(bool b);

    bool isEnabled() const override;
    QString disabledReason() const override;

protected:
    VsRunConfiguration(ProjectExplorer::Target *parent, VsRunConfiguration *source);
    bool fromMap(const QVariantMap &map) override;
    QString defaultDisplayName() const;

private:
    QString baseWorkingDirectory() const;
    void ctor();

    QString m_buildTarget;
    QString m_title;
    bool m_enabled = true;
};

class VsRunConfigurationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VsRunConfigurationWidget(VsRunConfiguration *VsRunConfiguration, QWidget *parent = 0);
};

class VsRunConfigurationFactory : public ProjectExplorer::IRunConfigurationFactory
{
    Q_OBJECT

public:
    explicit VsRunConfigurationFactory(QObject *parent = nullptr);

    bool canCreate(ProjectExplorer::Target *parent, Core::Id id) const override;
    bool canRestore(ProjectExplorer::Target *parent, const QVariantMap &map) const override;
    bool canClone(ProjectExplorer::Target *parent, ProjectExplorer::RunConfiguration *product) const override;
    ProjectExplorer::RunConfiguration *clone(ProjectExplorer::Target *parent,
                                             ProjectExplorer::RunConfiguration *product) override;

    QList<Core::Id> availableCreationIds(ProjectExplorer::Target *parent, CreationMode mode) const override;
    QString displayNameForId(Core::Id id) const override;

    static Core::Id idFromBuildTarget(const QString &target);
    static QString buildTargetFromId(Core::Id id);

private:
    bool canHandle(ProjectExplorer::Target *parent) const;

    ProjectExplorer::RunConfiguration *doCreate(ProjectExplorer::Target *parent, Core::Id id) override;
    ProjectExplorer::RunConfiguration *doRestore(ProjectExplorer::Target *parent,
                                                 const QVariantMap &map) override;
};

} // namespace Internal
} // namespace VsProjectManager
