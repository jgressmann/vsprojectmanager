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
#include <QBuffer>

#include <algorithm>
#include <stdio.h>
#include <Windows.h>

#pragma comment(lib, "User32.lib")

#ifndef _countof
#   define _countof(x) ((sizeof(x)/sizeof(x[0])))
#endif

namespace VsProjectManager {
namespace Internal {

namespace {

const QString _Configuration(QStringLiteral("$(Configuration)"));
const QString _ConfigurationName(QStringLiteral("$(ConfigurationName)"));
const QString _IntDir(QStringLiteral("$(IntDir)"));
const QString _OutDir(QStringLiteral("$(OutDir)"));
const QString _Platform(QStringLiteral("$(Platform)"));
const QString _PlatformName(QStringLiteral("$(PlatformName)"));
const QString _ProjectDir(QStringLiteral("$(ProjectDir)"));
const QString _ProjectName(QStringLiteral("$(ProjectName)"));
const QString _SolutionDir(QStringLiteral("$(SolutionDir)"));
const QString _TargetExt(QStringLiteral("$(TargetExt)"));
const QString _TargetName(QStringLiteral("$(TargetName)"));
const QString Debug(QStringLiteral("Debug"));
const QString Filter(QStringLiteral("Filter"));
const QString Include(QStringLiteral("Include"));
const QString Release(QStringLiteral("Release"));
const QString Win32(QStringLiteral("Win32"));
const QString x64(QStringLiteral("x64"));


struct HandleData {
    DWORD processId;
    HWND bestHandle;
};

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

bool IsKnownNodeName(const QString& name)
{
    return
            /* VS2010 */
            name == QLatin1String("CustomBuild") ||
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
            false;
}

} // namespace


////////////////////////////////////////////////////////////////////////////////
VsProjectFolder::~VsProjectFolder()
{
    DirectoryMap::iterator it = SubFolders.begin();
    DirectoryMap::iterator end = SubFolders.end();
    while (it != end) {
        delete it.value();
        ++it;
    }
}


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

//    auto data = readFile(projectFilePath.toString());
//    QBuffer file(&data);

    QDomDocument doc;
    doc.setContent(&file);

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
            return new Vs2010ProjectData(projectFilePath, doc, "VS100COMNTOOLS", 1600);
        } else if (version == QLatin1String("11.0")) { // VS2012
            return new Vs2010ProjectData(projectFilePath, doc, "VS110COMNTOOLS", 1700);
        } else if (version == QLatin1String("12.0")) { // VS2013
            return new Vs2010ProjectData(projectFilePath, doc, "VS120COMNTOOLS", 1800);
        } else if (version == QLatin1String("14.0")) { // VS2015
            return new Vs2010ProjectData(projectFilePath, doc, "VS140COMNTOOLS", 1900);
        } else {
            return nullptr;
        }
    }

    return nullptr;
}

