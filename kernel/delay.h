#pragma once

unsigned int udelay(unsigned long usecs)
{ 
  unsigned int tmp = 0;
  for(int i = 0;i<255; i++)
    tmp++;
  return tmp;
}