#include <iostream>
#include <vector>
#include <chrono>
#include "time.h"
#include <atomic>
using namespace std;

atomic_bool allowed (false);
atomic_bool alreadySend(false);
atomic_int  num(0);

void printFiveSeconds()
{
    
    if(allowed)
    {
        cout << "������Ҫ��������" << endl;
        if(++num >= 3)
        {
            allowed = false;
            num = 0;
        }
    }

    if(!allowed)
    {
        cout << "�����ҲŲ�����" << endl;
    }

}





int main()
{

    return 0;
}