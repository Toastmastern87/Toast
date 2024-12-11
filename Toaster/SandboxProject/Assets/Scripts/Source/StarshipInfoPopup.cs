using System;
using Toast;

namespace Sandbox
{
    public class StarshipInfoPopup : Entity
    { 

        private Entity mStarshipInfoPanel;

        void OnCreate()
        {
            mStarshipInfoPanel = FindChildEntityByName("Starship SN3", "InfoPopup");
        }

        void OnEvent()
        {
            mStarshipInfoPanel.GetComponent<UIPanelComponent>().Visible = true;
        }

        void OnUpdate(float ts)
        {
        }
    }
}