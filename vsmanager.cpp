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

#include "vsmanager.h"
#include "vsproject.h"
#include "vsprojectconstants.h"

using namespace ProjectExplorer;

namespace VsProjectManager {
namespace Internal {

Project *VsManager::openProject(const QString &fileName, QString *errorString)
{
    if (!QFileInfo(fileName).isFile()) {
        if (errorString)
            *errorString = tr("Failed opening project \"%1\": Project is not a file")
                .arg(fileName);
        return 0;
    }

    return new VsProject(this, fileName);
}

QString VsManager::mimeType() const
{
    return QLatin1String(Constants::MIMETYPE);
}

void VsManager::openInDevenvContextMenu()
{
    if (m_contextProject && m_contextProject->vsProjectData())
    {
        m_contextProject->vsProjectData()->openInDevenv();
    }
}

void VsManager::setContextProject(VsProject* project)
{
    m_contextProject = project;
}

} // namespace Internal
} // namespace VsProjectManager
