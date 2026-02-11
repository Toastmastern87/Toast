using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace Sandbox
{
    public class PlayButton : Entity
    {
        void OnCreate()
        {
        }

        void OnEvent()
        {

                Scene.LoadScene("Mars");
        }

        void OnUpdate(float ts)
        {

        }
    }
}
