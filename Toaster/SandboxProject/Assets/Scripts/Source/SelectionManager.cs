using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Toast;

namespace Sandbox
{
    public class SelectionManager : Entity
    {
        void OnCreate()
        {
        }

        void OnEvent()
        {
            if (Input.IsMouseButtonPressed(MouseCode.ButtonLeft))
            {
                Selection.Clear();
            }

            if (Input.IsMouseButtonPressed(MouseCode.ButtonRight))
            {
                Vector3 worldPos;
                if (Scene.GetWorldPositionFromScreenPos(out worldPos))
                {
                    Toast.Console.LogTrace("Right-click world position: " + worldPos);
                }
                else
                {
                    Toast.Console.LogTrace("Right-click on sky / no geometry");
                }
            }
        }

        void OnUpdate(float ts)
        {

        }
    }
}