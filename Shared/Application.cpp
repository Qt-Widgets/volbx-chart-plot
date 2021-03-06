#include "Application.h"

#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include <QWidget>

#include "CommonProperties.h"

const char* Application::cssSuffix_ = ".css";

void Application::setAdditionalApplicatioInfo(const char* productName)
{
    QApplication::setApplicationName(QLatin1String(productName));
    QApplication::setApplicationVersion(QStringLiteral(VER_PRODUCTVERSION_STR));
    QApplication::setOrganizationName(QStringLiteral(VER_COMPANYNAME_STR));
    QApplication::setOrganizationDomain(QLatin1String(VER_COMPANYDOMAIN_STR));
}

void Application::initStyle(const QString& nameFromConfig)
{
    QStringList qtStyles = QStyleFactory::keys();
    if (qtStyles.contains(nameFromConfig))
    {
        setQtStyle(nameFromConfig);
    }
    else
    {
        setCssStyle(nameFromConfig);
    }
}

QString Application::getStylePath(const QString& styleName)
{
    if (QFile::exists(getResStylePath(styleName)))
    {
        return getResStylePath(styleName);
    }

    return getLocalStyleFilePath(styleName);
}

QString Application::getResStylePath(const QString& styleName)
{
    const static QLatin1String resPrefix(":/Css/");
    return resPrefix + styleName + QLatin1String(cssSuffix_);
}

QString Application::getLocalStyleFilePath(const QString& styleName)
{
    QString stylePath;
    stylePath.append(QApplication::applicationDirPath());
    stylePath.append(QLatin1String("/"));
    stylePath.append(styleName);
    stylePath.append(QLatin1String(cssSuffix_));
    return stylePath;
}

void Application::setCssStyle(const QString& styleName)
{
    clearAppFocus();

    QFile styleFile(getStylePath(styleName));

    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString style(QLatin1String(styleFile.readAll()));

        qApp->setStyleSheet(style);
    }
}

void Application::setQtStyle(const QString& name)
{
    clearAppFocus();
    qApp->setStyleSheet(QString());
    qApp->setStyle(name);
}

void Application::clearAppFocus()
{
    QWidget* focusWidget = qApp->focusWidget();
    if (nullptr != focusWidget)
    {
        focusWidget->clearFocus();
    }
}
