/*
    Copyright (c) 2015, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/log.h"
#include "common/settings.h"

#include "platform/dummy/dummyclipboard.h"
#include "winplatform.h"
#include "winplatformclipboard.h"
#include "winplatformwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <qt_windows.h>

namespace {

void migrateDirectory(const QString oldPath, const QString newPath)
{
    QDir oldDir(oldPath);
    QDir newDir(newPath);

    if ( oldDir.exists() ) {
        newDir.mkpath(".");

        foreach ( const QString &fileName, oldDir.entryList(QDir::Files) ) {
            const QString oldFileName = oldDir.absoluteFilePath(fileName);
            const QString newFileName = newDir.absoluteFilePath(fileName);
            COPYQ_LOG( QString("Migrating \"%1\" -> \"%2\"")
                       .arg(oldFileName)
                       .arg(newFileName) );
            QFile::copy(oldFileName, newFileName);
        }
    }
}

void migrateConfigToAppDir()
{
    const QString path = QCoreApplication::applicationDirPath() + "/config";
    QDir dir(path);

    if ( dir.mkpath("copyq") && dir.isReadable() && QFileInfo(path).isWritable() ) {
        QSettings oldSettings;
        const QString oldConfigFileName =
                QSettings(QSettings::IniFormat, QSettings::UserScope,
                          QCoreApplication::organizationName(),
                          QCoreApplication::applicationName()).fileName();
        const QString oldConfigPath = QDir::cleanPath(oldConfigFileName + "/..");

        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, path);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        Settings newSettings;

        if ( Settings::isEmpty(newSettings) ) {
            COPYQ_LOG("Migrating configuration to application directory.");
            const QString newConfigPath = QDir::cleanPath(newSettings.fileName() + "/..");

            // Migrate configuration from system directory.
            migrateDirectory(oldConfigPath, newConfigPath);

            // Migrate themes from system directory.
            migrateDirectory(oldConfigPath + "/themes", newConfigPath + "/themes");

            // Migrate rest of the configuration from the system registry.
            foreach ( const QString &key, oldSettings.allKeys() )
                newSettings.setValue(key, oldSettings.value(key));
        }
    } else {
        COPYQ_LOG( QString("Cannot use \"%1\" directory to save user configuration and items.")
                   .arg(path) );
    }
}

} // namespace

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new WinPlatform);
}

PlatformWindowPtr WinPlatform::getWindow(WId winId)
{
    HWND window = reinterpret_cast<HWND>(winId);
    return PlatformWindowPtr( window ? new WinPlatformWindow(window) : NULL );
}

PlatformWindowPtr WinPlatform::getCurrentWindow()
{
    HWND window = GetForegroundWindow();
    return PlatformWindowPtr( window ? new WinPlatformWindow(window) : NULL );
}

QApplication *WinPlatform::createServerApplication(int &argc, char **argv)
{
    return new QApplication(argc, argv);
}

QApplication *WinPlatform::createMonitorApplication(int &argc, char **argv)
{
    return new QApplication(argc, argv);
}

QCoreApplication *WinPlatform::createClientApplication(int &argc, char **argv)
{
    return new QCoreApplication(argc, argv);
}

void WinPlatform::loadSettings()
{
    migrateConfigToAppDir();
}

PlatformClipboardPtr WinPlatform::clipboard()
{
    return PlatformClipboardPtr(new WinPlatformClipboard());
}

int WinPlatform::keyCode(const QKeyEvent &event)
{
    const quint32 vk = event.nativeVirtualKey();

    if (vk >= '0' && vk <= '9')
        return Qt::Key_0 + vk - '0';

    if (vk >= 'A' && vk <= 'Z')
        return Qt::Key_A + vk - 'A';

    return PlatformNativeInterface::keyCode(event);
}

QStringList WinPlatform::getCommandLineArguments(int, char**)
{
    int argumentCount;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);

    QStringList result;

    for (int i = 1; i < argumentCount; ++i)
        result.append( QString::fromUtf16(reinterpret_cast<ushort*>(arguments[i])) );

    return result;
}
