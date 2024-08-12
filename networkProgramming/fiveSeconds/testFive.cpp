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
        cout << "我现在要发送数据" << endl;
        if(++num >= 3)
        {
            allowed = false;
            num = 0;
        }
    }

    if(!allowed)
    {
        cout << "现在我才不发呢" << endl;
    }

}





int main()
{

    return 0;
}