#pragma once

#include "qcustomplot.h"

class QInteractiveAxisRect : public QCPAxisRect
{
public:
    explicit QInteractiveAxisRect(QCustomPlot *parentPlot, bool setupDefaultAxes=true) :
    QCPAxisRect(parentPlot, setupDefaultAxes)
    {};

protected:
    bool mRightDragging{false};

    virtual void mousePressEvent(QMouseEvent *event, const QVariant &details) Q_DECL_OVERRIDE
    {
        QCPAxisRect::mousePressEvent(event, details);
        if (event->buttons() & Qt::RightButton)
            mRightDragging = true;
    };

    virtual void mouseMoveEvent(QMouseEvent *event, const QPointF &startPos) Q_DECL_OVERRIDE
    {   
        QCPAxisRect::mouseMoveEvent(event, startPos);
        if(mRightDragging)
        {
            const QPointF pos = event->position();
            if (mRangeZoom != 0)
            {
                double factor;
                if (mRangeZoom.testFlag(Qt::Horizontal))
                {
                    double diff = (pos.x() - startPos.x())/600;
                    factor = qPow(mRangeZoomFactorHorz, diff);
                    foreach (QPointer<QCPAxis> axis, mRangeZoomHorzAxis)
                    {
                    if (!axis.isNull())
                        axis->scaleRange(factor, axis->pixelToCoord(startPos.x()));
                    }
                }
                if (mRangeZoom.testFlag(Qt::Vertical))
                {
                    double diff = (startPos.y() - pos.y())/600;
                    factor = qPow(mRangeZoomFactorVert, diff);
                    foreach (QPointer<QCPAxis> axis, mRangeZoomVertAxis)
                    {
                    if (!axis.isNull())
                        axis->scaleRange(factor, axis->pixelToCoord(startPos.y()));
                    }
                }
            }
        }
    };

    virtual void mouseReleaseEvent(QMouseEvent *event, const QPointF &startPos) Q_DECL_OVERRIDE
    {
        QCPAxisRect::mouseReleaseEvent(event, startPos);
        mRightDragging = false;
    }
};

class QInteractivePlot : public QCustomPlot
{
public:
    explicit QInteractivePlot(QWidget *parent = nullptr) :
    QCustomPlot(parent)
    {
        QCPAxisRect *defaultAxisRect = new QInteractiveAxisRect(this, true);
        mPlotLayout->remove(mPlotLayout->element(0,0));
        mPlotLayout->addElement(0, 0, defaultAxisRect);
        xAxis = defaultAxisRect->axis(QCPAxis::atBottom);
        yAxis = defaultAxisRect->axis(QCPAxis::atLeft);
        xAxis2 = defaultAxisRect->axis(QCPAxis::atTop);
        yAxis2 = defaultAxisRect->axis(QCPAxis::atRight);
        legend = new QCPLegend;
        legend->setVisible(false);
        defaultAxisRect->insetLayout()->addElement(legend, Qt::AlignRight|Qt::AlignTop);
        defaultAxisRect->insetLayout()->setMargins(QMargins(12, 12, 12, 12));

        defaultAxisRect->setLayer(QLatin1String("background"));
        xAxis->setLayer(QLatin1String("axes"));
        yAxis->setLayer(QLatin1String("axes"));
        xAxis2->setLayer(QLatin1String("axes"));
        yAxis2->setLayer(QLatin1String("axes"));
        xAxis->grid()->setLayer(QLatin1String("grid"));
        yAxis->grid()->setLayer(QLatin1String("grid"));
        xAxis2->grid()->setLayer(QLatin1String("grid"));
        yAxis2->grid()->setLayer(QLatin1String("grid"));
        legend->setLayer(QLatin1String("legend"));
        
        // create selection rect instance:
        mSelectionRect = new QCPSelectionRect(this);
        mSelectionRect->setLayer(QLatin1String("overlay"));
        
        setViewport(rect()); // needs to be called after mPlotLayout has been created
        replot(rpQueuedReplot);
    }

};

