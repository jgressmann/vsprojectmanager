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

#include <projectexplorer/projectnodes.h>


#include <QFileInfo>
#include <QDir>
#include <QList>
#include <QStringList>
#include <QDomDocument>
#include <QHash>
#include <QProcess>

#include <utils/fileutils.h>


namespace VsProjectManager {
namespace Internal {

enum TargetType {
    ExecutableType = 0,
    StaticLibraryType = 2,
    DynamicLibraryType = 3,
    UtilityType = 64,
    Other
};

class VsBuildTarget
{
public:
    VsBuildTarget() = default;
public:
    QString configuration;
    QString title;
    QString output;
    QString outdir;
    TargetType targetType;

    // code model
    QStringList includeDirectories;
    QStringList compilerOptions;
    QByteArray defines;
};

typedef QList<VsBuildTarget> VsBuildTargets;

class VsProjectData : public QObject
{
    Q_OBJECT
public:
    typedef QHash<QString, QString> VariableSubstitution;

public:
    virtual ~VsProjectData();
    static VsProjectData* load(const Utils::FileName& projectFile);

public:
    virtual VsBuildTargets targets() const = 0;
    virtual QStringList configurations() const = 0;
    virtual QStringList files() const = 0;
    virtual void buildCmd(const QString& configuration, QString* cmd, QString* args) const = 0;
    virtual void cleanCmd(const QString& configuration, QString* cmd, QString* args) const = 0;
    void openInDevenv();

protected:
    VsProjectData(const Utils::FileName& projectFile, const QDomDocument& doc);

protected:
    static void splitConfiguration(const QString& configuration, QString* configurationName, QString* platformName);
    QString makeAbsoluteFilePath(const QString& path) const;
    static QString substitute(QString input, const VariableSubstitution& sub);
    const QDomDocument& doc() const { return m_doc; }
    const Utils::FileName& projectFilePath() const { return m_projectFilePath; }
    const QDir& projectDirectory() const { return m_projectDirectory; }
    void addDefaultIncludeDirectories(QStringList& includes) const;
    void setInstallDir(const QDir& dir) { m_installDirectory = dir; }
    const QDir& installDir() const { return m_installDirectory; }

private:
    void devenvProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void devenvProcessErrorOccurred(QProcess::ProcessError error);
    void releaseDevenvProcess();

private:
    QDomDocument m_doc;
    Utils::FileName m_projectFilePath;
    QDir m_projectDirectory;
    QDir m_installDirectory;

    QProcess* m_devenvProcess = nullptr;
};

class Vs2005ProjectData : public VsProjectData
{
public:
    Vs2005ProjectData(const Utils::FileName& projectFile, const QDomDocument& doc);
public:
    virtual VsBuildTargets targets() const override;
    virtual QStringList configurations() const override;
    virtual QStringList files() const override;
    virtual void buildCmd(const QString& configuration, QString* cmd, QString* args) const override;
    virtual void cleanCmd(const QString& configuration, QString* cmd, QString* args) const override;

private:
    void makeCmd(const QString& configuration, const QString& buildSwitch, QString& cmd, QString& args) const;

    void parseFilter(
        const QDomNodeList& xmlItems,
        const QString& configuration,
        QStringList& files) const;
    static QString getDefaultOutputDirectory(const QString& platform);
    static QString getDefaultIntDirectory(const QString& platform);

private:
    VsBuildTargets m_targets;
    QStringList m_configurations;
    QStringList m_files;
    QString m_devenvPath;
    QString m_vcvarsPath;
    QString m_solutionDir;
};


class Vs2010ProjectData : public VsProjectData
{
public:
    Vs2010ProjectData(const Utils::FileName& projectFile, const QDomDocument& doc, const char* toolsEnvVarName);

public:
    virtual VsBuildTargets targets() const override;
    virtual QStringList configurations() const override;
    virtual QStringList files() const override;
    virtual void buildCmd(const QString& configuration, QString* cmd, QString* args) const override;
    virtual void cleanCmd(const QString& configuration, QString* cmd, QString* args) const override;

private:
    void makeCmd(const QString& configuration, const QString& buildSwitch, QString& cmd, QString& args) const;
    static QString getDefaultOutputDirectory(const QString& platform);
    static QString getDefaultIntDirectory(const QString& platform);

private:
    VsBuildTargets m_targets;
    QStringList m_configurations;
    QStringList m_files;
    QString m_vcvarsPath;
    QString m_solutionDir;
};


} // namespace Constants
} // namespace VsProjectManager
