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

#include <QDir>
#include <QFile>
#include <QFileInfo>


#include <algorithm>

#ifndef _countof
#   define _countof(x) ((sizeof(x)/sizeof(x[0])))
#endif

namespace VsProjectManager {
namespace Internal {

//#define VSDEBUG

#ifdef VSDEBUG
#include <stdio.h>
static void Indent(FILE* f, int level)
{
    for (int i = 0; i < level; ++i)
        fprintf(f, "  ");
}

static void DumpXml(FILE* f, const QDomNode& node, int level)
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

//VsBuildTarget::VsBuildTarget()
//{
//    clear();
//}

//void VsBuildTarget::clear()
//{
//    configuration.clear();
//    title.clear();
//    output.clear();
//    outdir.clear();
//    targetType = TargetType::ExecutableType;

//    // code model
//    includeDirectories.clear();
//    compilerOptions.clear();
//    defines.clear();
//}

//bool VsBuildTarget::fromStringList(const QStringList& str, VsBuildTarget* result)
//{
//    VsBuildTarget r;
//    int index = 0;
//    if (index >= str.size()) return false;
//    r.configuration = str[index++];
//    if (index >= str.size()) return false;
//    r.title = str[index++];
//    if (index >= str.size()) return false;
//    r.output = str[index++];
//    if (index >= str.size()) return false;
//    r.outdir = str[index++];
//    if (index >= str.size()) return false;
//    r.targetType = (TargetType)(str[index++].toInt());
//    if (index >= str.size()) return false;
//    r.includeDirectories = str[index++].split(QLatin1Char(';'), QString::SkipEmptyParts);
//    if (index >= str.size()) return false;
//    r.compilerOptions = str[index++].split(QLatin1Char(';'), QString::SkipEmptyParts);
//    if (index >= str.size()) return false;
//    r.defines = str[index++].toLocal8Bit();

//    if (result)
//        *result = r;

//    return true;
//}

//QStringList VsBuildTarget::toStringList() const
//{
//    QStringList result;
//    result << configuration;
//    result << title;
//    result << output;
//    result << outdir;
//    result << QString::number(targetType);
//    result << includeDirectories.join(QLatin1Char(';'));
//    result << compilerOptions.join(QLatin1Char(';'));
//    result << QString::fromLocal8Bit(defines);
//    return result;
//}

////////////////////////////////////////////////////////////////////////////////
VsProjectData::~VsProjectData()
{
    // So that the compiler knows where to put the vtable
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
        if (version == QLatin1String("4.0")) {
            return new Vs2010ProjectData(projectFilePath, doc);
        } else {
            qWarning("Don't know how to parse tools version %s project files", qPrintable(version));
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
        platform = configuration.right(pipeIndex);
        confName = configuration.left(pipeIndex);
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

QString VsProjectData::substitute(QString input, const QHash<QString, QString>& sub)
{
    QHash<QString, QString>::const_iterator it = sub.begin();
    QHash<QString, QString>::const_iterator end = sub.end();
    for (; it != end; ++it) {
        input = input.replace(it.key(), it.value());
    }
    return input;
}


////////////////////////////////////////////////////////////////////////////////
Vs2005ProjectData::Vs2005ProjectData(const Utils::FileName& projectFile, const QDomDocument& doc)
    : VsProjectData(projectFile, doc)
{
    auto vs2005ToolsPath = qgetenv("VS80COMNTOOLS");
    m_Vs2005InstallDir = QDir(QString::fromLocal8Bit(vs2005ToolsPath));
    m_Vs2005InstallDir.cdUp();
    m_Vs2005InstallDir.cdUp();

    m_devenvPath  = QDir::toNativeSeparators(m_Vs2005InstallDir.absoluteFilePath(QLatin1String("Common7/IDE/devenv.exe")));
    m_vcvarsPath  = QDir::toNativeSeparators(m_Vs2005InstallDir.absoluteFilePath(QLatin1String("VC/vcvarsall.bat")));

    m_solutionDir = projectDirectory().path();

    auto configurationNodes = doc.documentElement().namedItem(QLatin1String("Configurations")).childNodes();
    m_targets.reserve(configurationNodes.size());

    for (auto i = 0; i < configurationNodes.count(); ++i) {
        const auto& configNode = configurationNodes.at(i);
        auto key = configNode.attributes().namedItem(QLatin1String("Name")).nodeValue();

        m_configurations << key;
        QString platform, configuration;
        splitConfiguration(key, &configuration, &platform);


        auto configurationType = configNode.attributes().namedItem(QLatin1String("ConfigurationType")).nodeValue().toInt();


        VsBuildTarget target;
        switch (configurationType) {
        case 1:
            target.targetType = TargetType::ExecutableType;
            break;
        case 2:
            target.targetType = TargetType::DynamicLibraryType;
            break;
        case 3:
            target.targetType = TargetType::StaticLibraryType;
            break;
        case 4:
            target.targetType = TargetType::UtilityType;
            break;
        default:
            target.targetType = TargetType::Other;
            break;
        }

        target.outdir = substituteVariables(
                    configNode.attributes().namedItem(QLatin1String("OutputDirectory")).nodeValue(),
                    configuration,
                    platform);
        target.configuration = key;
        target.title = projectFile.toFileInfo().baseName();

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
                    target.includeDirectories.append(m_Vs2005InstallDir.absolutePath() + QLatin1String("/VC/include"));
                    target.includeDirectories.append(m_Vs2005InstallDir.absolutePath() + QLatin1String("/VC/atlmfc/include"));
                    target.includeDirectories.append(m_Vs2005InstallDir.absolutePath() + QLatin1String("/VC/PlatformSDK/include"));

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
                            outfile = substituteVariables(outfile, configuration, platform);
                            target.output = makeAbsoluteFilePath(outfile);
                        }
                        break;
                    }
                } else if (toolName == QLatin1String("VCLibrarianTool")) {
                    if (TargetType::StaticLibraryType == target.targetType) {
                        QString outfile = toolNode.attributes().namedItem(QLatin1String("OutputFile")).nodeValue();
                        outfile = substituteVariables(outfile, configuration, platform);
                        target.output = makeAbsoluteFilePath(outfile);
                    }
                }
            }
        }