void VsProjectData::splitConfiguration(const QString& configuration, QString* configurationName, QString* platformName)
{
    QString platform = Win32;
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

void VsProjectData::addDefaultDefines(
        QByteArray& defines,
        const QString& platform,
        RuntimeLibraryType rt) const
{
    defines += "#define _WIN32\n";
    if (Win32 == platform) {
        defines += "#define _M_IX86\n";
    }

    if (x64 == platform) {
        defines += "#define _M_X64\n";
        defines += "#define _M_AMD64\n"; // not true for VS2005
        defines += "#define _WIN64\n";
    }

    switch (rt) {
    case RTL_MT:
        defines += "#define _MT\n";
        break;
    case RTL_MTd:
        defines += "#define _MT\n";
        defines += "#define _DEBUG\n";
        break;
    case RTL_MD:
        defines += "#define _MT\n";
        defines += "#define _DLL\n";
        break;
    case RTL_MDd:
        defines += "#define _MT\n";
        defines += "#define _DLL\n";
        defines += "#define _DEBUG\n";
        break;
    }
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

Utils::FileNameList VsProjectData::files() const
{
    Utils::FileNameList files;
    collectFiles(files, m_rootFolder);
    return files;
}

void VsProjectData::collectFiles(Utils::FileNameList& files, const VsProjectFolder& folder)
{
    files << folder.Files;
    for (auto subFolder : folder.SubFolders) {
        collectFiles(files, *subFolder);
    }
}

QByteArray VsProjectData::readFile(const QString& filePath)
{
    QByteArray result;
    auto nativePath = QDir::toNativeSeparators(filePath);
    HANDLE fileHandle = CreateFileW(
                nativePath.toStdWString().c_str(),
                GENERIC_READ,
                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL, /* security attrs */
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL /* template */);
    if (INVALID_HANDLE_VALUE != fileHandle) {
        LARGE_INTEGER size;
        if (GetFileSizeEx(fileHandle, &size)) {
            result.resize((int)size.QuadPart);
            OVERLAPPED ov;
            memset(&ov, 0, sizeof(ov));
            for (int i = 0; i < result.size(); ) {
                ov.Offset = i;
                ov.OffsetHigh = static_cast<quint64>(i) >> 32;
                int toRead = result.size() - i;
                DWORD read = 0;
                if (!ReadFile(fileHandle, result.data(), toRead, &read, &ov))
                    break;

                i += read;
            }
        }

        CloseHandle(fileHandle);
    }

    return result;
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
    m_filesToWatch << projectFile.toFileInfo().absoluteFilePath();

    auto configurationNodes = doc.documentElement().namedItem(QLatin1String("Configurations")).childNodes();
    m_targets.reserve(configurationNodes.size());

    for (auto i = 0; i < configurationNodes.count(); ++i) {
        const auto& configNode = configurationNodes.at(i);
        auto key = configNode.attributes().namedItem(QLatin1String("Name")).nodeValue();

        m_configurations << key;
        QString platform, configuration;
        splitConfiguration(key, &configuration, &platform);

        VariableSubstitution sub;
        sub.insert(_ConfigurationName, configuration);
        sub.insert(_PlatformName, platform);
        sub.insert(_ProjectDir, projectDirectory().path() + QLatin1String("/"));
        sub.insert(_SolutionDir, m_solutionDir  + QLatin1String("/"));
        sub.insert(_ProjectName, projectFile.toFileInfo().baseName());
        sub.insert(_OutDir, getDefaultOutputDirectory(platform));
        sub.insert(_IntDir, getDefaultIntDirectory(platform));


        VsBuildTarget target;
        target.configuration = key;
        target.title = projectFile.toFileInfo().baseName();
        target.output = _OutDir + _ProjectName + _TargetExt;
        target.outdir = configNode.attributes().namedItem(QLatin1String("OutputDirectory")).nodeValue();
        target.defines += "#define _MSC_VER=1400\n";

        auto configurationType = configNode.attributes().namedItem(QLatin1String("ConfigurationType")).nodeValue().toInt();
        switch (configurationType) {
        case 1:
            target.targetType = TT_ExecutableType;
            sub.insert(_TargetExt, QLatin1String(".exe"));
            break;
        case 2:
            target.targetType = TT_DynamicLibraryType;
            sub.insert(_TargetExt, QLatin1String(".dll"));
            break;
        case 3:
            target.targetType = TT_StaticLibraryType;
            sub.insert(_TargetExt, QLatin1String(".lib"));
            break;
        case 4:
            target.targetType = TT_UtilityType;
            break;
        default:
            target.targetType = TT_Other;
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
                        target.defines.append("#define ");
                        target.defines.append(define.toLocal8Bit().replace('=', ' '));
                        target.defines += '\n';
                    }

                    auto runtimeLibrary = configChildNode.attributes().namedItem(QLatin1String("RuntimeLibrary")).nodeValue().toInt();
                    auto rtl = RuntimeLibraryType::RTL_Other;
                    switch (runtimeLibrary) {
                    case 0:
                        target.compilerOptions += QLatin1String("/MT");
                        rtl = RTL_MT;
                        break;
                    case 1:
                        target.compilerOptions += QLatin1String("/MTd");
                        rtl = RTL_MTd;
                        break;
                    case 2:
                        target.compilerOptions += QLatin1String("/MD");
                        rtl = RTL_MD;
                        break;
                    case 3:
                        target.compilerOptions += QLatin1String("/MDd");
                        rtl = RTL_MDd;
                        break;
                    }

                    addDefaultDefines(target.defines, platform, rtl);

                } else if (toolName == QLatin1String("VCLinkerTool")) {
                    switch (target.targetType) {
                    case TT_DynamicLibraryType:
                    case TT_ExecutableType: {
                            QString outfile = toolNode.attributes().namedItem(QLatin1String("OutputFile")).nodeValue();
                            if (!outfile.isEmpty()) {
                                target.output = outfile;
                            }
                        }
                        break;
                    }
                } else if (toolName == QLatin1String("VCLibrarianTool")) {
                    if (TT_StaticLibraryType == target.targetType) {
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
        parseFilter(filesChildNodes, key, *rootFolder());

        target.outdir = substitute(target.outdir, sub);
        target.outdir = makeAbsoluteFilePath(target.outdir);
        target.output = substitute(target.output, sub);
        target.output = makeAbsoluteFilePath(target.output);

        m_targets << target;
    }
}

void Vs2005ProjectData::parseFilter(
        const QDomNodeList& xmlItems,
        const QString& configuration,
        VsProjectFolder& parentFolder) const
{
    QFileInfo fi;
    for (auto i = 0; i < xmlItems.count(); ++i) {
        const auto& node = xmlItems.at(i);
        if (node.nodeType() == QDomNode::ElementNode) {
            if (node.nodeName() == QLatin1String("File")) {

                auto relPath = node.attributes().namedItem(QLatin1String("RelativePath")).nodeValue();
                fi.setFile(projectDirectory(), relPath);

                auto include = true;
                auto fileNodeChildren = node.childNodes();
                for (auto j = 0; j < fileNodeChildren.count(); ++j) {
                    auto fileNodeChild = fileNodeChildren.at(j);
                    if (node.nodeType() == QDomNode::ElementNode &&
                        fileNodeChild.nodeName() == QLatin1String("FileConfiguration")) {
                        include = fileNodeChild.attributes().namedItem(QLatin1String("Name")).nodeValue() == configuration;
                        if (include) {
                            break;
                        }
                    }
                }

                if (include) {
                    parentFolder.Files << Utils::FileName::fromString(QDir::cleanPath(fi.absoluteFilePath()));
                }
            } else {
                 if (node.nodeName() == QLatin1String("Filter")) {
                     auto filterName = node.attributes().namedItem(QLatin1String("Name")).nodeValue();
                     VsProjectFolder* filterFolder = new VsProjectFolder();
                     parentFolder.SubFolders.insert(filterName, filterFolder);
                     parseFilter(node.childNodes(), configuration, *filterFolder);
                 }
            }
        }
    }

    // remove dups
    std::sort(parentFolder.Files.begin(), parentFolder.Files.end());
    parentFolder.Files.erase(std::unique(parentFolder.Files.begin(), parentFolder.Files.end()), parentFolder.Files.end());
}

VsBuildTargets Vs2005ProjectData::targets() const
{
    return m_targets;
}

QStringList Vs2005ProjectData::filesToWatch() const
{
    return m_filesToWatch;
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

QString Vs2005ProjectData::getDefaultOutputDirectory(const QString& platform)
{
    QString result = _SolutionDir;
    if (Win32 != platform) {
        result += _PlatformName;
        result += QLatin1Char('/');
    }

    result += _ConfigurationName;
    result += QLatin1Char('/');
    return result;
}

QString Vs2005ProjectData::getDefaultIntDirectory(const QString& platform)
{
    QString result;
    if (Win32 != platform) {
        result += _PlatformName;
        result += QLatin1Char('/');
    }

    result += _ConfigurationName;
    result += QLatin1Char('/');
    return result;
}

////////////////////////////////////////////////////////////////////////////////
Vs2010ProjectData::Vs2010ProjectData(
        const Utils::FileName& projectFile,
        const QDomDocument& doc,
        const char* toolsEnvVarName,
        unsigned mscVer)
    : VsProjectData(projectFile, doc)
{
    auto toolsPath = qgetenv(toolsEnvVarName);
    auto installDir = QDir(QString::fromLocal8Bit(toolsPath));
    installDir.cdUp();
    installDir.cdUp();
    setInstallDir(installDir);

    m_vcvarsPath  = QDir::toNativeSeparators(installDir.absoluteFilePath(QLatin1String("VC/vcvarsall.bat")));
    m_solutionDir = projectDirectory().path();
    m_filesToWatch << projectFile.toFileInfo().absoluteFilePath();

    // _MSC_VER
    char mscVerDefine[32];
    _snprintf_s(mscVerDefine, _countof(mscVerDefine), _TRUNCATE, "#define _MSC_VER=%u\n", mscVer);

    Utils::FileNameList files;

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
                                m_configurations << childNode.toElement().attribute(Include);
                        }
                    }
                } else { // no attributes
                    auto itemGroupChildNodes = element.childNodes();
                    for (auto i = 0; i < itemGroupChildNodes.count(); ++i) {
                        auto childNode = itemGroupChildNodes.at(i);
                        if (childNode.isElement()) {
                            auto element = childNode.toElement();
                            auto name = element.nodeName();
                            if (IsKnownNodeName(name)) {
                                files << Utils::FileName::fromString(makeAbsoluteFilePath(element.attribute(Include)));
                            }
                        }
                    }
                }
            }
        }
    }

    // 2nd pass to pick up targets
    foreach (const QString& configuration, m_configurations) {
        QString platformName, configurationName;
        splitConfiguration(configuration, &configurationName, &platformName);

        VariableSubstitution sub;
        sub.insert(_Configuration, configurationName);
        sub.insert(_ConfigurationName, configurationName);
        sub.insert(_Platform, platformName);
        sub.insert(_PlatformName, platformName);
        sub.insert(_ProjectDir, projectDirectory().path() + QLatin1String("/"));
        sub.insert(_SolutionDir, m_solutionDir  + QLatin1String("/"));
        sub.insert(_ProjectName, projectFile.toFileInfo().baseName());
        sub.insert(_TargetName, _ProjectName);
        sub.insert(_OutDir, getDefaultOutputDirectory(platformName));
        sub.insert(_IntDir, getDefaultIntDirectory(platformName));


        VsBuildTarget target;
        target.targetType = TT_Other;
        target.configuration = configuration;
        target.title = projectFile.toFileInfo().baseName();
        target.outdir = _OutDir;
        target.output = _OutDir + _TargetName + _TargetExt;
        target.defines += mscVerDefine;

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
                                        target.targetType = TT_ExecutableType;
                                        sub.insert(_TargetExt, QLatin1String(".exe"));
                                    } else if (configurationType == QLatin1String("DynamicLibrary")) {
                                        target.targetType = TT_DynamicLibraryType;
                                        sub.insert(_TargetExt, QLatin1String(".dll"));
                                    } else if (configurationType == QLatin1String("StaticLibrary")) {
                                        target.targetType = TT_StaticLibraryType;
                                        sub.insert(_TargetExt, QLatin1String(".lib"));
                                    } else if (configurationType == QLatin1String("Utility")) {
                                        target.targetType = TT_UtilityType;
                                    } else {
                                        target.targetType = TT_Other;
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
                                        sub[_TargetName] = element.childNodes().at(0).nodeValue();
                                    }
                                } else if (element.nodeName() == QLatin1String("OutDir")) {
                                    if (element.attribute(QLatin1String("Condition")) == condition) {
                                        sub[_OutDir] = QDir::fromNativeSeparators(element.childNodes().at(0).nodeValue());
                                    }
                                } else if (element.nodeName() == QLatin1String("IntDir")) {
                                    if (element.attribute(QLatin1String("Condition")) == condition) {
                                        sub[_IntDir] = QDir::fromNativeSeparators(element.childNodes().at(0).nodeValue());
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
                            if (define == QLatin1String("%(PreprocessorDefinitions)")) {
                                continue;
                            }

                            target.defines += "#define ";
                            target.defines += define.toLocal8Bit();
                            target.defines += '\n';
                        }

                        auto includes = compileElement.namedItem(QLatin1String("AdditionalIncludeDirectories")).childNodes().at(0).nodeValue().split(QLatin1Char(';'));
                        foreach (const QString include, includes) {
                            if (include == QLatin1String("%(AdditionalIncludeDirectories)")) {
                                continue;
                            }

                            target.includeDirectories << makeAbsoluteFilePath(include);
                        }

                        auto runtimeLibraryNode = compileElement.namedItem(QLatin1String("RuntimeLibrary"));
                        auto rtl = RTL_Other;
                        if (runtimeLibraryNode.isElement()) {
                            auto runtimeLibrary = runtimeLibraryNode.childNodes().at(0).nodeValue();
                            if (QLatin1String("MultiThreadedDLL") == runtimeLibrary) {
                                target.compilerOptions << QLatin1String("/MD");
                                rtl = RTL_MD;
                            } else if (QLatin1String("MultiThreadedDebugDLL") == runtimeLibrary) {
                                target.compilerOptions << QLatin1String("/MDd");
                                rtl = RTL_MDd;
                            } else if (QLatin1String("MultiThreaded") == runtimeLibrary) {
                                target.compilerOptions << QLatin1String("/MT");
                                rtl = RTL_MT;
                            } else if (QLatin1String("MultiThreadedDebug") == runtimeLibrary) {
                                target.compilerOptions << QLatin1String("/MTd");
                                rtl = RTL_MTd;
                            }
                        } else { // omitted, assume defaults derived from configuration names
                            if (configurationName == Debug) {
                                target.compilerOptions << QLatin1String("/MDd");
                                rtl = RTL_MDd;
                            } else if (configurationName == Release) {
                                target.compilerOptions << QLatin1String("/MD");
                                rtl = RTL_MD;
                            }
                        }

                        addDefaultDefines(target.defines, platformName, rtl);

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

    // build project folder hierarchy
    QFileInfo filterFileInfo(projectDirectory().filePath(projectFile.toFileInfo().fileName() + QStringLiteral(".filters")));
    if (filterFileInfo.exists()) {
        m_filesToWatch << filterFileInfo.absoluteFilePath();

        QFile file(filterFileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning("%s: %s", qPrintable(filterFileInfo.absoluteFilePath()), qPrintable(file.errorString()));
            // backup plan, all files in root dir
            rootFolder()->Files << files;
        } else {
            QDomDocument doc;
            doc.setContent(&file);

//            QHash<QString, void*> filterNames;

            childNodes = doc.documentElement().childNodes();
            // first pass to pick up files and configurations
            for (auto i = 0; i < childNodes.count(); ++i) {
                auto childNode = childNodes.at(i);
                if (childNode.nodeType() == QDomNode::ElementNode) {
                    QDomElement element = childNode.toElement();
                    if (element.nodeName() == QLatin1String("ItemGroup")) {
                        auto itemGroupChildNodes = element.childNodes();
                        for (auto i = 0; i < itemGroupChildNodes.count(); ++i) {
                            auto childNode = itemGroupChildNodes.at(i);
                            if (childNode.nodeType() == QDomNode::ElementNode) {
                                QDomElement element = childNode.toElement();
                                auto name = element.nodeName();
                                /*if (name == QLatin1String("Filter")) {
                                    // build folder structure
                                    auto filterName = element.attributes().namedItem(QLatin1Literal("Include")).nodeValue();
                                    VsProjectFolder* parent = rootFolder();
                                    foreach (const QString& pathComponent, filterName.split(QLatin1Char('\\'), QString::SkipEmptyParts)) {
                                        auto it = parent->SubFolders.find(pathComponent);
                                        if (it == parent->SubFolders.end()) {
                                            it = parent->SubFolders.insert(pathComponent, new VsProjectFolder());
                                        }

                                        parent = it.value();
                                    }
                                } else */if (IsKnownNodeName(name)) {
                                    auto filterElement = element.namedItem(QLatin1String("Filter")).toElement();
                                    if (filterElement.isElement()) {
                                        auto relFilePath = element.attributes().namedItem(QLatin1Literal("Include")).nodeValue();
                                        auto filePath = Utils::FileName::fromString(makeAbsoluteFilePath(relFilePath));
                                        auto filterName = filterElement.childNodes().at(0).nodeValue();
                                        VsProjectFolder* parent = rootFolder();
                                        foreach (const QString& pathComponent, filterName.split(QLatin1Char('\\'), QString::SkipEmptyParts)) {
                                            auto it = parent->SubFolders.find(pathComponent);
                                            if (it == parent->SubFolders.end()) {
                                                it = parent->SubFolders.insert(pathComponent, new VsProjectFolder());
                                            }

                                            parent = it.value();
                                        }

                                        parent->Files << filePath;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

VsBuildTargets Vs2010ProjectData::targets() const
{
    return m_targets;
}

QStringList Vs2010ProjectData::filesToWatch() const
{
    return m_filesToWatch;
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

QString Vs2010ProjectData::getDefaultOutputDirectory(const QString& platform)
{
    QString result = _SolutionDir;
    if (Win32 != platform) {
        result += _Platform;
        result += QLatin1Char('/');
    }

    result += _Configuration;
    result += QLatin1Char('/');
    return result;
}

QString Vs2010ProjectData::getDefaultIntDirectory(const QString& platform)
{
    QString result;
    if (Win32 != platform) {
        result += _Platform;
        result += QLatin1Char('/');
    }

    result += _Configuration;
    result += QLatin1Char('/');
    return result;
}

} // namespace Internal
} // namespace VsProjectManager
