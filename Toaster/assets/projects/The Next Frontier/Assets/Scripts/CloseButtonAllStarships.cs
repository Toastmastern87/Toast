using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace TheNextFrontier
{
    public class CloseButtonAllStarships : Entity
    {
        private Entity mPanel;

        void OnCreate()
        {
            mPanel = FindEntityByName("AllStarshipsPopup");
        }

        bool OnEvent(Event e)
        {
            mPanel.GetComponent<UIPanelComponent>().Visible = false;

            return false;
        }

        void OnUpdate(float ts)
        {
        }
    }
}
