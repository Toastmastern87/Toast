using System;
using Toast;

namespace Sandbox
{
    public class CloseButton : Entity
    {
        private Entity mPanel;

        void OnCreate()
        {
            mPanel = FindEntityByName("InfoPopup");
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