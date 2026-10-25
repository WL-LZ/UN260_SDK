#include "un260/workspace/calculator_math.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void){
 int64_t n;assert(calculator_parse("0.1",&n)&&n==100000);assert(!calculator_parse("1.0000001",&n));assert(!calculator_parse("--1",&n));
 assert(calculator_calculate(100000,'+',200000,&n)==CALCULATOR_OK&&n==300000);
 assert(calculator_calculate(1000000,'/',3000000,&n)==CALCULATOR_OK&&n==333333);
 assert(calculator_calculate(-1000000,'/',6000000,&n)==CALCULATOR_OK&&n==-166667);
 assert(calculator_calculate(CALCULATOR_LIMIT,'*',CALCULATOR_SCALE,&n)==CALCULATOR_OK&&n==CALCULATOR_LIMIT);
 assert(calculator_calculate(CALCULATOR_LIMIT,'*',2*CALCULATOR_SCALE,&n)==CALCULATOR_OVERFLOW);
 assert(calculator_calculate(1,'/',0,&n)==CALCULATOR_DIV_ZERO);
 assert(calculator_convert(1000000,7250000,"CNY",&n)==CALCULATOR_OK&&n==7250000);
 assert(calculator_convert(1000000,1234567,"JPY",&n)==CALCULATOR_OK&&n==1000000);
 puts("PASS calculator parsing, arithmetic, overflow, divide by zero and currency rounding");
}
