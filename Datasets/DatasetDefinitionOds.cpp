#include "DatasetDefinitionOds.h"

#include <memory>

#include <QApplication>
#include <QDebug>
#include <QDomDocument>
#include <QDomElement>
#include <QXmlStreamReader>
#include <quazip5/quazipfile.h>

#include "Common/Constants.h"
#include "Common/ProgressBar.h"
#include "Shared/Logger.h"

DatasetDefinitionOds::DatasetDefinitionOds(const QString& name,
                                           const QString& zipFileName)
    : DatasetDefinitionSpreadsheet(name, zipFileName)
{

}

bool DatasetDefinitionOds::getSheetList(QuaZip& zip)
{
    if (zip.setCurrentFile(QStringLiteral("settings.xml")))
    {
        //Open file in zip archive.
        QuaZipFile zipFile(&zip);
        if (!zipFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            LOG(LogTypes::IMPORT_EXPORT,
                "Can not open file " + zipFile.getFileName() + ".");
            return false;
        }

        //Create, set content and read DOM.
        QDomDocument xmlDocument(name_);
        if (!xmlDocument.setContent(zipFile.readAll()))
        {
            LOG(LogTypes::IMPORT_EXPORT, "Xml file is damaged.");
            return false;
        }
        zipFile.close();

        QDomElement root = xmlDocument.documentElement();

        const QString configMapNamed(QStringLiteral("config:config-item-map-named"));
        const QString configName(QStringLiteral("config:name"));
        const QString configMapEntry(QStringLiteral("config:config-item-map-entry"));
        const QString tables(QStringLiteral("Tables"));

        int elementsCount = root.elementsByTagName(configMapNamed).size();
        for (int i = 0 ; i < elementsCount; i++)
        {
            QDomElement currentElement =
                root.elementsByTagName(configMapNamed)
                .at(i).toElement();
            if (currentElement.hasAttribute(configName) &&
                currentElement.attribute(configName) == tables)
            {
                int innerElementsCount =
                    currentElement.elementsByTagName(configMapEntry).size();
                for (int j = 0; j < innerElementsCount; j++)
                {
                    QDomElement element =
                        currentElement.elementsByTagName(configMapEntry).at(j).toElement();
                    sheetNames_.push_back(element.attribute(configName));
                }
            }
        }
    }
    else
    {
        LOG(LogTypes::IMPORT_EXPORT, "Can not open file xl/workbook.xml in archive.");
        return false;
    }

    return true;
}

