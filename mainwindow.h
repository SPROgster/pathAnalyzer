#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QVector>

#include "morphology.h"

#define k 3
#define rho 0.01
const qint64 fps = 20;

struct RgbColor
{
    unsigned int R, G, B;
};

struct RgbColorF
{
    float Rf, Gf, Bf;
};

namespace Ui {
class MainWindow;
}

class Gaussian
{
private:
    float sigma[3][3];
    float inver[3][3];
    float sigma_k;
    QList<RgbColor> points;

    bool isNotFinalized;
    bool usingRealTime;
    bool usingFullMastrix;
    bool usingHsv;

public:
    float sigmamin;

    RgbColorF mu;
    float det;
    float detSqrt;
    float det3Rt;

    Gaussian(float _sirmamin = 5, bool fullMatrix = true, bool hsv = false);

    void finalize();
    void addItem(QRgb x_);
    //void addItemMix(QRgb x_);

    float p(uchar x);
    bool isBackground(QRgb x_);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void openImages();
    void clearImageList();
    void play();
    void recognize();
    void learn();

    void itemClicked(QListWidgetItem * item);

    void spinSigmaMinChanged(double newValue);

public:
    void playImages(QList<QImage*>& pixmap);
    void substractBackground();
    void substractBackground2();
    void applyMasks();

    QList<QImage *>* createBorders();
    void applyBorders();

private:
    Ui::MainWindow *ui;

    QVector<Gaussian*> backg;
    void fillBackg(int n);
    void clearBackg();

    QImage* maskGradient(QImage *origin);

    QList<QImage*>  imageList;
    QList<QImage*>  masks;
    QList<QImage*>  imagesWithMasks;

    int pixelCount;

    float sigmamin;

    void clearLists();
    void convertToGrayscale(QImage &image);
};

#endif // MAINWINDOW_H
