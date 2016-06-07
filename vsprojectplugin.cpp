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

#include "vsprojectplugin.h"
#include "vsmanager.h"
#include "vsbuildconfiguration.h"
#include "devenvstep.h"
#include "vsrunconfiguration.h"
#include "vsprojectconstants.h"
#include "vsproject.h"

#include <QStringList>
#include <QtPlugin>
#include <QAction>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/command.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projecttree.h>
#include <utils/mimetypes/mimedatabase.h>

using namespace VsProjectManager::Internal;

void VsProjectPlugin::extensionsInitialized()
{ }

bool VsProjectPlugin::initialize(const QStringList &arguments,
                                        QString *errorString)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

    const Core::Context projecTreeContext(ProjectExplorer::Constants::C_PROJECT_TREE);

    Utils::MimeDatabase::addMimeTypes(QLatin1String(":vsprojectmanager/vsprojectmanager.mimetypes.xml"));

    addAutoReleasedObject(new VsBuildConfigurationFactory);
    addAutoReleasedObject(new DevenvStepFactory);
    addAutoReleasedObject(new VsRunConfigurationFactory);
    m_manager = new VsManager();
    addAutoReleasedObject(m_manager);


    Core::ActionContainer *mproject =
            Core::ActionManager::actionContainer(ProjectExplorer::Constants::M_PROJECTCONTEXT);

    //register actions
    Core::Command *command = nullptr;

    m_openInDevenvContextMenu = new QAction(tr("Open in Visual Studio"), this);
    command = Core::ActionManager::registerAction(
                m_openInDevenvContextMenu, Constants::OPENINDEVENVCONTEXTMENU, projecTreeContext);
    mproject->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);

    connect(m_openInDevenvContextMenu, &QAction::triggered,
            m_manager, &VsManager::openInDevenvContextMenu);

    connect(ProjectExplorer::ProjectTree::instance(), &ProjectExplorer::ProjectTree::currentNodeChanged,
            this, &VsProjectPlugin::updateContextActions);

    return true;
}

void VsProjectPlugin::updateContextActions(ProjectExplorer::Node *node, ProjectExplorer::Project *project)
{
    Q_UNUSED(node);
    auto pro = qobject_cast<VsProject *>(project);
    m_openInDevenvContextMenu->setVisible(pro);
    m_manager->setContextProject(pro);
}
