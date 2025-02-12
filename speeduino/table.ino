/*
Speeduino - Simple engine management for the Arduino Mega 2560 platform
Copyright (C) Josh Stewart
A full copy of the license may be found in the projects root directory
*/

/*
Because the size of the table is dynamic, this functino is required to reallocate the array sizes
Note that this may clear some of the existing values of the table
*/
#include "table.h"
#include "globals.h"

void table2D_setSize(struct table2D* targetTable, byte newSize)
{
  //Table resize is ONLY permitted during system initialisation.
  //if(initialisationComplete == false)
  {
    //2D tables can contain either bytes or ints, depending on the value of the valueSize field
    if(targetTable->valueSize == SIZE_BYTE)
    {
      //The following lines have MISRA suppressions as realloc is otherwise forbidden. These calls have been verified as unable to be executed from anywhere but controlled areas. 
      //cppcheck-suppress misra-21.3
      targetTable->values = (byte *)realloc(targetTable->values, newSize * sizeof(byte)); //cppcheck-suppress misra_21.3
      targetTable->axisX = (byte *)realloc(targetTable->axisX, newSize * sizeof(byte));
      targetTable->xSize = newSize;
    }
    else
    {
      targetTable->values16 = (int16_t *)realloc(targetTable->values16, newSize * sizeof(int16_t));
      targetTable->axisX16 = (int16_t *)realloc(targetTable->axisX16, newSize * sizeof(int16_t));
      targetTable->xSize = newSize;
    } //Byte or int
  } //initialisationComplete
}


void table3D_setSize(struct table3D *targetTable, byte newSize)
{
  if(initialisationComplete == false)
  {
    targetTable->values = (byte **)malloc(newSize * sizeof(byte*));
    for(byte i = 0; i < newSize; i++) { targetTable->values[i] = (byte *)malloc(newSize * sizeof(byte)); }

    targetTable->axisX = (int16_t *)malloc(newSize * sizeof(int16_t));
    targetTable->axisY = (int16_t *)malloc(newSize * sizeof(int16_t));
    targetTable->xSize = newSize;
    targetTable->ySize = newSize;
  } //initialisationComplete
}