bool DatasetDefinitionOds::getColumnList(QuaZip& zip,
                                         const QString& sheetName)
{
    if (zip.setCurrentFile(QStringLiteral("content.xml")))
    {
        //Open file in zip archive.
        QuaZipFile zipFile(&zip);
        if (!zipFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            LOG(LogTypes::IMPORT_EXPORT,
                "Can not open file " + zipFile.getFileName() + ".");
            return false;
        }

        QXmlStreamReader xmlStreamReader;
        xmlStreamReader.setDevice(&zipFile);

        //Move to first row in selected sheet.
        while (!xmlStreamReader.atEnd() &&
               xmlStreamReader.name() != "table:table" &&
               xmlStreamReader.attributes().value(QLatin1String("table:name")) != sheetName)
        {
            xmlStreamReader.readNext();
        }

        while (!xmlStreamReader.atEnd() &&
               xmlStreamReader.name() != "table-row")
        {
            xmlStreamReader.readNext();
        }

        xmlStreamReader.readNext();

        //Actual column number.
        int j = Constants::NOT_SET_COLUMN;
        QXmlStreamReader::TokenType lastToken = xmlStreamReader.tokenType();

        const QString numberColumnsRepeated(QStringLiteral("table:number-columns-repeated"));

        //Parse first row.
        while (!xmlStreamReader.atEnd() &&
               xmlStreamReader.name() != "table-row")
        {
            //When we encounter first cell of worksheet.
            if (xmlStreamReader.name().toString() == QLatin1String("table-cell") &&
                xmlStreamReader.tokenType() == QXmlStreamReader::StartElement)
            {
                QString emptyColNumber =
                    xmlStreamReader.attributes().value(numberColumnsRepeated).toString();
                if (!emptyColNumber.isEmpty())
                {
                    break;
                }

                //Add column number.
                j++;
            }

            //If we encounter start of cell content we add it to list.
            if (!xmlStreamReader.atEnd() &&
                xmlStreamReader.name().toString() == QStringLiteral("p") &&
                xmlStreamReader.tokenType() == QXmlStreamReader::StartElement)
            {
                while (xmlStreamReader.tokenType() != QXmlStreamReader::Characters)
                {
                    xmlStreamReader.readNext();
                }

                headerColumnNames_.push_back(xmlStreamReader.text().toString());
            }

            //If we encounter empty cell we add it to list.
            if (xmlStreamReader.name().toString() == QLatin1String("table-cell") &&
                xmlStreamReader.tokenType() == QXmlStreamReader::EndElement &&
                lastToken == QXmlStreamReader::StartElement)
            {
                headerColumnNames_ << emptyColName_;
            }

            lastToken = xmlStreamReader.tokenType();
            xmlStreamReader.readNext();
        }
    }
    else
    {
        LOG(LogTypes::IMPORT_EXPORT, "Can not open file " + sheetName + " in archive.");
        return false;
    }

    return true;
}

bool DatasetDefinitionOds::openZipAndMoveToSecondRow(QuaZip& zip,
                                                     const QString& sheetName,
                                                     QuaZipFile& zipFile,
                                                     QXmlStreamReader& xmlStreamReader)
{
    //Open file in zip archive.
    if (!zip.setCurrentFile(QStringLiteral("content.xml")))
    {
        LOG(LogTypes::IMPORT_EXPORT, "Can not open file " + sheetName + " in archive.");
        return false;
    }
    zipFile.setZip(&zip);
    if (!zipFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        LOG(LogTypes::IMPORT_EXPORT, "Can not open file " + zipFile.getFileName() + ".");
        return false;
    }

    xmlStreamReader.setDevice(&zipFile);

    //Move to first row in selected sheet.
    while (!xmlStreamReader.atEnd() &&
           xmlStreamReader.name() != "table:table" &&
           xmlStreamReader.attributes().value(QLatin1String("table:name")) != sheetName)
    {
        xmlStreamReader.readNext();
    }

    bool secondRow = false;
    while (!xmlStreamReader.atEnd())
    {
        if (xmlStreamReader.name() == "table-row" &&
            xmlStreamReader.tokenType() == QXmlStreamReader::StartElement)
        {
            if (secondRow)
            {
                break;
            }
            secondRow = true;
        }
        xmlStreamReader.readNext();
    }

    return true;
}

