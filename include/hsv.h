#ifndef __HSV_H__
#define __HSV_H__

class HSV
{
public:

  HSV() :
    hue(0.0f), sat(0.0f), val(0.0f) {}

  HSV(float hue, float sat, float val) :
    hue(hue), sat(sat), val(val) {}

  public:
    float hue;
    float sat;
    float val;
};

#endif // __HSV_H__