/*
This function simply pulls a 1D linear interpolated (ie averaged) value from a 2D table
ie: Given a value on the X axis, it returns a Y value that coresponds to the point on the curve between the nearest two defined X values

This function must take into account whether a table contains 8-bit or 16-bit values.
Unfortunately this means many of the lines are duplicated depending on this
*/
int table2D_getValue(struct table2D *fromTable, int X_in)
{
    int returnValue = 0;
    bool valueFound = false;

    int X = X_in;
    int xMinValue, xMaxValue;
    int xMin = 0;
    int xMax = 0;

    //Check whether the X input is the same as last time this ran
    if( (X_in == fromTable->lastInput) && (fromTable->cacheTime == currentStatus.secl) )
    {
      returnValue = fromTable->lastOutput;
      valueFound = true;
    }
    else
    {
      fromTable->cacheTime = currentStatus.secl; //As we're not using the cache value, set the current secl value to track when this new value was calc'd

      if (fromTable->valueSize == SIZE_BYTE)
      {
        //Byte version
        xMinValue = fromTable->axisX[0];
        xMaxValue = fromTable->axisX[fromTable->xSize-1];
      }
      else
      {
        //int version
        xMinValue = fromTable->axisX16[0];
        xMaxValue = fromTable->axisX16[fromTable->xSize-1];
      }

      //If the requested X value is greater/small than the maximum/minimum bin, reset X to be that value
      if(X > xMaxValue) { X = xMaxValue; }
      if(X < xMinValue) { X = xMinValue; }


      if (fromTable->valueSize == SIZE_BYTE)
      {
         //**** Byte version ****

         //1st check is whether we're still in the same X bin as last time
         if ( (X <= fromTable->axisX[fromTable->lastXMax]) && (X > fromTable->axisX[fromTable->lastXMin]) )
         {
           xMaxValue = fromTable->axisX[fromTable->lastXMax];
           xMinValue = fromTable->axisX[fromTable->lastXMin];
           xMax = fromTable->lastXMax;
           xMin = fromTable->lastXMin;
         }
         else
         {
            //If we're not in the same bin, loop through to find where we are
            for (int x = fromTable->xSize-1; x >= 0; x--)
            {
                //Checks the case where the X value is exactly what was requested
                if ( (X == fromTable->axisX[x]) || (x == 0) )
                {
                  returnValue = fromTable->values[x]; //Simply return the coresponding value
                  valueFound = true;
                  break;
                }
                else
                {
                  //Normal case
                  if ( (X <= fromTable->axisX[x]) && (X > fromTable->axisX[x-1]) )
                  {
                    xMaxValue = fromTable->axisX[x];
                    xMinValue = fromTable->axisX[x-1];
                    xMax = x;
                    fromTable->lastXMax = xMax;
                    xMin = x-1;
                    fromTable->lastXMin = xMin;
                    break;
                  }
                }
            }
         }
      }
      else
      {
        // **** INT VERSION ****

         //1st check is whether we're still in the same X bin as last time
         if ( (X <= fromTable->axisX16[fromTable->lastXMax]) && (X > fromTable->axisX16[fromTable->lastXMin]) )
         {
           xMaxValue = fromTable->axisX16[fromTable->lastXMax];
           xMinValue = fromTable->axisX16[fromTable->lastXMin];
           xMax = fromTable->lastXMax;
           xMin = fromTable->lastXMin;
         }
         else
         {
            //If we're not in the same bin, loop through to find where we are
            for (int x = fromTable->xSize-1; x >= 0; x--)
            {
                //Checks the case where the X value is exactly what was requested
                if ( (X == fromTable->axisX16[x]) || (x == 0) )
                {
                  returnValue = fromTable->values16[x]; //Simply return the coresponding value
                  valueFound = true;
                  break;
                }
                else
                {
                  //Normal case
                  if ( (X <= fromTable->axisX16[x]) && (X > fromTable->axisX16[x-1]) )
                  {
                    xMaxValue = fromTable->axisX16[x];
                    xMinValue = fromTable->axisX16[x-1];
                    xMax = x;
                    fromTable->lastXMax = xMax;
                    xMin = x-1;
                    fromTable->lastXMin = xMin;
                    break;
                  } //Found in loop
                } //Exact hit
            } //For loop
         } //In cache or not
      } //byte or int values
    } //X_in same as last time

    if (valueFound == false)
    {
      unsigned int m = X - xMinValue;
      unsigned int n = xMaxValue - xMinValue;

      //Float version
      /*
      int yVal = (m / n) * (abs(fromTable.values[xMax] - fromTable.values[xMin]));
      */

      //Non-Float version
      uint16_t yVal;
      if (fromTable->valueSize == SIZE_BYTE)
      {
         //Byte version
         yVal = ((long)(m << 6) / n) * (abs(fromTable->values[xMax] - fromTable->values[xMin]));
         yVal = (yVal >> 6);

         if (fromTable->values[xMax] > fromTable->values[xMin]) { yVal = fromTable->values[xMin] + yVal; }
         else { yVal = fromTable->values[xMin] - yVal; }
      }
      else
      {
         //int version
         yVal = ((long)(m << 6) / n) * (abs(fromTable->values16[xMax] - fromTable->values16[xMin]));
         yVal = (yVal >> 6);

         if (fromTable->values[xMax] > fromTable->values16[xMin]) { yVal = fromTable->values16[xMin] + yVal; }
         else { yVal = fromTable->values16[xMin] - yVal; }
      }
      returnValue = yVal;
    }

    fromTable->lastInput = X_in;
    fromTable->lastOutput = returnValue;

    return returnValue;
}