        // parse <Files> section in the context of this configuration
        auto filesChildNodes = doc.documentElement().namedItem(QLatin1String("Files")).childNodes();
        parseFilter(filesChildNodes, key, m_files);

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

//QString Vs2005ProjectData::devenvPath() const
//{
//    return m_devenvPath;
//}
//QString Vs2005ProjectData::vcvarsPath() const
//{
//    return m_vcvarsPath;
//}

//QStringList Vs2005ProjectData::buildArgs(const QString& configuration) const
//{
//    QStringList result;
//    result << QDir::toNativeSeparators(projectFilePath().toFileInfo().absoluteFilePath()) << QLatin1String("/Build") << configuration;
//    return result;
//}

//QStringList Vs2005ProjectData::cleanArgs(const QString& configuration) const
//{
//    QStringList result;
//    result << QDir::toNativeSeparators(projectFilePath().toFileInfo().absoluteFilePath()) << QLatin1String("/Clean") << configuration;
//    return result;
//}

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

QString Vs2005ProjectData::substituteVariables(const QString& input, const QString& configuration, const QString& platform) const
{
    QString result = input;
    result.replace(QLatin1String("$(ProjectDir)"), projectDirectory().path() + QLatin1String("/"));
    result.replace(QLatin1String("$(SolutionDir)"), m_solutionDir  + QLatin1String("/"));
    result.replace(QLatin1String("$(ConfigurationName)"), configuration);
    result.replace(QLatin1String("$(PlatformName)"), platform);
    return result;
}


////////////////////////////////////////////////////////////////////////////////
Vs2010ProjectData::Vs2010ProjectData(const Utils::FileName& projectFile, const QDomDocument& doc)
    : VsProjectData(projectFile, doc)
{
    auto vs2010ToolsPath = qgetenv("VS100COMNTOOLS");
    m_Vs2010InstallDir = QDir(QString::fromLocal8Bit(vs2010ToolsPath));
    m_Vs2010InstallDir.cdUp();
    m_Vs2010InstallDir.cdUp();

    m_vcvarsPath  = QDir::toNativeSeparators(m_Vs2010InstallDir.absoluteFilePath(QLatin1String("VC/vcvarsall.bat")));
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
                            if (name == QLatin1String("ClCompile") ||
                                name == QLatin1String("None") ||
                                name == QLatin1String("Midl") ||
                                name == QLatin1String("ResourceCompile")) {
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
        sub.insert(QLatin1String("$(TargetName)"), projectFile.toFileInfo().baseName());

        VsBuildTarget target;
        target.configuration = configuration;
        target.title = projectFile.toFileInfo().baseName();
        target.outdir = QLatin1String("$(SolutionDir)$(Configuration)/");
        target.output = QLatin1String("$(OutDir)$(TargetName)$(TargetExt)");

        auto condition = QStringLiteral("'$(Configuration)|$(Platform)'=='%1'").arg(configuration);
        QString platformName, configurationName;
        splitConfiguration(configuration, &configurationName, &platformName);
        sub.insert(QLatin1String("$(Configuration)"), configurationName);
        sub.insert(QLatin1String("$(ConfigurationName)"), configurationName);
        sub.insert(QLatin1String("$(Platform)"), platformName);
        sub.insert(QLatin1String("$(PlatformName)"), platformName);

        for (auto i = 0; i < childNodes.count(); ++i) {
            auto childNode = childNodes.at(i);
            if (childNode.nodeType() == QDomNode::ElementNode) {
                QDomElement element = childNode.toElement();
                QString elementName = element.nodeName();
                if (elementName == QLatin1String("PropertyGroup")) {
                    if (element.hasAttributes()) {
                        if (element.attribute(QLatin1String("Label")) == QLatin1String("Configuration") &&
                            element.attribute(QLatin1String("Condition")) == condition) {
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
                        }
                    } else {
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

                        QDomElement linkElement = element.namedItem(QLatin1String("Link")).toElement();
                        auto outputFileNode = linkElement.namedItem(QLatin1String("OutputFile"));
                        if (outputFileNode.isElement()) {
                            target.output = QDir::fromNativeSeparators(outputFileNode.childNodes().at(0).nodeValue());
                        }
                    }
                }
            }
        }

        target.output = substitute(target.output, sub);
        target.output = makeAbsoluteFilePath(target.output);
        target.outdir = substitute(target.outdir, sub);
        target.outdir = makeAbsoluteFilePath(target.output);
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
    args = QString::fromLatin1("/c \"set \"VISUALSTUDIOVERSION=\" & call \"%1\" & %2 \"%3\" /nologo %4 /p:Configuration=\"%5\" /p:Platform=\"%6\"\"").arg(
                m_vcvarsPath,
                QLatin1String("msbuild"),
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
