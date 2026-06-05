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
                Entity hovered = Scene.GetHoveredEntity();
                if (hovered != null && hovered.IsSelectable())
                    hovered.SelectExclusive();
                else
                    Selection.Clear();
            }

            if (Input.IsMouseButtonPressed(MouseCode.ButtonRight))
            {
                Vector3 worldPos;
                if (Scene.GetWorldPositionUnderCursor(out worldPos))
                {
                    foreach (Entity e in Selection.GetSelected())
                        e.MoveTo(worldPos, 2.0f);
                } 
            }
        }

        void OnUpdate(float ts)
        {

        }
    }
}