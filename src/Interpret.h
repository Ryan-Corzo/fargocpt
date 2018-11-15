#ifndef INTERPRET_H
#define INTERPRET_H

#include "data.h"

//void var(char* name, void* ptr, int type, int necessary, char* deflt);
void ReadVariables(char* filename, t_data &data);
void PrintUsage(char* execname);
double TellNbOrbits(double time);
double TellNbOutputs(double time);
void TellEverything();

#endif // INTERPRET_H
