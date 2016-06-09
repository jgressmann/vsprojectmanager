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

#include "vsprojectdata.h"

#include <QFile>

#include <algorithm>

#include <Windows.h>

#ifndef _countof
#   define _countof(x) ((sizeof(x)/sizeof(x[0])))
#endif

namespace VsProjectManager {
namespace Internal {

//#define VSDEBUG

#ifdef VSDEBUG
#include <stdio.h>
#endif

namespace {
#ifdef VSDEBUG
void Indent(FILE* f, int level)
{
    for (int i = 0; i < level; ++i)
        fprintf(f, "  ");
}

void DumpXml(FILE* f, const QDomNode& node, int level)
{
    if (node.isElement()) {
        Indent(f, level); fprintf(f, "<%s", qPrintable(node.nodeName()));
        if (node.hasAttributes()) {
            fprintf(stdout, " ");
            QDomNamedNodeMap attrs = node.attributes();
            for (int i = 0; i < attrs.size(); ++i) {
                QDomNode attr = attrs.item(i);
                fprintf(stdout, "\"%s\"=\"%s\"", qPrintable(attr.nodeName()), qPrintable(attr.nodeValue()));
            }
        }
        fprintf(f, ">\n");
        QDomNodeList children = node.childNodes();
        for (int i = 0; i < children.size(); ++i) {
            DumpXml(f, children.at(i), level + 1);
        }
        Indent(f, level); fprintf(f, "</%s>\n", qPrintable(node.nodeName()));
    } else if (node.isText()) {
        fprintf(f, "%s", qPrintable(node.nodeValue()));
    }
}
#endif

struct HandleData {
    DWORD processId;
    HWND bestHandle;
};

#pragma comment(lib, "User32.lib")


BOOL isMainWindow(HWND handle)
{
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}


BOOL CALLBACK enumWindowsCallback(HWND handle, LPARAM lParam)
{
    HandleData& data = *(HandleData*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.processId != process_id || !isMainWindow(handle)) {
        return TRUE;
    }
    data.bestHandle = handle;
    return FALSE;
}

HWND findMainWindow(DWORD processId)
{
    HandleData data;
    data.processId = processId;
    data.bestHandle = nullptr;
    EnumWindows(enumWindowsCallback, (LPARAM)&data);
    return data.bestHandle;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
VsProjectData::~VsProjectData()
{
    // So that the compiler knows where to put the vtable
    releaseDevenvProcess();
}

VsProjectData::VsProjectData(const Utils::FileName& projectFilePath, const QDomDocument& doc) :
    m_doc(doc),
    m_projectFilePath(projectFilePath),
    m_projectDirectory(projectFilePath.toFileInfo().absoluteDir())
{ }

VsProjectData* VsProjectData::load(const Utils::FileName& projectFilePath)
{
    QFileInfo info(projectFilePath.toFileInfo());
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("%s: %s", qPrintable(info.absoluteFilePath()), qPrintable(file.errorString()));
        return nullptr;
    }

    QDomDocument doc;
    doc.setContent(&file);

#ifdef VSDEBUG
    FILE* f = fopen("c:\\temp\\proj.xml", "w");
    if (f) {
        DumpXml(f, doc.documentElement(), 0);
        fclose(f);
    }
#endif

    auto root = doc.documentElement();
    if (root.nodeName() == QLatin1String("VisualStudioProject")) {
        auto version = root.attributes().namedItem(QLatin1String("Version")).nodeValue().replace(QLatin1Char(','), QLatin1Char('.'));
        if (version == QLatin1String("8.00")) {
            return new Vs2005ProjectData(projectFilePath, doc);
        } else {
            qWarning("Don't know how to parse version %s project files", qPrintable(version));
            return nullptr;
        }
    }

    if (root.nodeName() == QLatin1String("Project")) {
        auto version = root.attributes().namedItem(QLatin1String("ToolsVersion")).nodeValue().replace(QLatin1Char(','), QLatin1Char('.'));
        if (version == QLatin1String("4.0")) { // VS2010
            return new Vs2010ProjectData(projectFilePath, doc, "VS100COMNTOOLS");
        } else if (version == QLatin1String("12.0")) { // VS2013
            return new Vs2010ProjectData(projectFilePath, doc, "VS120COMNTOOLS");
        } else if (version == QLatin1String("14.0")) { // VS2015
            return new Vs2010ProjectData(projectFilePath, doc, "VS140COMNTOOLS");
        } else {
            return nullptr;
        }
    }

    return nullptr;
}

void VsProjectData::splitConfiguration(const QString& configuration, QString* configurationName, QString* platformName)
{
    QString platform = QLatin1String("Win32");
    QString confName = configuration;
    int pipeIndex;
    if ((pipeIndex = configuration.indexOf(QLatin1String("|"))) >= 0) {
        confName = configuration.left(pipeIndex);
        platform = configuration.right(configuration.length() - pipeIndex - 1);
    }

    if (platformName) *platformName = platform;
    if (configurationName) *configurationName = confName;
}

QString VsProjectData::makeAbsoluteFilePath(const QString& input) const
{
    auto output = QDir::fromNativeSeparators(input);
    if (QDir::isRelativePath(output)) {
        output = QDir::cleanPath(projectDirectory().absoluteFilePath(output));
    }
    return output;
}

QString VsProjectData::substitute(QString input, const VariableSubstitution& sub)
{
    VariableSubstitution::const_iterator beg = sub.begin();
    VariableSubstitution::const_iterator end = sub.end();
    QString output;

    while (1) {
        output = input;

        for (VariableSubstitution::const_iterator it = beg; it != end; ++it) {
            output = output.replace(it.key(), it.value());
        }

        if (output == input) {
            break;
        }

        input = output;
    }

    return output;
}

void VsProjectData::addDefaultIncludeDirectories(QStringList& includes) const
{
    includes << m_installDirectory.absolutePath() + QLatin1String("/VC/include");
    includes << m_installDirectory.absolutePath() + QLatin1String("/VC/atlmfc/include");
}

void VsProjectData::openInDevenv()
{
    if (!m_devenvProcess) {
        if (m_installDirectory.exists()) {
            m_devenvProcess = new QProcess();
            connect(m_devenvProcess, &QProcess::errorOccurred, this, &VsProjectData::devenvProcessErrorOccurred);
            connect(m_devenvProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &VsProjectData::devenvProcessFinished);
            m_devenvProcess->start(
                        QDir::toNativeSeparators(m_installDirectory.absoluteFilePath(QLatin1String("Common7/IDE/devenv.exe"))),
                        QStringList() << QDir::toNativeSeparators(m_projectFilePath.toFileInfo().absoluteFilePath()),
                        QProcess::NotOpen);
        }
    } else {
        HWND mainWindow = findMainWindow(static_cast<DWORD>(m_devenvProcess->processId()));
        if (mainWindow) {
            SetForegroundWindow(mainWindow);
        }
    }
}

void VsProjectData::devenvProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    releaseDevenvProcess();
}

void VsProjectData::devenvProcessErrorOccurred(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    releaseDevenvProcess();
}

void VsProjectData::releaseDevenvProcess()
{
    if (m_devenvProcess) {
        disconnect(m_devenvProcess, &QProcess::errorOccurred, this, &VsProjectData::devenvProcessErrorOccurred);
        disconnect(m_devenvProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &VsProjectData::devenvProcessFinished);
        delete m_devenvProcess;
        m_devenvProcess = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////
Vs2005ProjectData::Vs2005ProjectData(const Utils::FileName& projectFile, const QDomDocument& doc)
    : VsProjectData(projectFile, doc)
{
    auto toolsPath = qgetenv("VS80COMNTOOLS");
    auto installDir = QDir(QString::fromLocal8Bit(toolsPath));
    installDir.cdUp();
    installDir.cdUp();
    setInstallDir(installDir);

    m_vcvarsPath  = QDir::toNativeSeparators(installDir.absoluteFilePath(QLatin1String("VC/vcvarsall.bat")));
    m_solutionDir = projectDirectory().path();

    auto configurationNodes = doc.documentElement().namedItem(QLatin1String("Configurations")).childNodes();
    m_targets.reserve(configurationNodes.size());

    for (auto i = 0; i < configurationNodes.count(); ++i) {
        const auto& configNode = configurationNodes.at(i);
        auto key = configNode.attributes().namedItem(QLatin1String("Name")).nodeValue();

        m_configurations << key;
        QString platform, configuration;
        splitConfiguration(key, &configuration, &platform);

        VariableSubstitution sub;
        sub.insert(QLatin1String("$(ConfigurationName)"), configuration);
        sub.insert(QLatin1String("$(PlatformName)"), platform);

        sub.insert(QLatin1String("$(ProjectDir)"), projectDirectory().path() + QLatin1String("/"));
        sub.insert(QLatin1String("$(SolutionDir)"), m_solutionDir  + QLatin1String("/"));
        sub.insert(QLatin1String("$(ProjectName)"), projectFile.toFileInfo().baseName());
        sub.insert(QLatin1String("$(OutDir)"), QLatin1String("$(SolutionDir)$(ConfigurationName)/"));
        sub.insert(QLatin1String("$(IntDir)"), QLatin1String("$(ConfigurationName)/"));


        VsBuildTarget target;
        target.configuration = key;
        target.title = projectFile.toFileInfo().baseName();
        target.output = QLatin1String("$(OutDir)$(ProjectName)$(TargetExt)");
        target.outdir = configNode.attributes().namedItem(QLatin1String("OutputDirectory")).nodeValue();

        auto configurationType = configNode.attributes().namedItem(QLatin1String("ConfigurationType")).nodeValue().toInt();
        switch (configurationType) {
        case 1:
            target.targetType = TargetType::ExecutableType;
            sub.insert(QLatin1String("$(TargetExt)"), QLatin1String(".exe"));
            break;
        case 2:
            target.targetType = TargetType::DynamicLibraryType;
            sub.insert(QLatin1String("$(TargetExt)"), QLatin1String(".dll"));
            break;
        case 3:
            target.targetType = TargetType::StaticLibraryType;
            sub.insert(QLatin1String("$(TargetExt)"), QLatin1String(".lib"));
            break;
        case 4:
            target.targetType = TargetType::UtilityType;
            break;
        default:
            target.targetType = TargetType::Other;
            break;
        }

        auto charset = configNode.attributes().namedItem(QLatin1String("CharacterSet")).nodeValue().toInt();
        switch (charset) {
        case 1:
            target.defines += "#define _UNICODE\n#define UNICODE\n";
            break;
        case 2:
            target.defines += "#define _MBCS\n";
            break;
        }

        auto useOfMfc = configNode.attributes().namedItem(QLatin1String("UseOfMFC")).nodeValue().toInt();
        if (useOfMfc == 2) { // shared DLL
            target.defines += "#define _AFXDLL\n";
        }

        // parse project type (executable), include dirs, defines, c(xx)flags
        auto configNodeChildren = configNode.childNodes();
        for (auto j = 0; j < configNodeChildren.count(); ++j) {
            const auto& configChildNode = configNodeChildren.at(j);
            if (configChildNode.nodeType() == QDomNode::ElementNode &&
                configChildNode.nodeName() == QLatin1String("Tool")) {

                const auto& toolNode = configChildNode;
                auto toolName = configChildNode.attributes().namedItem(QLatin1String("Name")).nodeValue();
                if (toolName == QLatin1String("VCCLCompilerTool")) {
                    auto additionalIncludeDirs = configChildNode.attributes().namedItem(QLatin1String("AdditionalIncludeDirectories")).nodeValue();
                    auto additionalIncludeDirsList = additionalIncludeDirs.splitRef(QLatin1Char(';'), QString::SkipEmptyParts);
                    if (additionalIncludeDirsList.size() == 1) {
                        additionalIncludeDirsList = additionalIncludeDirs.splitRef(QLatin1Char(','), QString::SkipEmptyParts);
                    }

                    target.includeDirectories.reserve(additionalIncludeDirsList.size());
                    foreach (const QStringRef& ref, additionalIncludeDirsList) {
                        target.includeDirectories << makeAbsoluteFilePath(ref.toString());
                    }

                    // default includes
                    addDefaultIncludeDirectories(target.includeDirectories);
                    target.includeDirectories.append(installDir.absolutePath() + QLatin1String("/VC/PlatformSDK/include"));

                    auto defines = configChildNode.attributes().namedItem(QLatin1String("PreprocessorDefinitions")).nodeValue().split(QLatin1Char(';'));
                    foreach (const QString& define, defines) {
                        if (target.defines.size())
                            target.defines += '\n';
                        target.defines.append("#define ");
                        target.defines.append(define.toLocal8Bit().replace('=', ' '));
                    }

                    auto runtimeLibrary = configChildNode.attributes().namedItem(QLatin1String("RuntimeLibrary")).nodeValue().toInt();
                    switch (runtimeLibrary) {
                    case 0:
                        target.compilerOptions += QLatin1String("/MT");
                        break;
                    case 1:
                        target.compilerOptions += QLatin1String("/MTd");
                        break;
                    case 2:
                        target.compilerOptions += QLatin1String("/MD");
                        break;
                    case 3:
                        target.compilerOptions += QLatin1String("/MDd");
                        break;
                    }
                } else if (toolName == QLatin1String("VCLinkerTool")) {
                    switch (target.targetType) {
                    case TargetType::DynamicLibraryType:
                    case TargetType::ExecutableType: {
                            QString outfile = toolNode.attributes().namedItem(QLatin1String("OutputFile")).nodeValue();
                            if (!outfile.isEmpty()) {
                                target.output = outfile;
                            }
                        }
                        break;
                    }
                } else if (toolName == QLatin1String("VCLibrarianTool")) {
                    if (TargetType::StaticLibraryType == target.targetType) {
                        QString outfile = toolNode.attributes().namedItem(QLatin1String("OutputFile")).nodeValue();
                        if (!outfile.isEmpty()) {
                            target.output = outfile;
                        }
                    }
                }
            }
        }

        // parse <Files> section in the context of this configuration
        auto filesChildNodes = doc.documentElement().namedItem(QLatin1String("Files")).childNodes();
        parseFilter(filesChildNodes, key, m_files);

        target.outdir = substitute(target.outdir, sub);
        target.outdir = makeAbsoluteFilePath(target.outdir);
        target.output = substitute(target.output, sub);
        target.output = makeAbsoluteFilePath(target.output);

        m_targets << target;
    }

    std::sort(m_files.begin(), m_files.end());
    m_files.erase(std::unique(m_files.begin(), m_files.end()), m_files.end());
}

void Vs2005ProjectData::parseFilter(
        const QDomNodeList& xmlItems,
        const QString& configuration,
        QStringList& files) const
{
    QFileInfo fi;
    for (auto i = 0; i < xmlItems.count(); ++i) {
        const auto& fileNode = xmlItems.at(i);
        if (fileNode.nodeType() == QDomNode::ElementNode) {
            if (fileNode.nodeName() == QLatin1String("File")) {

                auto relPath = fileNode.attributes().namedItem(QLatin1String("RelativePath")).nodeValue();
                fi.setFile(projectDirectory(), relPath);

                auto include = true;
                auto fileNodeChildren = fileNode.childNodes();
                for (auto j = 0; j < fileNodeChildren.count(); ++j) {
                    auto fileNodeChild = fileNodeChildren.at(j);
                    if (fileNode.nodeType() == QDomNode::ElementNode &&
                        fileNodeChild.nodeName() == QLatin1String("FileConfiguration")) {
                        include = fileNodeChild.attributes().namedItem(QLatin1String("Name")).nodeValue() == configuration;
                        if (include) {
                            break;
                        }
                    }
                }

                if (include) {
                    files << QDir::cleanPath(fi.absoluteFilePath());
                }
            } else {
                 if (fileNode.nodeName() == QLatin1String("Filter")) {
                     parseFilter(fileNode.childNodes(), configuration, files);
                 }
            }
        }
    }
}


VsBuildTargets Vs2005ProjectData::targets() const
{
    return m_targets;
}


QStringList Vs2005ProjectData::files() const
{
    return m_files;
}

void Vs2005ProjectData::buildCmd(const QString& configuration, QString* cmd, QString* args) const
{
    makeCmd(configuration, QString(), *cmd, *args);
}

void Vs2005ProjectData::cleanCmd(const QString& configuration, QString* cmd, QString* args) const
{
    makeCmd(configuration, QLatin1String("/Clean "), *cmd, *args);
}

void Vs2005ProjectData::makeCmd(const QString& configuration, const QString& buildSwitch, QString& cmd, QString& args) const
{
    auto cfgArg = configuration;
    cfgArg.replace(QLatin1String("|"), QLatin1String("^|"));
    if (cfgArg.indexOf(QLatin1Char(' ')) >= 0) {
        cfgArg = QLatin1String("\"") + cfgArg + QLatin1String("\"");
    }

    cmd = QLatin1String("%comspec%");
    args = QString::fromLatin1("/c \"call \"%1\" & vcbuild \"%2\" /nologo %3%4\"").arg(
                m_vcvarsPath,
                QDir::toNativeSeparators(projectFilePath().toFileInfo().absoluteFilePath()),
                buildSwitch,
                cfgArg);
}

QStringList Vs2005ProjectData::configurations() const
{
    return m_configurations;
}

////////////////////////////////////////////////////////////////////////////////
Vs2010ProjectData::Vs2010ProjectData(
        const Utils::FileName& projectFile,
        const QDomDocument& doc,
        const char* toolsEnvVarName)
    : VsProjectData(projectFile, doc)
{
    auto toolsPath = qgetenv(toolsEnvVarName);
    auto installDir = QDir(QString::fromLocal8Bit(toolsPath));
    installDir.cdUp();
    installDir.cdUp();
    setInstallDir(installDir);

    m_vcvarsPath  = QDir::toNativeSeparators(installDir.absoluteFilePath(QLatin1String("VC/vcvarsall.bat")));
    m_solutionDir = projectDirectory().path();

    auto childNodes = doc.documentElement().childNodes();
    // first pass to pick up files and configurations
    for (auto i = 0; i < childNodes.count(); ++i) {
        auto childNode = childNodes.at(i);
        if (childNode.nodeType() == QDomNode::ElementNode) {
            QDomElement element = childNode.toElement();
            if (element.nodeName() == QLatin1String("ItemGroup")) {
                if (element.hasAttributes()) {
                    if (element.attribute(QLatin1String("Label")) == QLatin1String("ProjectConfigurations")) {
                        auto projectConfigurationNodes = element.childNodes();
                        for (auto i = 0; i < projectConfigurationNodes.count(); ++i) {
                            auto childNode = projectConfigurationNodes.at(i);
                            if (childNode.isElement() && childNode.nodeName() == QLatin1String("ProjectConfiguration"))
                                m_configurations << childNode.toElement().attribute(QLatin1String("Include"));
                        }
                    }
                } else { // no attributes
                    auto itemGroupChildNodes = element.childNodes();
                    for (auto i = 0; i < itemGroupChildNodes.count(); ++i) {
                        auto childNode = itemGroupChildNodes.at(i);
                        if (childNode.isElement()) {
                            auto element = childNode.toElement();
                            auto name = element.nodeName();
                            if (/* VS2010 */
                                name == QLatin1String("ClCompile") ||
                                name == QLatin1String("Midl") ||
                                name == QLatin1String("None") ||
                                name == QLatin1String("ResourceCompile") ||
                                /* VS2013 (and up?) */
                                name == QLatin1String("ClInclude") ||
                                name == QLatin1String("FxCompile") /* shaders */ ||
                                name == QLatin1String("Image") ||
                                name == QLatin1String("Text") ||
                                name == QLatin1String("Xml") ||
                                false) {
                                m_files << makeAbsoluteFilePath(element.attribute(QLatin1String("Include")));
                            }
                        }
                    }
                }
            }
        }
    }

    // 2nd pass to pick up targets
    foreach (const QString& configuration, m_configurations) {

        VariableSubstitution sub;
        sub.insert(QLatin1String("$(ProjectDir)"), projectDirectory().path() + QLatin1String("/"));
        sub.insert(QLatin1String("$(SolutionDir)"), m_solutionDir  + QLatin1String("/"));
        sub.insert(QLatin1String("$(ProjectName)"), projectFile.toFileInfo().baseName());
        sub.insert(QLatin1String("$(TargetName)"), QLatin1String("$(ProjectName)"));
        sub.insert(QLatin1String("$(OutDir)"), QLatin1String("$(SolutionDir)$(Configuration)/"));
        sub.insert(QLatin1String("$(IntDir)"), QLatin1String("$(Configuration)/"));

        QString platformName, configurationName;
        splitConfiguration(configuration, &configurationName, &platformName);
        sub.insert(QLatin1String("$(Configuration)"), configurationName);
        sub.insert(QLatin1String("$(ConfigurationName)"), configurationName);
        sub.insert(QLatin1String("$(Platform)"), platformName);
        sub.insert(QLatin1String("$(PlatformName)"), platformName);

        VsBuildTarget target;
        target.targetType = TargetType::Other;
        target.configuration = configuration;
        target.title = projectFile.toFileInfo().baseName();
        target.outdir = QLatin1String("$(OutDir)");
        target.output = QLatin1String("$(OutDir)$(TargetName)$(TargetExt)");

        auto condition = QStringLiteral("'$(Configuration)|$(Platform)'=='%1'").arg(configuration);

        for (auto i = 0; i < childNodes.count(); ++i) {
            auto childNode = childNodes.at(i);
            if (childNode.nodeType() == QDomNode::ElementNode) {
                QDomElement element = childNode.toElement();
                QString elementName = element.nodeName();
                if (elementName == QLatin1String("PropertyGroup")) {
                    if (element.hasAttributes()) {
                        if (element.attribute(QLatin1String("Condition")) == condition) {
                            if (element.hasAttributes()) {
                                if (element.attribute(QLatin1String("Label")) == QLatin1String("Configuration")) {
                                    QString configurationType = element.namedItem(QLatin1String("ConfigurationType")).childNodes().at(0).nodeValue();
                                    if (configurationType == QLatin1String("Application")) {
                                        target.targetType = TargetType::ExecutableType;
                                        sub.insert(QLatin1String("$(TargetExt)"), QLatin1String(".exe"));
                                    } else if (configurationType == QLatin1String("DynamicLibrary")) {
                                        target.targetType = TargetType::DynamicLibraryType;
                                        sub.insert(QLatin1String("$(TargetExt)"), QLatin1String(".dll"));
                                    } else if (configurationType == QLatin1String("StaticLibrary")) {
                                        target.targetType = TargetType::StaticLibraryType;
                                        sub.insert(QLatin1String("$(TargetExt)"), QLatin1String(".lib"));
                                    } else if (configurationType == QLatin1String("Utility")) {
                                        target.targetType = TargetType::UtilityType;
                                    } else {
                                        target.targetType = TargetType::Other;
                                    }

                                    auto charsetNode = element.namedItem(QLatin1String("CharacterSet"));
                                    if (charsetNode.isElement()) {
                                        auto charset = charsetNode.childNodes().at(0).nodeValue();
                                        if (charset == QLatin1String("Unicode")) {
                                            target.defines += "#define _UNICODE\n#define UNICODE\n";
                                        } else if (charset == QLatin1String("MultiByte")) {
                                            target.defines += "#define _MBCS\n";
                                        }
                                    }

                                    auto useOfMfcNode = element.namedItem(QLatin1String("UseOfMfc"));
                                    if (useOfMfcNode.isElement()) {
                                        auto useOfMfc = useOfMfcNode.childNodes().at(0).nodeValue();
                                        if (useOfMfc == QLatin1String("Dynamic")) {
                                            target.defines += "#define _AFXDLL\n";
                                        }
                                    }
                                }
                            }
                        }
                    } else { // no attributes
                        auto propertyGroupChildNodes = element.childNodes();
                        for (auto i = 0; i < propertyGroupChildNodes.count(); ++i) {
                            auto childNode = propertyGroupChildNodes.at(i);
                            if (childNode.nodeType() == QDomNode::ElementNode) {
                                QDomElement element = childNode.toElement();
                                if (element.nodeName() == QLatin1String("TargetName")) {
                                    if (element.attribute(QLatin1String("Condition")) == condition) {
                                        sub[QLatin1String("TargetName")] = element.childNodes().at(0).nodeValue();
                                    }
                                } else if (element.nodeName() == QLatin1String("OutDir")) {
                                    if (element.attribute(QLatin1String("Condition")) == condition) {
                                        target.outdir = QDir::fromNativeSeparators(element.childNodes().at(0).nodeValue());
                                    }
                                }
                            }
                        }
                    }
                } else if (elementName == QLatin1String("ItemDefinitionGroup")) {
                    if (element.attribute(QLatin1String("Condition")) == condition) {
                        QDomElement compileElement = element.namedItem(QLatin1String("ClCompile")).toElement();
                        auto defines = compileElement.namedItem(QLatin1String("PreprocessorDefinitions")).childNodes().at(0).nodeValue().split(QLatin1Char(';'));
                        foreach (const QString define, defines) {
                            if (define == QLatin1String("%(PreprocessorDefinitions")) {
                                continue;
                            }

                            target.defines += "#define ";
                            target.defines += define.toLocal8Bit();
                            target.defines += '\n';
                        }

                        auto includes = compileElement.namedItem(QLatin1String("AdditionalIncludeDirectories")).childNodes().at(0).nodeValue().split(QLatin1Char(';'));
                        foreach (const QString include, includes) {
                            if (include == QLatin1String("%(AdditionalIncludeDirectories")) {
                                continue;
                            }

                            target.includeDirectories << makeAbsoluteFilePath(include);
                        }

                        auto runtimeLibrary = compileElement.namedItem(QLatin1String("RuntimeLibrary")).childNodes().at(0).nodeValue();
                        if (QLatin1String("MultiThreadedDLL") == runtimeLibrary) {
                            target.compilerOptions << QLatin1String("/MD");
                        } else if (QLatin1String("MultiThreadedDebugDLL") == runtimeLibrary) {
                            target.compilerOptions << QLatin1String("/MDd");
                        } else if (QLatin1String("MultiThreaded") == runtimeLibrary) {
                            target.compilerOptions << QLatin1String("/MT");
                        } else if (QLatin1String("MultiThreadedDebug") == runtimeLibrary) {
                            target.compilerOptions << QLatin1String("/MTd");
                        }


                        QDomElement linkElement = element.namedItem(QLatin1String("Link")).toElement();
                        auto outputFileNode = linkElement.namedItem(QLatin1String("OutputFile"));
                        if (outputFileNode.isElement()) {
                            target.output = QDir::fromNativeSeparators(outputFileNode.childNodes().at(0).nodeValue());
                        }
                    }
                }
            }
        }

        addDefaultIncludeDirectories(target.includeDirectories);

        target.outdir = substitute(target.outdir, sub);
        target.outdir = makeAbsoluteFilePath(target.outdir);
        target.output = substitute(target.output, sub);
        target.output = makeAbsoluteFilePath(target.output);

        m_targets << target;
    }

    std::sort(m_files.begin(), m_files.end());
    m_files.erase(std::unique(m_files.begin(), m_files.end()), m_files.end());
}

VsBuildTargets Vs2010ProjectData::targets() const
{
    return m_targets;
}

QStringList Vs2010ProjectData::files() const
{
    return m_files;
}


void Vs2010ProjectData::buildCmd(const QString& configuration, QString* cmd, QString* args) const
{
    makeCmd(configuration, QLatin1String("/t:Build"), *cmd, *args);
}

void Vs2010ProjectData::cleanCmd(const QString& configuration, QString* cmd, QString* args) const
{
    makeCmd(configuration, QLatin1String("/t:Clean"), *cmd, *args);
}

void Vs2010ProjectData::makeCmd(const QString& configuration, const QString& buildSwitch, QString& cmd, QString& args) const
{
    QString configurationName, platformName;
    splitConfiguration(configuration, &configurationName, &platformName);

    // Need to clear VISUALSTUDIOVERSION env var, else wrong msbuild might be picked up
    // https://connect.microsoft.com/VisualStudio/feedback/details/806393/error-trying-to-build-using-msbuild-from-code
    cmd = QLatin1String("%comspec%");
    args = QString::fromLatin1("/c \"set \"VISUALSTUDIOVERSION=\" & call \"%1\" & msbuild \"%2\" /nologo %3 /p:Configuration=\"%4\" /p:Platform=\"%5\"\"").arg(
                m_vcvarsPath,
                QDir::toNativeSeparators(projectFilePath().toFileInfo().absoluteFilePath()),
                buildSwitch,
                configurationName,
                platformName);
}

QStringList Vs2010ProjectData::configurations() const
{
    return m_configurations;
}

} // namespace Internal
} // namespace VsProjectManager
