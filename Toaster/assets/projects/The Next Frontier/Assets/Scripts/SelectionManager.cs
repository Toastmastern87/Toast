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

        bool OnEvent(Event e)
        {
            if (Input.IsMouseButtonPressed(MouseCode.ButtonLeft))
            {
                Entity hovered = Scene.GetHoveredEntity();
                if (hovered != null && hovered.IsSelectable())
                    hovered.SelectExclusive();
                else
                    Selection.Clear();

                return true;    
            }

            if (Input.IsMouseButtonPressed(MouseCode.ButtonRight))
            {
                Vector3 worldPos;
                if (Scene.GetWorldPositionUnderCursor(out worldPos))
                {
                    foreach (Entity entity in Selection.GetSelected())
                        entity.MoveTo(worldPos, 2.0f);
                } 

                return true;
            }

            return false;
        }

        void OnUpdate(float ts)
        {
        }
    }
}
