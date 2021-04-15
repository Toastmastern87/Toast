using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

class ClientHelloWorld
{
    void OnCreate()
    {
        Toast.Console.LogTrace("Testar Trace!");
        Toast.Console.LogInfo("Testar Info!");
        Toast.Console.LogWarning("Testar Warning!");
        Toast.Console.LogError("Testar Error!");
        Toast.Console.LogCritical("Testar Critical!");
    }

    void OnUpdate(float ts) 
    {
        Toast.Console.LogTrace("Update Entity timestep: " + ts);
    }
}