bool DatasetDefinitionOds::getColumnTypes(QuaZip& zip,
                                          const QString& sheetName)
{
    ProgressBar bar(ProgressBar::PROGRESS_TITLE_DETECTING_COLUMN_TYPES, 0, nullptr);
    QTime performanceTimer;
    performanceTimer.start();

    QApplication::processEvents();

    QuaZipFile zipFile;
    QXmlStreamReader xmlStreamReader;

    if (!openZipAndMoveToSecondRow(zip, sheetName, zipFile, xmlStreamReader))
    {
        return false;
    }

    columnsFormat_.clear();

    for (int i = 0; i < columnsCount_; ++i)
    {
        columnsFormat_.push_back(DATA_FORMAT_UNKNOWN);
    }

    //Actual column number.
    int column = Constants::NOT_SET_COLUMN;

    //Actual row number.
    int rowCounter = 0;

    //Actual data type in current cell (s, str, null).
    QString currentColType(QStringLiteral("string"));

    int repeatCount = 1;

    const QString tableTag(QStringLiteral("table"));
    const QString tableRowTag(QStringLiteral("table-row"));
    const QString tableCellTag(QStringLiteral("table-cell"));
    const QString officeValueTypeTag(QStringLiteral("office:value-type"));
    const QString columnsRepeatedTag(QStringLiteral("table:number-columns-repeated"));
    const QString stringTag(QStringLiteral("string"));
    const QString dateTag(QStringLiteral("date"));
    const QString floatTag(QStringLiteral("float"));
    const QString percentageTag(QStringLiteral("percentage"));
    const QString currencyTag(QStringLiteral("currency"));
    const QString timeTag(QStringLiteral("time"));

    bool rowEmpty = true;

    while (!xmlStreamReader.atEnd() &&
           xmlStreamReader.name().compare(tableTag) != 0)
    {
        //If start of row encountered than reset column counter add increment row counter.
        if (0 == xmlStreamReader.name().compare(tableRowTag) &&
            xmlStreamReader.isStartElement())
        {
            column = Constants::NOT_SET_COLUMN;

            if (!rowEmpty)
            {
                rowCounter++;

                const int batchSize {100};
                if (0 == rowCounter % batchSize)
                {
                    QApplication::processEvents();
                }

                rowEmpty = true;
            }
        }

        //When we encounter start of cell description.
        if (0 == xmlStreamReader.name().compare(tableCellTag) &&
            xmlStreamReader.tokenType() == QXmlStreamReader::StartElement)
        {
            column++;

            //If we encounter column outside expected grid we move to row end.
            if (column >= columnsCount_)
            {
                while (!xmlStreamReader.atEnd() &&
                       !(0 == xmlStreamReader.name().compare(tableRowTag) &&
                         xmlStreamReader.isEndElement()))
                {
                    xmlStreamReader.readNext();
                }
                continue;
            }

            //Remember column type.
            currentColType =
                xmlStreamReader.attributes().value(officeValueTypeTag).toString();

            //Number of reapeats.
            repeatCount =
                xmlStreamReader.attributes().value(columnsRepeatedTag).toString().toInt();

            if (0 == repeatCount)
            {
                repeatCount = 1;
            }

            if (column + repeatCount - 1 >= columnsCount_)
            {
                repeatCount = columnsCount_ - column;
            }

            for (int i = 0; i < repeatCount; ++i)
            {
                if (0 == currentColType.compare(stringTag))
                {
                    rowEmpty = false;
                    if (columnsFormat_.at(column + i) == DATA_FORMAT_UNKNOWN)
                    {
                        columnsFormat_[column + i] = DATA_FORMAT_STRING;
                    }
                    else
                    {
                        if (columnsFormat_.at(column + i) != DATA_FORMAT_STRING)
                        {
                            columnsFormat_[column + i] = DATA_FORMAT_STRING;
                        }
                    }
                }
                else
                {
                    if (0 == currentColType.compare(dateTag))
                    {
                        rowEmpty = false;
                        if (columnsFormat_.at(column + i) == DATA_FORMAT_UNKNOWN)
                        {
                            columnsFormat_[column + i] = DATA_FORMAT_DATE;
                        }
                        else
                        {
                            if (columnsFormat_.at(column + i) != DATA_FORMAT_DATE)
                            {
                                columnsFormat_[column + i] = DATA_FORMAT_STRING;
                            }
                        }
                    }
                    else
                    {
                        if (0 == currentColType.compare(floatTag) ||
                            0 == currentColType.compare(percentageTag) ||
                            0 == currentColType.compare(currencyTag) ||
                            0 == currentColType.compare(timeTag))
                        {
                            rowEmpty = false;
                            if (columnsFormat_.at(column + i) == DATA_FORMAT_UNKNOWN)
                            {
                                columnsFormat_[column + i] = DATA_FORMAT_FLOAT;
                            }
                            else
                            {
                                if (columnsFormat_.at(column + i) != DATA_FORMAT_FLOAT)
                                {
                                    columnsFormat_[column + i] = DATA_FORMAT_STRING;
                                }
                            }
                        }
                    }
                }
            }
            column += repeatCount - 1;
        }
        xmlStreamReader.readNextStartElement();
    }

    for (int i = 0; i < columnsCount_; ++i)
    {
        if (DATA_FORMAT_UNKNOWN == columnsFormat_.at(i))
        {
            columnsFormat_[i] = DATA_FORMAT_STRING;
        }
    }

    rowsCount_ = rowCounter;

    LOG(LogTypes::IMPORT_EXPORT, "Analyzed file having " + QString::number(rowsCount_) +
        " rows in time " +
        QString::number(performanceTimer.elapsed() * 1.0 / 1000) +
        " seconds.");

    return true;
}

