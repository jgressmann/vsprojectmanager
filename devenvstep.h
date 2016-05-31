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

#include <projectexplorer/abstractprocessstep.h>
#include <projectexplorer/task.h>

QT_BEGIN_NAMESPACE
class QLineEdit;
QT_END_NAMESPACE

#include "vsprojectdata.h"

namespace VsProjectManager {
namespace Internal {

class VsProject;
class DevenvStep;

class DevenvStepFactory : public ProjectExplorer::IBuildStepFactory
{
    Q_OBJECT

public:
    DevenvStepFactory(QObject *parent = 0);

    QList<Core::Id> availableCreationIds(ProjectExplorer::BuildStepList *bc) const override;
    QString displayNameForId(Core::Id id) const override;

    bool canCreate(ProjectExplorer::BuildStepList *parent, Core::Id id) const override;
    ProjectExplorer::BuildStep *create(ProjectExplorer::BuildStepList *parent, Core::Id id) override;
    bool canClone(ProjectExplorer::BuildStepList *parent, ProjectExplorer::BuildStep *source) const override;
    ProjectExplorer::BuildStep *clone(ProjectExplorer::BuildStepList *parent, ProjectExplorer::BuildStep *source) override;
    bool canRestore(ProjectExplorer::BuildStepList *parent, const QVariantMap &map) const override;
    ProjectExplorer::BuildStep *restore(ProjectExplorer::BuildStepList *parent, const QVariantMap &map) override;
};


class DevenvStep : public ProjectExplorer::AbstractProcessStep
{
    Q_OBJECT
    friend class DevenvStepFactory;
    friend class DevenvStepConfigWidget;

public:
    explicit DevenvStep(ProjectExplorer::BuildStepList *bsl);

    virtual bool init(QList<const BuildStep *> &earlierSteps) override;

    void run(QFutureInterface<bool> &interface) override;
    ProjectExplorer::BuildStepConfigWidget *createConfigWidget() override;
    QVariantMap toMap() const override;
    void setClean(bool clean);
    void setConfiguration(const QString& configuration);
    bool immutable() const override;


protected:
    DevenvStep(ProjectExplorer::BuildStepList *bsl, DevenvStep *bs);
    DevenvStep(ProjectExplorer::BuildStepList *bsl, Core::Id id);
    bool fromMap(const QVariantMap &map) override;

private:
    void ctor();

private:
    QString m_configuration;
    bool m_clean = false;
};


class DevenvStepConfigWidget : public ProjectExplorer::BuildStepConfigWidget
{
    Q_OBJECT

public:
    DevenvStepConfigWidget(DevenvStep *DevenvStep);

    QString displayName() const override;
    QString summaryText() const override;

private:
    void updateDetails();

    DevenvStep *m_devenvStep;
    QString m_summaryText;
    QLineEdit *m_additionalArguments;
};

} // namespace Internal
} // namespace VsProjectManager
