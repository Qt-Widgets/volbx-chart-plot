﻿#ifndef GROUPPLOT_H
#define GROUPPLOT_H

#include <QEvent>

#include <qwt_scale_div.h>
#include <qwt_plot_curve.h>

#include "Common/Quantiles.h"

#include "CustomMarker.h"
#include "GroupPicker.h"
#include "PlotBase.h"
#include "Picker.h"
#include "NotchedMarker.h"

class QwtPlotCurve;

/**
 * @brief Plot on which user can group by text columns.
 */
class GroupPlot : public PlotBase
{
    Q_OBJECT
public:
    explicit GroupPlot(QWidget* parent = nullptr);

    ~GroupPlot() override = default;

    GroupPlot& operator=(const GroupPlot& other) = delete;
    GroupPlot(const GroupPlot& other) = delete;

    GroupPlot& operator=(GroupPlot&& other) = delete;
    GroupPlot(GroupPlot&& other) = delete;

    void setNewData(const QVector<Quantiles>& quantiles,
                    QVector<QString>& intervalStrings);

    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent* event) override;

private:
    void shortenIntervalsNamesIfNeeded(QVector<QString>& intervalsNames,
                                       const QVector<Quantiles>& quantilesForIntervals);

    ///Maximum number of chars in label.
    const static int maxCharsInLabel_;

    NotchedMarker marker_;

    //Quantiles.
    QVector<Quantiles> quantiles_;

    //Names used on axis.
    QVector<QString> shortIntervalNames_;

    ///Names used in tooltip.
    QVector<QString> longIntervalNames_;

    GroupPicker picker_;
};
#endif // GROUPPLOT_H
