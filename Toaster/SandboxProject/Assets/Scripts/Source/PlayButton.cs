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
            if (Input.IsMouseButtonReleased(MouseCode.ButtonLeft))
                Scene.LoadScene("Mars");
        }

        void OnUpdate(float ts)
        {

        }
    }
}
