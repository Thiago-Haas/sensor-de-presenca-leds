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
    c.start(0); assert(c.sample(100,true,500,50,height)==FloorCalibration::Pending);
    for(int i=0;i<14;i++) assert(c.sample(220+i%3*0.2f,true,1000+i*70,50,height)==FloorCalibration::Pending);
    assert(c.sample(220,true,1980,50,height)==FloorCalibration::Ready);
    assert(height>=220 && height<=220.4f && !c.active());
    // Uma segunda calibracao substitui a primeira sem reiniciar a placa.
    c.start(3000);
    for(int i=0;i<14;i++) assert(c.sample(215,true,4000+i*70,50,height)==FloorCalibration::Pending);
    assert(c.sample(215,true,4980,50,height)==FloorCalibration::Ready);
    assert(height==215);
    c.start(0);
    for(int i=0;i<14;i++) c.sample(220,true,1000+i*70,50,height);
    assert(c.sample(225,true,1980,50,height)==FloorCalibration::Unstable);
    assert(height<221);
    c.start(0); c.sample(0,false,1000,50,height);
    assert(c.sample(0,false,5000,50,height)==FloorCalibration::NoEcho);
    c.start(UINT32_MAX-500);
    assert(c.sample(216,true,600,50,height)==FloorCalibration::Pending);
    assert(c.sample(216,true,4500,50,height)==FloorCalibration::NoEcho);
    c.start(0);
    for(int i=0;i<14;i++) c.sample(216,true,1000+i*70,50,height);
    c.sample(std::numeric_limits<float>::quiet_NaN(),true,2000,50,height);
    assert(c.sample(216,true,2070,50,height)==FloorCalibration::Pending);
}