bool DatasetDefinitionOds::getDataFromZip(QuaZip& zip,
                                          const QString& sheetName,
                                          QVector<QVector<QVariant> >* dataContainer,
                                          bool fillSamplesOnly)
{
    std::unique_ptr<ProgressBar> bar {fillSamplesOnly ? nullptr : std::make_unique<ProgressBar>(ProgressBar::PROGRESS_TITLE_LOADING, rowsCount_, nullptr)};

    QTime performanceTimer;
    performanceTimer.start();

    QuaZipFile zipFile;
    QXmlStreamReader xmlStreamReader;

    if (!openZipAndMoveToSecondRow(zip, sheetName, zipFile, xmlStreamReader))
    {
        return false;
    }

    //Null elements row.
    QVector<QVariant> templateDataRow;

    QMap<int, int> activeColumnsMapping;

    int columnToFill = 0;

    templateDataRow.resize((fillSamplesOnly ? columnsCount_ : getActiveColumnCount()));
    for (int i = 0; i < columnsCount_; ++i)
    {
        if (fillSamplesOnly || activeColumns_.at(i))
        {
            templateDataRow[columnToFill] =
                getDefaultVariantForFormat(columnsFormat_[i]);

            activeColumnsMapping[i] = columnToFill;
            columnToFill++;
        }
    }

    //Protection from potential core related to empty rows.
    int containerSize = dataContainer->size();
    for (int i = 0; i < containerSize; ++i)
    {
        (*dataContainer)[i] = templateDataRow;
    }

    //Current row data.
    QVector<QVariant> currentDataRow(templateDataRow);

    //Actual column number.
    int column = Constants::NOT_SET_COLUMN;

    //Actual data type in cell (s, str, null).
    QString currentColType = QStringLiteral("string");

    //Current row number.
    int rowCounter = 0;

    int repeatCount = 1;

    int cellsFilledInRow = 0;

    //Optimization.
    const QString tableTag(QStringLiteral("table"));
    const QString tableRowTag(QStringLiteral("table-row"));
    const QString tableCellTag(QStringLiteral("table-cell"));
    const QString officeValueTypeTag(QStringLiteral("office:value-type"));
    const QString columnsRepeatedTag(QStringLiteral("table:number-columns-repeated"));
    const QString pTag(QStringLiteral("p"));
    const QString officeDateValueTag(QStringLiteral("office:date-value"));
    const QString officeValueTag(QStringLiteral("office:value"));
    const QString dateFormat(QStringLiteral("yyyy-MM-dd"));

    const QString emptyString(QLatin1String(""));

    while (!xmlStreamReader.atEnd() &&
           0 != xmlStreamReader.name().compare(tableTag) &&
           rowCounter < rowsCount_)
    {
        //If start of row encountered than reset column counter add
        //increment row counter.
        if (0 == xmlStreamReader.name().compare(tableRowTag) &&
            xmlStreamReader.isStartElement())
        {
            column = Constants::NOT_SET_COLUMN;

            if (0 != cellsFilledInRow)
            {
                (*dataContainer)[rowCounter] = currentDataRow;
                currentDataRow = QVector<QVariant>(templateDataRow);
                cellsFilledInRow = 0;
                rowCounter++;

                if (!fillSamplesOnly)
                {
                    bar->updateProgress(rowCounter);
                }

                if (fillSamplesOnly && rowCounter >= SAMPLE_SIZE)
                {
                    break;
                }
            }
        }

        //When we encounter start of cell description.
        if (0 == xmlStreamReader.name().compare(tableCellTag) &&
            xmlStreamReader.isStartElement())
        {
            column++;

            //If we encounter column outside expected grid we move to row end.
            if (column >= columnsCount_)
            {
                while (!xmlStreamReader.atEnd() &&
                       !(0 == xmlStreamReader.name().compare(tableRowTag) &&
                         xmlStreamReader.isEndElement()))
                {
                    xmlStreamReader.readNext();
                }
                continue;
            }

            //Remember cell type.
            currentColType =
                xmlStreamReader.attributes().value(officeValueTypeTag).toString();

            //Number of repeats.
            repeatCount =
                xmlStreamReader.attributes().value(columnsRepeatedTag).toString().toInt();

            if (0 == repeatCount)
            {
                repeatCount = 1;
            }

            if (column + repeatCount - 1 >= columnsCount_)
            {
                repeatCount = columnsCount_ - column;
            }

            if (!currentColType.isEmpty())
            {
                QVariant value;
                DataFormat format = columnsFormat_.at(column);

                switch (format)
                {
                    case DATA_FORMAT_STRING:
                    {
                        while (!xmlStreamReader.atEnd() &&
                               0 != xmlStreamReader.name().compare(pTag))

                        {
                            xmlStreamReader.readNext();
                        }

                        while (xmlStreamReader.tokenType() != QXmlStreamReader::Characters &&
                               0 != xmlStreamReader.name().compare(tableCellTag))
                        {
                            xmlStreamReader.readNext();
                        }

                        const QString* stringPointer = xmlStreamReader.text().string();
                        value =
                            QVariant(getStringIndex((stringPointer == nullptr ? emptyString : *stringPointer)));

                        break;
                    }

                    case DATA_FORMAT_DATE:
                    {
                        static const int odsStringDateLength {10};
                        value =
                            QVariant(QDate::fromString(xmlStreamReader.attributes().value(officeDateValueTag).toString().left(odsStringDateLength), dateFormat));

                        break;
                    }

                    case DATA_FORMAT_FLOAT:
                    {
                        value =
                            QVariant(xmlStreamReader.attributes().value(officeValueTag).toDouble());
                        break;
                    }

                    case DATA_FORMAT_UNKNOWN:
                    {
                        Q_ASSERT(false);
                        break;
                    }
                }

                for (int i = 0; i < repeatCount; ++i)
                {
                    if (!fillSamplesOnly && !activeColumns_[column + i])
                    {
                        continue;
                    }

                    cellsFilledInRow++;
                    currentDataRow[activeColumnsMapping[column + i]] = value;
                }
            }

            column += repeatCount - 1;
        }
        xmlStreamReader.readNextStartElement();
    }

    if (!fillSamplesOnly)
    {
        Q_ASSERT(rowsCount_ == dataContainer->size());

        rebuildDefinitonUsingActiveColumnsOnly();

        LOG(LogTypes::IMPORT_EXPORT, "Read file having " + QString::number(rowsCount_) +
            " rows in time " + QString::number(performanceTimer.elapsed() * 1.0 / 1000) +
            " seconds.");
    }

    return true;
}

const QString& DatasetDefinitionOds::getSheetName()
{
    return sheetNames_.constFirst();
}

bool DatasetDefinitionOds::loadSpecificData(QuaZip& /*zip*/)
{
    //Nothing specific for .ods.
    return true;
}
