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
        }

        void OnUpdate(float ts)
        {

        }
    }
}