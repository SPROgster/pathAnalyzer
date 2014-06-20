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
    QVector<RgbColor> points;

    bool isNotFinalized;
    // http://ru.wikipedia.org/wiki/%D0%9D%D0%BE%D1%80%D0%BC%D0%B0%D0%BB%D1%8C%D0%BD%D0%BE%D0%B5_%D1%80%D0%B0%D1%81%D0%BF%D1%80%D0%B5%D0%B4%D0%B5%D0%BB%D0%B5%D0%BD%D0%B8%D0%B5
    // http://www.machinelearning.ru/wiki/index.php?title=%D0%9F%D1%80%D0%B0%D0%BA%D1%82%D0%B8%D0%BA%D1%83%D0%BC_%D0%BD%D0%B0_%D0%AD%D0%92%D0%9C_%28317%29/2013-2014/BackgroundSubtraction

public:
    float sigmamin;

    RgbColorF mu;
    float det;
    float detSqrt;

    Gaussian(float _sirmamin = 5);

    void finalize();
    void addItem(QRgb x_);

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
