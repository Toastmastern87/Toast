using System;
using Toast;

namespace Sandbox
{
    public class CloseButtonAllStarships : Entity
    {
        private Entity mPanel;

        void OnCreate()
        {
            mPanel = FindEntityByName("AllStarshipsPopup");
        }

        void OnEvent()
        {
            mPanel.GetComponent<UIPanelComponent>().Visible = false;
        }

        void OnUpdate(float ts)
        {
        }
    }
}