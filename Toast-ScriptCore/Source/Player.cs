using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Toast;

namespace Sandbox
{
    public class Player : Entity
    {
        void OnCreate() 
        {
            System.Console.WriteLine($"Player.OnCreated - {ID}");
        }

        void OnUpdate(float ts)
        {
            System.Console.WriteLine($"Player.OnUpdate: {ts}");
        }
    }
}