//This function pulls a value from a 3D table given a target for X and Y coordinates.
//It performs a 2D linear interpolation as descibred in: www.megamanual.com/v22manual/ve_tuner.pdf
int get3DTableValue(struct table3D *fromTable, int Y_in, int X_in)
  {
    int X = X_in;
    int Y = Y_in;

    int tableResult = 0;
    //Loop through the X axis bins for the min/max pair
    //Note: For the X axis specifically, rather than looping from tableAxisX[0] up to tableAxisX[max], we start at tableAxisX[Max] and go down.
    //      This is because the important tables (fuel and injection) will have the highest RPM at the top of the X axis, so starting there will mean the best case occurs when the RPM is highest (And hence the CPU is needed most)
    int xMinValue = fromTable->axisX[0];
    int xMaxValue = fromTable->axisX[fromTable->xSize-1];
    byte xMin = 0;
    byte xMax = 0;

    //If the requested X value is greater/small than the maximum/minimum bin, reset X to be that value
    if(X > xMaxValue) { X = xMaxValue; }
    if(X < xMinValue) { X = xMinValue; }

    //1st check is whether we're still in the same X bin as last time
    if ( (X <= fromTable->axisX[fromTable->lastXMax]) && (X > fromTable->axisX[fromTable->lastXMin]) )
    {
      xMaxValue = fromTable->axisX[fromTable->lastXMax];
      xMinValue = fromTable->axisX[fromTable->lastXMin];
      xMax = fromTable->lastXMax;
      xMin = fromTable->lastXMin;
    }
    //2nd check is whether we're in the next RPM bin (To the right)
    else if ( ((fromTable->lastXMax + 1) < fromTable->xSize ) && (X <= fromTable->axisX[fromTable->lastXMax +1 ]) && (X > fromTable->axisX[fromTable->lastXMin + 1]) ) //First make sure we're not already at the last X bin
    {
      xMax = fromTable->lastXMax + 1;
      fromTable->lastXMax = xMax;
      xMin = fromTable->lastXMin + 1;
      fromTable->lastXMin = xMin;
      xMaxValue = fromTable->axisX[fromTable->lastXMax];
      xMinValue = fromTable->axisX[fromTable->lastXMin];
    }
    //3rd check is to look at the previous bin (to the left)
    else if ( (fromTable->lastXMin > 0 ) && (X <= fromTable->axisX[fromTable->lastXMax - 1]) && (X > fromTable->axisX[fromTable->lastXMin - 1]) ) //First make sure we're not already at the first X bin
    {
      xMax = fromTable->lastXMax - 1;
      fromTable->lastXMax = xMax;
      xMin = fromTable->lastXMin - 1;
      fromTable->lastXMin = xMin;
      xMaxValue = fromTable->axisX[fromTable->lastXMax];
      xMinValue = fromTable->axisX[fromTable->lastXMin];
    }
    else
    //If it's not caught by one of the above scenarios, give up and just run the loop
    {
      for (int8_t x = fromTable->xSize-1; x >= 0; x--)
      {
        //Checks the case where the X value is exactly what was requested
        if ( (X == fromTable->axisX[x]) || (x == 0) )
        {
          xMaxValue = fromTable->axisX[x];
          xMinValue = fromTable->axisX[x];
          xMax = x;
          fromTable->lastXMax = xMax;
          xMin = x;
          fromTable->lastXMin = xMin;
          break;
        }
        //Normal case
        if ( (X <= fromTable->axisX[x]) && (X > fromTable->axisX[x-1]) )
        {
          xMaxValue = fromTable->axisX[x];
          xMinValue = fromTable->axisX[x-1];
          xMax = x;
          fromTable->lastXMax = xMax;
          xMin = x-1;
          fromTable->lastXMin = xMin;
          break;
        }
      }
    }

    //Loop through the Y axis bins for the min/max pair
    int yMaxValue = fromTable->axisY[0];
    int yMinValue = fromTable->axisY[fromTable->ySize-1];
    byte yMin = 0;
    byte yMax = 0;

    //If the requested Y value is greater/small than the maximum/minimum bin, reset Y to be that value
    if(Y > yMaxValue) { Y = yMaxValue; }
    if(Y < yMinValue) { Y = yMinValue; }

    //1st check is whether we're still in the same Y bin as last time
    if ( (Y >= fromTable->axisY[fromTable->lastYMax]) && (Y < fromTable->axisY[fromTable->lastYMin]) )
    {
      yMaxValue = fromTable->axisY[fromTable->lastYMax];
      yMinValue = fromTable->axisY[fromTable->lastYMin];
      yMax = fromTable->lastYMax;
      yMin = fromTable->lastYMin;
    }
    //2nd check is whether we're in the next MAP/TPS bin (Next one up)
    else if ( (fromTable->lastYMin > 0 ) && (Y <= fromTable->axisY[fromTable->lastYMin - 1 ]) && (Y > fromTable->axisY[fromTable->lastYMax - 1]) ) //First make sure we're not already at the top Y bin
    {
      yMax = fromTable->lastYMax - 1;
      fromTable->lastYMax = yMax;
      yMin = fromTable->lastYMin - 1;
      fromTable->lastYMin = yMin;
      yMaxValue = fromTable->axisY[fromTable->lastYMax];
      yMinValue = fromTable->axisY[fromTable->lastYMin];
    }
    //3rd check is to look at the previous bin (Next one down)
    else if ( ((fromTable->lastYMax + 1) < fromTable->ySize) && (Y <= fromTable->axisY[fromTable->lastYMin + 1]) && (Y > fromTable->axisY[fromTable->lastYMax + 1]) ) //First make sure we're not already at the bottom Y bin
    {
      yMax = fromTable->lastYMax + 1;
      fromTable->lastYMax = yMax;
      yMin = fromTable->lastYMin + 1;
      fromTable->lastYMin = yMin;
      yMaxValue = fromTable->axisY[fromTable->lastYMax];
      yMinValue = fromTable->axisY[fromTable->lastYMin];
    }
    else
    //If it's not caught by one of the above scenarios, give up and just run the loop
    {

      for (int8_t y = fromTable->ySize-1; y >= 0; y--)
      {
        //Checks the case where the Y value is exactly what was requested
        if ( (Y == fromTable->axisY[y]) || (y==0) )
        {
          yMaxValue = fromTable->axisY[y];
          yMinValue = fromTable->axisY[y];
          yMax = y;
          fromTable->lastYMax = yMax;
          yMin = y;
          fromTable->lastYMin = yMin;
          break;
        }
        //Normal case
        if ( (Y >= fromTable->axisY[y]) && (Y < fromTable->axisY[y-1]) )
        {
          yMaxValue = fromTable->axisY[y];
          yMinValue = fromTable->axisY[y-1];
          yMax = y;
          fromTable->lastYMax = yMax;
          yMin = y-1;
          fromTable->lastYMin = yMin;
          break;
        }
      }
    }


    /*
    At this point we have the 4 corners of the map where the interpolated value will fall in
    Eg: (yMin,xMin)  (yMin,xMax)

        (yMax,xMin)  (yMax,xMax)

    In the following calculation the table values are referred to by the following variables:
              A          B

              C          D

    */
    int A = fromTable->values[yMin][xMin];
    int B = fromTable->values[yMin][xMax];
    int C = fromTable->values[yMax][xMin];
    int D = fromTable->values[yMax][xMax];

    //Check that all values aren't just the same (This regularly happens with things like the fuel trim maps)
    if( (A == B) && (A == C) && (A == D) ) { tableResult = A; }
    else
    {
      //Create some normalised position values
      //These are essentially percentages (between 0 and 1) of where the desired value falls between the nearest bins on each axis


      //Initial check incase the values were hit straight on

      unsigned long p = (long)X - xMinValue;
      if (xMaxValue == xMinValue) { p = (p << 8); }  //This only occurs if the requested X value was equal to one of the X axis bins
      else { p = ( (p << 8) / (xMaxValue - xMinValue) ); } //This is the standard case

      unsigned long q;
      if (yMaxValue == yMinValue)
      {
        q = (long)Y - yMinValue;
        q = (q << 8);
      }
      //Standard case
      else
      {
        q = long(Y) - yMaxValue;
        q = 256 - ( (q << 8) / (yMinValue - yMaxValue) );
      }

/*
      long q;
      if (yMaxValue == yMinValue) { q = ((long)(Y - yMinValue) << 8); }
      else { q = 256 - (((long)(Y - yMaxValue) << 8) / (yMinValue - yMaxValue)); }
      */

      int m = ((256-p) * (256-q)) >> 8;
      int n = (p * (256-q)) >> 8;
      int o = ((256-p) * q) >> 8;
      int r = (p * q) >> 8;
      tableResult = ( (A * m) + (B * n) + (C * o) + (D * r) ) >> 8;
    }
    return tableResult;
}
