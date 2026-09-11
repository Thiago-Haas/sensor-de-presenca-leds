#include "calibration.h"
#include <cassert>
#include <limits>
int main() {
    CalibrationButton b;
    assert(!b.pressed(true, 10)); assert(!b.pressed(false, 20));
    assert(!b.pressed(true, 25)); assert(!b.pressed(true, 74));
    assert(b.pressed(true, 75)); assert(!b.pressed(true, 1000));
    assert(!b.pressed(false, 1100)); assert(!b.pressed(false, 1150));
    assert(!b.pressed(true, 1200)); assert(b.pressed(true,1250));

    FloorCalibration c; float height=216;
    // Calibracao normal: a coleta comeca no instante do aperto do botao (sem espera de 1s);
    // a altura calibrada corresponde a esse momento.
    c.start(0);
    for(int i=0;i<14;i++) assert(c.sample(220+i%3*0.2f,true,i*70,50,height)==FloorCalibration::Pending);
    assert(c.sample(220,true,980,50,height)==FloorCalibration::Ready);
    assert(height>=220 && height<=220.4f && !c.active());

    // Uma segunda calibracao substitui a primeira sem reiniciar a placa.
    c.start(3000);
    for(int i=0;i<14;i++) assert(c.sample(215,true,3000+i*70,50,height)==FloorCalibration::Pending);
    assert(c.sample(215,true,3980,50,height)==FloorCalibration::Ready);
    assert(height==215);

    // Leituras instaveis (variacao > 3 cm) sao rejeitadas.
    c.start(0);
    for(int i=0;i<14;i++) c.sample(220,true,i*70,50,height);
    assert(c.sample(225,true,980,50,height)==FloorCalibration::Unstable);
    assert(height<221);

    // Sem eco valido durante toda a janela de 5 s.
    c.start(0); c.sample(0,false,1000,50,height);
    assert(c.sample(0,false,5000,50,height)==FloorCalibration::NoEcho);

    // Overflow de millis(): a janela de 5 s continua funcionando apos o wraparound.
    c.start(UINT32_MAX-500);
    assert(c.sample(216,true,600,50,height)==FloorCalibration::Pending);
    assert(c.sample(216,true,4500,50,height)==FloorCalibration::NoEcho);

    // Uma leitura invalida (NaN) no meio da coleta reinicia a contagem.
    c.start(0);
    for(int i=0;i<14;i++) c.sample(216,true,i*70,50,height);
    c.sample(std::numeric_limits<float>::quiet_NaN(),true,1000,50,height);
    assert(c.sample(216,true,1070,50,height)==FloorCalibration::Pending);
}
