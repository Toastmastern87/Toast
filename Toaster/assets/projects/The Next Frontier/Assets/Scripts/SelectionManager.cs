using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace TheNextFrontier
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
